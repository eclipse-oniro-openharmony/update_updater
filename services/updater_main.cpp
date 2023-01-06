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
#include "cert_verify.h"
#include "flashd/flashd.h"
#include "fs_manager/mount.h"
#include "include/updater/updater.h"
#include "language/language_ui.h"
#include "log/dump.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "package/pkg_manager.h"
#include "pkg_manager.h"
#include "pkg_utils.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "updater_ui_stub.h"
#include "utils.h"

namespace Updater {
using Utils::String2Int;
using namespace Hpackage;
using namespace Updater::Utils;
using namespace std::literals::chrono_literals;

[[maybe_unused]] constexpr int DISPLAY_TIME = 1000 * 1000;
constexpr struct option OPTIONS[] = {
    { "update_package", required_argument, nullptr, 0 },
    { "retry_count", required_argument, nullptr, 0 },
    { "factory_wipe_data", no_argument, nullptr, 0 },
    { "user_wipe_data", no_argument, nullptr, 0 },
    { "upgraded_pkg_num", required_argument, nullptr, 0 },
    { nullptr, 0, nullptr, 0 },
};
constexpr float VERIFY_PERCENT = 0.05;

static void SetMessageToMisc(const int message, const std::string headInfo)
{
    if (headInfo.empty()) {
        return;
    }
    std::vector<std::string> args = ParseParams(0, nullptr);
    struct UpdateMessage msg {};
    if (strncpy_s(msg.command, sizeof(msg.command), "boot_updater", strlen("boot_updater") + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncpy_s failed";
        return;
    }
    for (const auto& arg : args) {
        if (arg.find(headInfo) == std::string::npos) {
            if (strncat_s(msg.update, sizeof(msg.update), arg.c_str(), strlen(arg.c_str()) + 1) != EOK) {
                LOG(ERROR) << "SetMessageToMisc strncat_s failed";
                return;
            }
            if (strncat_s(msg.update, sizeof(msg.update), "\n", strlen("\n") + 1) != EOK) {
                LOG(ERROR) << "SetMessageToMisc strncat_s failed";
                return;
            }
        }
    }
    char buffer[128] {}; // 128 : set headInfo size
    if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s=%d", headInfo.c_str(), message) == -1) {
        LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
        return;
    }
    if (strncat_s(msg.update, sizeof(msg.update), buffer, strlen(buffer) + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncat_s failed";
        return;
    }
    if (WriteUpdaterMiscMsg(msg) != true) {
        LOG(ERROR) << "Write command to misc failed.";
    }
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

static int OtaUpdatePreCheck(PkgManager::PkgManagerPtr pkgManager, const std::string &packagePath)
{
    UPDATER_INIT_RECORD;
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Fail to GetPackageInstance";
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return UPDATE_CORRUPT;
    }
    char realPath[PATH_MAX + 1] = {0};
    if (realpath(packagePath.c_str(), realPath) == nullptr) {
        LOG(ERROR) << "realpath error";
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return PKG_INVALID_FILE;
    }
    if (access(realPath, F_OK) != 0) {
        LOG(ERROR) << "package does not exist!";
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return PKG_INVALID_FILE;
    }

    int32_t ret = pkgManager->VerifyOtaPackage(packagePath);
    if (ret != PKG_SUCCESS) {
        LOG(INFO) << "VerifyOtaPackage fail ret :"<< ret;
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    return PKG_SUCCESS;
}

static UpdaterStatus VerifyPackages(const UpdaterParams &upParams)
{
    LOG(INFO) << "Verify packages start...";
    UPDATER_UI_INSTANCE.ShowProgressPage();
    UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKG));

    UPDATER_UI_INSTANCE.ShowProgress(0.0);
    for (unsigned int i = upParams.pkgLocation; i < upParams.updatePackage.size(); i++) {
        LOG(INFO) << "Verify package:" << upParams.updatePackage[i];
        PkgManager::PkgManagerPtr manager = PkgManager::GetPackageInstance();
        int32_t verifyret = OtaUpdatePreCheck(manager, upParams.updatePackage[i]);
        PkgManager::ReleasePackageInstance(manager);

        if (verifyret != PKG_SUCCESS) {
            UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKGFAIL), true);
            UPDATER_LAST_WORD(UPDATE_CORRUPT);
            return UPDATE_CORRUPT;
        }
    }

    ProgressSmoothHandler(0, static_cast<int>(VERIFY_PERCENT * FULL_PERCENT_PROGRESS));
    LOG(INFO) << "Verify packages successfull...";
    return UPDATE_SUCCESS;
}

UpdaterStatus UpdaterFromSdcard()
{
#ifndef UPDATER_UT
    // sdcard fsType only support ext4/vfat
    auto sdParam = "updater.data.configs";
    Flashd::SetParameter(sdParam, "1");
    std::string sdcardStr = GetBlockDeviceByMountPoint(SDCARD_PATH);
    if (!IsSDCardExist(sdcardStr)) {
        UPDATER_UI_INSTANCE.ShowLog(
            (errno == ENOENT) ? TR(LOG_SDCARD_NOTFIND) : TR(LOG_SDCARD_ABNORMAL), true);
        return UPDATE_ERROR;
    }
    if (MountForPath(SDCARD_PATH) != 0) {
        int ret = mount(sdcardStr.c_str(), SDCARD_PATH, "vfat", 0, NULL);
        if (ret != 0) {
            LOG(WARNING) << "MountForPath /sdcard failed!";
            return UPDATE_ERROR;
        }
    }
#endif
    if (access(SDCARD_CARD_PKG_PATH, 0) != 0) {
        LOG(ERROR) << "package is not exist";
        UPDATER_UI_INSTANCE.ShowLog(TR(LOG_NOPKG), true);
        return UPDATE_ERROR;
    }

    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return UPDATE_ERROR;
    }
    UpdaterParams upParams {
        false, false, 0, 0, 1, 0, {SDCARD_CARD_PKG_PATH}
    };
    // verify packages first
    if (VerifyPackages(upParams) != UPDATE_SUCCESS) {
        PkgManager::ReleasePackageInstance(pkgManager);
        return UPDATE_ERROR;
    }
    upParams.initialProgress += VERIFY_PERCENT;

    STAGE(UPDATE_STAGE_BEGIN) << "UpdaterFromSdcard";
    LOG(INFO) << "UpdaterFromSdcard start, sdcard updaterPath : " << upParams.updatePackage[upParams.pkgLocation];
    UPDATER_UI_INSTANCE.ShowLog(TR(LOG_SDCARD_NOTMOVE));
    UpdaterStatus updateRet = DoInstallUpdaterPackage(pkgManager, upParams, SDCARD_UPDATE);
    if (updateRet != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_UI_INSTANCE.ShowLog(TR(LOG_SDCARD_FAIL));
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

static UpdaterStatus InstallUpdaterPackage(UpdaterParams &upParams, PkgManager::PkgManagerPtr manager)
{
    UPDATER_INIT_RECORD;
    UpdaterStatus status = UPDATE_UNKNOWN;
    STAGE(UPDATE_STAGE_BEGIN) << "Install package";
    if (upParams.retryCount == 0) {
        // First time enter updater, record retryCount in case of abnormal reset.
        if (!PartitionRecord::GetInstance().ClearRecordPartitionOffset()) {
            LOG(ERROR) << "ClearRecordPartitionOffset failed";
            return UPDATE_ERROR;
        }
        SetMessageToMisc(upParams.retryCount + 1, "retry_count");
    }
    status = DoInstallUpdaterPackage(manager, upParams, HOTA_UPDATE);
    if (status != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_UI_INSTANCE.ShowLog(TR(LOG_UPDFAIL));
        STAGE(UPDATE_STAGE_FAIL) << "Install failed";
        if (status == UPDATE_RETRY && upParams.retryCount < MAX_RETRY_COUNT) {
            upParams.retryCount += 1;
            UPDATER_UI_INSTANCE.ShowFailedPage();
            SetMessageToMisc(upParams.retryCount, "retry_count");
            Utils::DoReboot("updater");
        }
    } else {
        LOG(INFO) << "Install package success.";
        STAGE(UPDATE_STAGE_SUCCESS) << "Install package";
    }
    return status;
}

static UpdaterStatus CalcProgress(const UpdaterParams &upParams,
    std::vector<double> &pkgStartPosition, double &updateStartPosition)
{
    int64_t allPkgSize = 0;
    std::vector<int64_t> everyPkgSize;
    for (const auto &path : upParams.updatePackage) {
        char realPath[PATH_MAX + 1] = {0};
        if (realpath(path.c_str(), realPath) == nullptr) {
            LOG(ERROR) << "Can not find updatePackage : " << path;
            return UPDATE_ERROR;
        }
        struct stat st {};
        if (stat(realPath, &st) == 0) {
            everyPkgSize.push_back(st.st_size);
            allPkgSize += st.st_size;
            LOG(INFO) << "pkg " << path << " size is:" << st.st_size;
        }
    }
    pkgStartPosition.push_back(VERIFY_PERCENT);
    if (allPkgSize == 0) {
        LOG(ERROR) << "All packages's size is 0.";
        return UPDATE_ERROR;
    }
    int64_t startSize = 0;
    for (auto size : everyPkgSize) {
        startSize += size;
        float percent = static_cast<double>(startSize) / static_cast<double>(allPkgSize) + VERIFY_PERCENT;
        percent = (percent > 1.0) ? 1.0 : percent; // 1.0 : 100%
        LOG(INFO) << "percent is:" << percent;
        pkgStartPosition.push_back(percent);
    }

    updateStartPosition = pkgStartPosition[upParams.pkgLocation];
    return UPDATE_SUCCESS;
}

static UpdaterStatus PreUpdatePackages(UpdaterParams &upParams)
{
    LOG(INFO) << "start to update packages, start index:" << upParams.pkgLocation;
    for (unsigned int i = 0; i < upParams.updatePackage.size(); i++) {
        LOG(INFO) << "package " << i << ":" << upParams.updatePackage[i] <<
            " precent:" << upParams.currentPercentage;
    }

    UpdaterStatus status = UPDATE_UNKNOWN;
    if (SetupPartitions() != 0) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
        return UPDATE_ERROR;
    }
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    if (access(resultPath.c_str(), F_OK) != -1) {
        (void)DeleteFile(resultPath);
    }

    // verify packages first
    if (VerifyPackages(upParams) != UPDATE_SUCCESS) {
        return UPDATE_ERROR;
    }

    // Only handle UPATE_ERROR and UPDATE_SUCCESS here.Let package verify handle others.
    if (IsSpaceCapacitySufficient(upParams.updatePackage) == UPDATE_ERROR) {
        return status;
    }
    if (upParams.retryCount == 0 && !IsBatteryCapacitySufficient()) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(LOG_LOWPOWER));
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        LOG(ERROR) << "Battery is not sufficient for install package.";
        return UPDATE_SKIP;
    }
    return UPDATE_SUCCESS;
}

static UpdaterStatus DoUpdatePackages(UpdaterParams &upParams)
{
    UpdaterStatus status = UPDATE_UNKNOWN;
    std::vector<double> pkgStartPosition {};
    double updateStartPosition;
    status = CalcProgress(upParams, pkgStartPosition, updateStartPosition);
    if (status != UPDATE_SUCCESS) {
        return status;
    }
    UPDATER_UI_INSTANCE.ShowProgress(updateStartPosition * FULL_PERCENT_PROGRESS);
    for (; upParams.pkgLocation < upParams.updatePackage.size(); upParams.pkgLocation++) {
        PkgManager::PkgManagerPtr manager = PkgManager::GetPackageInstance();
        upParams.currentPercentage = pkgStartPosition[upParams.pkgLocation + 1] -
            pkgStartPosition[upParams.pkgLocation];
        upParams.initialProgress = pkgStartPosition[upParams.pkgLocation];
        LOG(INFO) << "InstallUpdaterPackage pkg is " << upParams.updatePackage[upParams.pkgLocation] <<
            " percent:" << upParams.initialProgress << "~" << pkgStartPosition[upParams.pkgLocation + 1];

        status = InstallUpdaterPackage(upParams, manager);
        SetMessageToMisc(upParams.pkgLocation + 1, "upgraded_pkg_num");
        ProgressSmoothHandler(
            static_cast<int>(upParams.initialProgress * FULL_PERCENT_PROGRESS +
            upParams.currentPercentage * GetTmpProgressValue()),
            static_cast<int>(pkgStartPosition[upParams.pkgLocation + 1] * FULL_PERCENT_PROGRESS));
        if (status != UPDATE_SUCCESS) {
            LOG(ERROR) << "InstallUpdaterPackage failed! Pkg is " << upParams.updatePackage[upParams.pkgLocation];
            if (!CheckDumpResult()) {
                UPDATER_LAST_WORD(status);
            }
            return status;
        }
        PkgManager::ReleasePackageInstance(manager);
    }
    UPDATER_UI_INSTANCE.ShowSuccessPage();
    return status;
}

static void PostUpdatePackages(UpdaterParams &upParams, bool updateResult)
{
    std::string writeBuffer;
    std::string buf;
    if (!updateResult) {
        const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
        std::ifstream fin {resultPath};
        if (!fin.is_open() || !std::getline(fin, buf)) {
            LOG(ERROR) << "read result file error " << resultPath;
            buf = "fail";
        }
    } else {
        buf = "pass";
        upParams.pkgLocation = upParams.pkgLocation == 0 ? upParams.pkgLocation : (upParams.pkgLocation - 1);
    }

    for (unsigned int i = 0; i < upParams.pkgLocation; i++) {
        writeBuffer += upParams.updatePackage[i] + "|pass\n";
    }
    writeBuffer += upParams.updatePackage[upParams.pkgLocation] + "|" + buf + "\n";
    for (unsigned int i = upParams.pkgLocation + 1; i < upParams.updatePackage.size(); i++) {
        writeBuffer += upParams.updatePackage[i] + "\n";
    }
    if (writeBuffer != "") {
        writeBuffer.pop_back();
    }
    LOG(INFO) << "post over, writeBuffer = " << writeBuffer;
    WriteDumpResult(writeBuffer);
}

static UpdaterStatus InstallUpdaterPackages(UpdaterParams &upParams)
{
    UpdaterStatus status = PreUpdatePackages(upParams);
    if (status == UPDATE_SUCCESS) {
        status = DoUpdatePackages(upParams);
    }
    PostUpdatePackages(upParams, status == UPDATE_SUCCESS);
    return status;
}

static UpdaterStatus StartUpdaterEntry(UpdaterParams &upParams)
{
    UpdaterStatus status = UPDATE_UNKNOWN;
    if (upParams.updatePackage.size() > 0) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        status = InstallUpdaterPackages(upParams);
    } else if (upParams.factoryWipeData) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        LOG(INFO) << "Factory level FactoryReset begin";
        status = UPDATE_SUCCESS;
#ifndef UPDATER_UT
        DoProgress();
#endif
        if (FactoryReset(FACTORY_WIPE_DATA, "/data") != 0) {
            LOG(ERROR) << "FactoryReset factory level failed";
            status = UPDATE_ERROR;
        }
        UPDATER_UI_INSTANCE.ShowLogRes(
            (status != UPDATE_SUCCESS) ? TR(LOGRES_FACTORY_FAIL) : TR(LOGRES_FACTORY_DONE));
    } else if (upParams.userWipeData) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        LOG(INFO) << "User level FactoryReset begin";
        status = UPDATE_SUCCESS;
#ifndef UPDATER_UT
        DoProgress();
#endif
        if (FactoryReset(USER_WIPE_DATA, "/data") != 0) {
            LOG(ERROR) << "FactoryReset user level failed";
            status = UPDATE_ERROR;
        }
        if (status != UPDATE_SUCCESS) {
            UPDATER_UI_INSTANCE.ShowLogRes(TR(LOGRES_WIPE_FAIL));
        } else {
            UPDATER_UI_INSTANCE.ShowSuccessPage();
            UPDATER_UI_INSTANCE.ShowLogRes(TR(LOGRES_WIPE_FINISH));
            PostUpdater(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
        }
    }
    return status;
}

static UpdaterStatus StartUpdater(const std::vector<std::string> &args,
    char **argv, PackageUpdateMode &mode)
{
    UpdaterParams upParams {
        false, false, 0, 0, 0, 0
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
                    upParams.updatePackage.push_back(optarg);
                    (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::OTA);
                    mode = HOTA_UPDATE;
                } else if (option == "retry_count") {
                    upParams.retryCount = atoi(optarg);
                    (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::OTA);
                    mode = HOTA_UPDATE;
                } else if (option == "factory_wipe_data") {
                    (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::REBOOTFACTORYRST);
                    upParams.factoryWipeData = true;
                } else if (option == "user_wipe_data") {
                    (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::REBOOTFACTORYRST);
                    upParams.userWipeData = true;
                } else if (option == "upgraded_pkg_num") {
                    upParams.pkgLocation = static_cast<unsigned int>(atoi(optarg));
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
    if (upParams.factoryWipeData && upParams.userWipeData) {
        LOG(WARNING) << "Factory level reset and user level reset both set. use user level reset.";
        upParams.factoryWipeData = false;
    }
    return StartUpdaterEntry(upParams);
}

int UpdaterMain(int argc, char **argv)
{
    [[maybe_unused]] UpdaterStatus status = UPDATE_UNKNOWN;
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_PRE_INIT_EVENT);
    std::vector<std::string> args = ParseParams(argc, argv);

    LOG(INFO) << "Ready to start";
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
    UPDATER_UI_INSTANCE.InitEnv();
#endif
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_INIT_EVENT);
    PackageUpdateMode mode = UNKNOWN_UPDATE;
    status = StartUpdater(args, argv, mode);
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
    UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
    if (status != UPDATE_SUCCESS && status != UPDATE_SKIP) {
        if (mode == HOTA_UPDATE) {
            UPDATER_UI_INSTANCE.ShowFailedPage();
        } else {
            UPDATER_UI_INSTANCE.ShowMainpage();
            UPDATER_UI_INSTANCE.Sleep(50); /* wait for page flush 50ms */
            UPDATER_UI_INSTANCE.SaveScreen();
        }
        // Wait for user input
        while (true) {
            Utils::UsSleep(DISPLAY_TIME);
        }
        return 0;
    }
#endif
    PostUpdater(true);
    Utils::DoReboot("");
    return 0;
}
} // Updater
