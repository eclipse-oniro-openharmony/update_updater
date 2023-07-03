/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include "json_node.h"
#include "language/language_ui.h"
#include "log/dump.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "package/pkg_manager.h"
#include "pkg_manager.h"
#include "pkg_utils.h"
#include "ptable_parse/ptable_process.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "updater/updater_preprocess.h"
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
    { "sdcard_update", no_argument, nullptr, 0 },
    { "upgraded_pkg_num", required_argument, nullptr, 0 },
    { "force_update_action", required_argument, nullptr, 0 },
    { nullptr, 0, nullptr, 0 },
};
constexpr float VERIFY_PERCENT = 0.05;

static void SetMessageToMisc(const std::string &miscCmd, const int message, const std::string headInfo)
{
    if (headInfo.empty()) {
        return;
    }
    std::vector<std::string> args = ParseParams(0, nullptr);
    struct UpdateMessage msg {};
    if (strncpy_s(msg.command, sizeof(msg.command), miscCmd.c_str(), miscCmd.size() + 1) != EOK) {
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
    if (headInfo == "sdcard_update") {
        if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s", headInfo.c_str()) == -1) {
            LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
            return;
        }
    } else {
        if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s=%d", headInfo.c_str(), message) == -1) {
            LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
            return;
        }
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
        LOG(ERROR) << "pkgManager is nullptr";
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

static UpdaterStatus VerifyPackages(UpdaterParams &upParams)
{
    LOG(INFO) << "Verify packages start...";
    UPDATER_UI_INSTANCE.ShowProgressPage();
    UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKG));

    if (upParams.callbackProgress == nullptr) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKGFAIL), true);
        UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT;
    }
    upParams.callbackProgress(0.0);
    for (unsigned int i = upParams.pkgLocation; i < upParams.updatePackage.size(); i++) {
        LOG(INFO) << "Verify package:" << upParams.updatePackage[i];
        PkgManager::PkgManagerPtr manager = PkgManager::CreatePackageInstance();
        int32_t verifyret = OtaUpdatePreCheck(manager, upParams.updatePackage[i]);
        PkgManager::ReleasePackageInstance(manager);

        if (verifyret != PKG_SUCCESS) {
            upParams.pkgLocation = i;
            UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKGFAIL), true);
            UPDATER_LAST_WORD(UPDATE_CORRUPT);
            return UPDATE_CORRUPT;
        }
    }

    ProgressSmoothHandler(0, static_cast<int>(VERIFY_PERCENT * FULL_PERCENT_PROGRESS));
    LOG(INFO) << "Verify packages successfull...";
    return UPDATE_SUCCESS;
}

static UpdaterStatus GetSdcardPkgsPath(UpdaterParams &upParams)
{
    if (upParams.updatePackage.size() != 0) {
        LOG(INFO) << "get sdcard packages from misc";
        return UPDATE_SUCCESS;
    }
    LOG(INFO) << "get sdcard packages from default path";
    std::vector<std::string> sdcardPkgs = Utils::SplitString(SDCARD_CARD_PKG_PATH, ", ");
    for (auto pkgPath : sdcardPkgs) {
        if (access(pkgPath.c_str(), 0) == 0) {
            LOG(INFO) << "find sdcard package : " << pkgPath;
            upParams.updatePackage.push_back(pkgPath);
        }
    }
    if (upParams.updatePackage.size() == 0) {
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

static UpdaterStatus CheckSdcardPkgs(UpdaterParams &upParams)
{
#ifndef UPDATER_UT
    auto sdParam = "updater.data.configs";
    Flashd::SetParameter(sdParam, "1");
    std::string sdcardStr = GetBlockDeviceByMountPoint(SDCARD_PATH);
    if (!IsSDCardExist(sdcardStr)) {
        UPDATER_UI_INSTANCE.ShowLog(
            (errno == ENOENT) ? TR(LOG_SDCARD_NOTFIND) : TR(LOG_SDCARD_ABNORMAL), true);
        return UPDATE_ERROR;
    }
    std::string sdcardPath = SDCARD_PATH;
    if (MountSdcard(sdcardPath, sdcardStr) != 0) {
        LOG(ERROR) << "mount sdcard fail!";
        return UPDATE_ERROR;
    }
#endif
    if (GetSdcardPkgsPath(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "there is no package in sdcard/updater, please check";
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
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
    struct UpdateMessage boot {};
    if (ReadUpdaterMiscMsg(boot) && strcmp(boot.command, "boot_updater") == 0) {
        LOG(INFO) << "this is OTA update, on need to determine the battery";
        return true;
    }
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
    UpdaterStatus status = UPDATE_UNKNOWN;
    STAGE(UPDATE_STAGE_BEGIN) << "Install package";
    if (upParams.retryCount == 0) {
        // First time enter updater, record retryCount in case of abnormal reset.
        if (!PartitionRecord::GetInstance().ClearRecordPartitionOffset()) {
            LOG(ERROR) << "ClearRecordPartitionOffset failed";
            UPDATER_LAST_WORD(UPDATE_ERROR);
            return UPDATE_ERROR;
        }
        SetMessageToMisc(upParams.miscCmd, upParams.retryCount + 1, "retry_count");
    }
    if (upParams.sdcardUpdate) {
        status = DoInstallUpdaterPackage(manager, upParams, SDCARD_UPDATE);
    } else {
        status = DoInstallUpdaterPackage(manager, upParams, HOTA_UPDATE);
    }
    if (status != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_UI_INSTANCE.ShowLog(TR(LOG_UPDFAIL));
        STAGE(UPDATE_STAGE_FAIL) << "Install failed";
        if (status == UPDATE_RETRY && upParams.retryCount < MAX_RETRY_COUNT) {
            upParams.retryCount += 1;
            UPDATER_UI_INSTANCE.ShowFailedPage();
            SetMessageToMisc(upParams.miscCmd, upParams.retryCount, "retry_count");
            Utils::UpdaterDoReboot("updater");
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
    UPDATER_INIT_RECORD;
    int64_t allPkgSize = 0;
    std::vector<int64_t> everyPkgSize;
    for (const auto &path : upParams.updatePackage) {
        char realPath[PATH_MAX + 1] = {0};
        if (realpath(path.c_str(), realPath) == nullptr) {
            LOG(ERROR) << "Can not find updatePackage : " << path;
            UPDATER_LAST_WORD(UPDATE_ERROR);
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
        UPDATER_LAST_WORD(UPDATE_ERROR);
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
    UPDATER_INIT_RECORD;
    LOG(INFO) << "start to update packages, start index:" << upParams.pkgLocation;

    UpdaterStatus status = UPDATE_UNKNOWN;
    if (SetupPartitions() != 0) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR;
    }
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    if (access(resultPath.c_str(), F_OK) != -1) {
        (void)DeleteFile(resultPath);
        LOG(INFO) << "deleate last upgrade file";
    }

    // verify packages first
    if (VerifyPackages(upParams) != UPDATE_SUCCESS) {
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR;
    }

    for (unsigned int i = 0; i < upParams.updatePackage.size(); i++) {
        int ret = PreProcess::GetInstance().DoUpdateAuth(upParams.updatePackage[i]);
        if (ret == 0) {
            LOG(INFO) << upParams.updatePackage[i] << " auth success";
        } else {
            upParams.pkgLocation = i;
            LOG(ERROR) << upParams.updatePackage[i] << " auth failed";
            UPDATER_LAST_WORD(UPDATE_ERROR);
            return UPDATE_ERROR;
        }
    }

    // Only handle UPATE_ERROR and UPDATE_SUCCESS here.Let package verify handle others.
    if (IsSpaceCapacitySufficient(upParams.updatePackage) == UPDATE_ERROR) {
        UPDATER_LAST_WORD(status);
        return status;
    }

#ifdef UPDATER_USE_PTABLE
    if (!PtablePreProcess::GetInstance().DoPtableProcess(upParams)) {
        LOG(ERROR) << "DoPtableProcess failed";
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR;
    }
#endif
    return UPDATE_SUCCESS;
}

static UpdaterStatus DoUpdatePackages(UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    UpdaterStatus status = UPDATE_UNKNOWN;
    std::vector<double> pkgStartPosition {};
    double updateStartPosition;
    status = CalcProgress(upParams, pkgStartPosition, updateStartPosition);
    if (status != UPDATE_SUCCESS) {
        UPDATER_LAST_WORD(status);
        return status;
    }
    for (unsigned int i = 0; i < upParams.updatePackage.size(); i++) {
        LOG(INFO) << "package " << i << ":" << upParams.updatePackage[i] <<
            " precent:" << upParams.currentPercentage;
    }
    if (upParams.callbackProgress == nullptr) {
        LOG(ERROR) << "CallbackProgress is nullptr";
        return UPDATE_CORRUPT;
    }
    upParams.callbackProgress(updateStartPosition * FULL_PERCENT_PROGRESS);
    for (; upParams.pkgLocation < upParams.updatePackage.size(); upParams.pkgLocation++) {
        PkgManager::PkgManagerPtr manager = PkgManager::CreatePackageInstance();
        upParams.currentPercentage = pkgStartPosition[upParams.pkgLocation + 1] -
            pkgStartPosition[upParams.pkgLocation];
        upParams.initialProgress = pkgStartPosition[upParams.pkgLocation];
        LOG(INFO) << "InstallUpdaterPackage pkg is " << upParams.updatePackage[upParams.pkgLocation] <<
            " percent:" << upParams.initialProgress << "~" << pkgStartPosition[upParams.pkgLocation + 1];

        status = InstallUpdaterPackage(upParams, manager);
        SetMessageToMisc(upParams.miscCmd, upParams.pkgLocation + 1, "upgraded_pkg_num");
        ProgressSmoothHandler(
            static_cast<int>(upParams.initialProgress * FULL_PERCENT_PROGRESS +
            upParams.currentPercentage * GetTmpProgressValue()),
            static_cast<int>(pkgStartPosition[upParams.pkgLocation + 1] * FULL_PERCENT_PROGRESS));
        if (status != UPDATE_SUCCESS) {
            LOG(ERROR) << "InstallUpdaterPackage failed! Pkg is " << upParams.updatePackage[upParams.pkgLocation];
            if (!CheckDumpResult()) {
                UPDATER_LAST_WORD(status);
            }
            PkgManager::ReleasePackageInstance(manager);
            return status;
        }
        PkgManager::ReleasePackageInstance(manager);
    }
    if (upParams.forceUpdate) {
        UPDATER_UI_INSTANCE.ShowLogRes(TR(LABEL_UPD_OK_SHUTDOWN));
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

UpdaterStatus UpdaterFromSdcard(UpdaterParams &upParams)
{
    upParams.callbackProgress = [] (float value) { UPDATER_UI_INSTANCE.ShowProgress(value); };
    SetMessageToMisc(upParams.miscCmd, 0, "sdcard_update");
    if (CheckSdcardPkgs(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "can not find sdcard packages";
        return UPDATE_ERROR;
    }
    // verify packages first
    if (upParams.retryCount == 0 && !IsBatteryCapacitySufficient()) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(LOG_LOWPOWER));
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        LOG(ERROR) << "Battery is not sufficient for install package.";
        return UPDATE_SKIP;
    }

    if (upParams.pkgLocation == 0 && VerifyPackages(upParams) != UPDATE_SUCCESS) {
        return UPDATE_ERROR;
    }
#ifdef UPDATER_USE_PTABLE
    if (!PtablePreProcess::GetInstance().DoPtableProcess(upParams)) {
        LOG(ERROR) << "DoPtableProcess failed";
        return UPDATE_ERROR;
    }
#endif
    upParams.initialProgress += VERIFY_PERCENT;
    upParams.currentPercentage -= VERIFY_PERCENT;

    STAGE(UPDATE_STAGE_BEGIN) << "UpdaterFromSdcard";
    LOG(INFO) << "UpdaterFromSdcard start, sdcard updaterPath : " << upParams.updatePackage[upParams.pkgLocation];
    UPDATER_UI_INSTANCE.ShowLog(TR(LOG_SDCARD_NOTMOVE));
    return DoUpdatePackages(upParams);
}

UpdaterStatus InstallUpdaterPackages(UpdaterParams &upParams)
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
    if (upParams.sdcardUpdate) {
        LOG(INFO) << "start sdcard update";
        UPDATER_UI_INSTANCE.ShowProgressPage();
        status = UpdaterFromSdcard(upParams);
        return status;
    } else if (upParams.updatePackage.size() > 0) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        status = InstallUpdaterPackages(upParams);
    } else if (upParams.factoryWipeData) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        LOG(INFO) << "Factory level FactoryReset begin";
        status = UPDATE_SUCCESS;
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
        DoProgress();
#endif
        if (FactoryReset(FACTORY_WIPE_DATA, "/data") != 0) {
            LOG(ERROR) << "FactoryReset factory level failed";
            status = UPDATE_ERROR;
        }
        UPDATER_UI_INSTANCE.ShowLogRes(
            (status != UPDATE_SUCCESS) ? TR(LOGRES_FACTORY_FAIL) : TR(LOGRES_FACTORY_DONE));
        UpdaterInit::GetInstance().InvokeEvent(UPDATER_RPMB_DATA_CLEAR_EVENT);
    } else if (upParams.userWipeData) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        LOG(INFO) << "User level FactoryReset begin";
        status = UPDATE_SUCCESS;
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
        DoProgress();
#endif
        if (FactoryReset(USER_WIPE_DATA, "/data") != 0) {
            LOG(ERROR) << "FactoryReset user level failed";
            status = UPDATE_ERROR;
        }
        if (status != UPDATE_SUCCESS) {
            UPDATER_UI_INSTANCE.ShowLogRes(TR(LOGRES_WIPE_FAIL));
        } else {
            UpdaterInit::GetInstance().InvokeEvent(UPDATER_RPMB_DATA_CLEAR_EVENT);
            UPDATER_UI_INSTANCE.ShowSuccessPage();
            UPDATER_UI_INSTANCE.ShowLogRes(TR(LOGRES_WIPE_FINISH));
            PostUpdater(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
        }
    }
    return status;
}

static UpdaterStatus StartUpdater(const std::vector<std::string> &args,
    char **argv, PackageUpdateMode &mode, UpdaterParams &upParams)
{
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
                } else if (option == "factory_wipe_data") {
                    (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::REBOOTFACTORYRST);
                    upParams.factoryWipeData = true;
                } else if (option == "user_wipe_data") {
                    (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::REBOOTFACTORYRST);
                    upParams.userWipeData = true;
                } else if (option == "upgraded_pkg_num") {
                    upParams.pkgLocation = static_cast<unsigned int>(atoi(optarg));
                } else if (option == "sdcard_update") {
                    upParams.sdcardUpdate = true;
                } else if (option == "force_update_action" && std::string(optarg) == POWEROFF) { /* Only for OTA. */
                    upParams.forceUpdate = true;
                }
                break;
            }
            default:
                LOG(ERROR) << "Invalid argument.";
                break;
        }
    }
    optind = 1;
    // Sanity checks
    if (upParams.sdcardUpdate) {
        (void)UPDATER_UI_INSTANCE.SetMode(UpdaterMode::SDCARD);
        mode = SDCARD_UPDATE;
    }
    if (upParams.factoryWipeData && upParams.userWipeData) {
        LOG(WARNING) << "Factory level reset and user level reset both set. use user level reset.";
        upParams.factoryWipeData = false;
    }
    return StartUpdaterEntry(upParams);
}

// add updater mode
REGISTER_MODE(Updater, "updater.hdc.configfs");

int UpdaterMain(int argc, char **argv)
{
    [[maybe_unused]] UpdaterStatus status = UPDATE_UNKNOWN;
    UpdaterParams upParams;
    upParams.callbackProgress = [] (float value) { UPDATER_UI_INSTANCE.ShowProgress(value); };
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_PRE_INIT_EVENT);
    std::vector<std::string> args = ParseParams(argc, argv);

    LOG(INFO) << "Ready to start";
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
    UPDATER_UI_INSTANCE.InitEnv();
#endif
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_INIT_EVENT);
    PackageUpdateMode mode = UNKNOWN_UPDATE;
    status = StartUpdater(args, argv, mode, upParams);
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
    UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
    if (status != UPDATE_SUCCESS && status != UPDATE_SKIP) {
        if (mode == HOTA_UPDATE) {
            UPDATER_UI_INSTANCE.ShowFailedPage();
        } else if (mode == SDCARD_UPDATE) {
            UPDATER_UI_INSTANCE.ShowLogRes(
                status == UPDATE_CORRUPT ? TR(LOGRES_VERIFY_FAILED) : TR(LOGRES_UPDATE_FAILED));
            UPDATER_UI_INSTANCE.ShowFailedPage();
            Utils::UsSleep(5 * DISPLAY_TIME); // 5 : 5s
            UPDATER_UI_INSTANCE.ShowMainpage();
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
    upParams.forceUpdate ? Utils::DoShutdown() : Utils::UpdaterDoReboot("");
    return 0;
}
} // Updater
