/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "updater_main.h"
#include <chrono>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <string>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "applypatch/partition_record.h"
#include "fs_manager/mount.h"
#include "include/updater/updater.h"
#include "log/dump.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "package/pkg_manager.h"
#include "pkg_manager.h"
#include "pkg_utils.h"
#include "securec.h"
#include "ui/frame.h"
#include "ui/text_label.h"
#include "ui/updater_ui.h"
#include "updater/updater_const.h"
#include "utils.h"

namespace Updater {
using Utils::String2Int;
using namespace Hpackage;
using namespace Updater::Utils;

extern TextLabel *g_logLabel;
extern TextLabel *g_logResultLabel;

constexpr int DISPLAY_TIME = 1000 * 1000;
constexpr struct option OPTIONS[] = {
    { "update_package", required_argument, nullptr, 0 },
    { "retry_count", required_argument, nullptr, 0 },
    { "factory_wipe_data", no_argument, nullptr, 0 },
    { "user_wipe_data", no_argument, nullptr, 0 },
    { nullptr, 0, nullptr, 0 },
};

static void SetRetryCountToMisc(int retryCount, const std::vector<std::string> args)
{
    struct UpdateMessage msg {};
    char buffer[20];
    UPDATER_ERROR_CHECK(!strncpy_s(msg.command, sizeof(msg.command), "boot_updater", strlen("boot_updater") + 1),
        "SetRetryCountToMisc strncpy_s failed", return);
    for (const auto& arg : args) {
        if (arg.find("--retry_count") == std::string::npos) {
            UPDATER_ERROR_CHECK(!strncat_s(msg.update, sizeof(msg.update), arg.c_str(), strlen(arg.c_str()) + 1),
                "SetRetryCountToMisc strncat_s failed", return);
            UPDATER_ERROR_CHECK(!strncat_s(msg.update, sizeof(msg.update), "\n", strlen("\n") + 1),
                "SetRetryCountToMisc strncat_s failed", return);
        }
    }
    UPDATER_ERROR_CHECK(snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--retry_count=%d", retryCount) != -1,
        "SetRetryCountToMisc snprintf_s failed", return);
    UPDATER_ERROR_CHECK(!strncat_s(msg.update, sizeof(msg.update), buffer, strlen(buffer) + 1),
        "SetRetryCountToMisc strncat_s failed", return);
    UPDATER_ERROR_CHECK_NOT_RETURN(WriteUpdaterMiscMsg(msg) == true, "Write command to misc failed.");
}

static int DoFactoryReset(FactoryResetMode mode, const std::string &path)
{
    if (mode == USER_WIPE_DATA) {
        STAGE(UPDATE_STAGE_BEGIN) << "User FactoryReset";
        LOG(INFO) << "Begin erasing /data";
        if (FormatPartition(path, true) != 0) {
            LOG(ERROR) << "User level FactoryReset failed";
            STAGE(UPDATE_STAGE_FAIL) << "User FactoryReset";
            ERROR_CODE(CODE_FACTORY_RESET_FAIL);
            return 1;
        }
        LOG(INFO) << "User level FactoryReset success";
        STAGE(UPDATE_STAGE_SUCCESS) << "User FactoryReset";
    }
    return 0;
}

int FactoryReset(FactoryResetMode mode, const std::string &path)
{
    return DoFactoryReset(mode, path);
}

UpdaterStatus UpdaterFromSdcard()
{
#ifndef UPDATER_UT
    // sdcard fsType only support ext4/vfat
    std::string sdcardStr = GetBlockDeviceByMountPoint(SDCARD_PATH);
    if (!IsSDCardExist(sdcardStr)) {
        if (errno == ENOENT) {
            ShowText(g_logLabel, "Cannot detect SdCard!");
        } else {
            ShowText(g_logLabel, "Detecting SdCard abnormally!");
        }
        return UPDATE_ERROR;
    }
    if (MountForPath(SDCARD_PATH) != 0) {
        int ret = mount(sdcardStr.c_str(), SDCARD_PATH, "vfat", 0, NULL);
        UPDATER_WARING_CHECK(ret == 0, "MountForPath /sdcard failed!", return UPDATE_ERROR);
    }
#endif
    UPDATER_ERROR_CHECK(access(SDCARD_CARD_PKG_PATH, 0) == 0, "package is not exist",
        ShowText(g_logLabel, "Package is not exist!");
        return UPDATE_ERROR);
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "pkgManager is nullptr", return UPDATE_ERROR);

    STAGE(UPDATE_STAGE_BEGIN) << "UpdaterFromSdcard";
    LOG(INFO) << "UpdaterFromSdcard start, sdcard updaterPath : " << SDCARD_CARD_PKG_PATH;

    g_logLabel->SetText("Don't remove SD Card!");
    Utils::UsSleep(DISPLAY_TIME);
    UpdaterStatus updateRet = DoInstallUpdaterPackage(pkgManager, SDCARD_CARD_PKG_PATH, 0, SDCARD_UPDATE);
    if (updateRet != UPDATE_SUCCESS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
        g_logLabel->SetText("SD Card update failed!");
        STAGE(UPDATE_STAGE_FAIL) << "UpdaterFromSdcard failed";
    } else {
        LOG(INFO) << "Update from SD Card successfully!";
        STAGE(UPDATE_STAGE_SUCCESS) << "UpdaterFromSdcard success";
    }
    PkgManager::ReleasePackageInstance(pkgManager);
    return updateRet;
}

bool GetBatteryCapacity(int &capacity)
{
    const static std::vector<const char *> vec = {
        "/sys/class/power_supply/battery/capacity",
        "/sys/class/power_supply/Battery/capacity"
    };
    for (auto &it : vec) {
        std::ifstream ifs { it };
        if (!ifs.is_open()) {
            continue;
        }

        int tmpCapacity = 0;
        ifs >> tmpCapacity;
        if ((ifs.fail()) || (ifs.bad())) {
            continue;
        }

        capacity = tmpCapacity;
        return true;
    }

    return false;
}

bool IsBatteryCapacitySufficient()
{
    static constexpr auto levelIdx = "lowBatteryLevel";
    static constexpr auto jsonPath = "/etc/product_cfg.json";

    int capacity = 0;
    bool ret = GetBatteryCapacity(capacity);
    if (!ret) {
        return true; /* maybe no battery or err value return default true */
    }

    JsonNode node { Fs::path { jsonPath }};
    auto item = node[levelIdx].As<int>();
    if (!item.has_value()) {
        return true; /* maybe no value return default true */
    }

    int lowLevel = *item;
    if (lowLevel > 100 || lowLevel < 0) { /* full percent is 100 */
        LOG(ERROR) << "load battery level error:" << lowLevel;
        return false; /* config err not allow to update */
    }

    LOG(INFO) << "current capacity:" << capacity << ", low level:" << lowLevel;

    return capacity > lowLevel;
}

static UpdaterStatus InstallUpdaterPackage(UpdaterParams &upParams, const std::vector<std::string> &args,
    PkgManager::PkgManagerPtr manager)
{
    UPDATER_INIT_RECORD;
    UpdaterStatus status = UPDATE_UNKNOWN;
    if (IsBatteryCapacitySufficient() == false) {
        g_logLabel->SetText("Battery is low.\n");
        UPDATER_LAST_WORD(UPDATE_ERROR);
        LOG(ERROR) << "Battery is not sufficient for install package.";
        status = UPDATE_SKIP;
    } else {
        STAGE(UPDATE_STAGE_BEGIN) << "Install package";
        if (upParams.retryCount == 0) {
            // First time enter updater, record retryCount in case of abnormal reset.
            UPDATER_ERROR_CHECK(PartitionRecord::GetInstance().ClearRecordPartitionOffset() == true,
                "ClearRecordPartitionOffset failed", return UPDATE_ERROR);
            SetRetryCountToMisc(upParams.retryCount + 1, args);
        }
        UPDATER_CHECK_ONLY_RETURN(SetupPartitions() == 0, ShowText(GetUpdateInfoLabel(), "Setup partitions failed");
            return UPDATE_ERROR);
        status = DoInstallUpdaterPackage(manager, upParams.updatePackage, upParams.retryCount, HOTA_UPDATE);
        if (status != UPDATE_SUCCESS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
            std::string errMsg = ((status == UPDATE_SPACE_NOTENOUGH) ? "Free space is not enough" : "Update failed!");
            g_logLabel->SetText(errMsg.c_str());
            STAGE(UPDATE_STAGE_FAIL) << "Install failed";
            if (status == UPDATE_RETRY && upParams.retryCount < MAX_RETRY_COUNT) {
                upParams.retryCount += 1;
                g_logLabel->SetText("Retry installation");
                SetRetryCountToMisc(upParams.retryCount, args);
                Utils::DoReboot("updater");
            }
        } else {
            LOG(INFO) << "Install package success.";
            STAGE(UPDATE_STAGE_SUCCESS) << "Install package";
        }
    }
    return status;
}

static UpdaterStatus StartUpdaterEntry(PkgManager::PkgManagerPtr manager,
    const std::vector<std::string> &args, UpdaterParams &upParams)
{
    UpdaterStatus status = UPDATE_UNKNOWN;
    if (upParams.updatePackage != "") {
        ShowUpdateFrame(true);
        status = InstallUpdaterPackage(upParams, args, manager);
        if (status != UPDATE_SUCCESS) {
            if (!CheckDumpResult()) {
                UPDATER_LAST_WORD(status);
            }
            return status;
        }
        WriteDumpResult("pass");
    } else if (upParams.factoryWipeData) {
        LOG(INFO) << "Factory level FactoryReset begin";
        status = UPDATE_SUCCESS;
        SetUpdateFlag(1);
        ShowUpdateFrame(true);
        DoProgress();
        UPDATER_ERROR_CHECK(FactoryReset(FACTORY_WIPE_DATA, "/data") == 0, "FactoryReset factory level failed",
            status = UPDATE_ERROR);

        ShowUpdateFrame(false);
        if (status != UPDATE_SUCCESS) {
            g_logResultLabel->SetText("Factory reset failed");
        } else {
            g_logResultLabel->SetText("Factory reset done");
        }
    } else if (upParams.userWipeData) {
        LOG(INFO) << "User level FactoryReset begin";
        status = UPDATE_SUCCESS;
        SetUpdateFlag(1);
        ShowUpdateFrame(true);
        DoProgress();
        UPDATER_ERROR_CHECK(FactoryReset(USER_WIPE_DATA, "/data") == 0, "FactoryReset user level failed",
            status = UPDATE_ERROR);
        ShowUpdateFrame(false);
        if (status != UPDATE_SUCCESS) {
            g_logResultLabel->SetText("Wipe data failed");
        } else {
            g_logResultLabel->SetText("Wipe data finished");
            PostUpdater(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
        }
    }
    return status;
}

static UpdaterStatus StartUpdater(PkgManager::PkgManagerPtr manager, const std::vector<std::string> &args,
    char **argv, PackageUpdateMode &mode)
{
    UpdaterParams upParams {
        false, false, 0, ""
    };
    std::vector<char *> extractedArgs;
    int rc;
    int optionIndex;

    for (const auto &arg : args) {
        extractedArgs.push_back(const_cast<char *>(arg.c_str()));
    }
    extractedArgs.push_back(nullptr);
    extractedArgs.insert(extractedArgs.begin(), argv[0]);
    while ((rc = getopt_long(extractedArgs.size() - 1, extractedArgs.data(), "", OPTIONS, &optionIndex)) != -1) {
        switch (rc) {
            case 0: {
                std::string option = OPTIONS[optionIndex].name;
                if (option == "update_package") {
                    upParams.updatePackage = optarg;
                    mode = HOTA_UPDATE;
                } else if (option == "retry_count") {
                    upParams.retryCount = atoi(optarg);
                    mode = HOTA_UPDATE;
                } else if (option == "factory_wipe_data") {
                    upParams.factoryWipeData = true;
                } else if (option == "user_wipe_data") {
                    upParams.userWipeData = true;
                }
                break;
            }
            case '?':
                LOG(ERROR) << "Invalid argument.";
                break;
            default:
                LOG(ERROR) << "Invalid argument.";
                break;
        }
    }
    optind = 1;
    // Sanity checks
    UPDATER_WARING_CHECK((upParams.factoryWipeData && upParams.userWipeData) == false,
        "Factory level reset and user level reset both set. use user level reset.", upParams.factoryWipeData = false);

    return StartUpdaterEntry(manager, args, upParams);
}

int UpdaterMain(int argc, char **argv)
{
    UpdaterStatus status = UPDATE_UNKNOWN;
    PkgManager::PkgManagerPtr manager = PkgManager::GetPackageInstance();
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_PRE_INIT_EVENT);
    Dump::GetInstance().RegisterDump("DumpHelperLog", std::make_unique<DumpHelperLog>());
    std::vector<std::string> args = ParseParams(argc, argv);

    LOG(INFO) << "Ready to start";
#ifndef UPDATER_UT
    UpdaterUiInit();
#endif
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_INIT_EVENT);
    PackageUpdateMode mode = UNKNOWN_UPDATE;
    status = StartUpdater(manager, args, argv, mode);
    std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
#ifndef UPDATER_UT
    if (status != UPDATE_SUCCESS && status != UPDATE_SKIP) {
        ShowUpdateFrame(false);
        // Wait for user input
        while (true) {
            pause();
        }
        return 0;
    }
#endif
    PostUpdater(true);
    Utils::DoReboot("");
    return 0;
}
} // Updater
