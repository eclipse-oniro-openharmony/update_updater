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
#include "sdcard_update/sdcard_update.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "updater/hwfault_retry.h"
#include "updater/updater_preprocess.h"
#include "updater_ui_stub.h"
#include "utils.h"
#include "factory_reset/factory_reset.h"
#include "write_state/write_state.h"

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
    { "menu_wipe_data", no_argument, nullptr, 0 },
    { "sdcard_update", no_argument, nullptr, 0 },
    { "upgraded_pkg_num", required_argument, nullptr, 0 },
    { "force_update_action", required_argument, nullptr, 0 },
    { "night_update", no_argument, nullptr, 0 },
    { USB_MODE, no_argument, nullptr, 0 },
    { "UPDATE:MAINIMG", no_argument, nullptr, 0 },
    { "update_protect", no_argument, nullptr, 0 },
    { "factory_sd_update", no_argument, nullptr, 0 },
    { "UPDATE:SD", no_argument, nullptr, 0 },
    { "UPDATE:SDFROMDEV", no_argument, nullptr, 0 },
    { "sdcard_intral_update", optional_argument, nullptr, 0},
    { "shrink_info", required_argument, nullptr, 0 },
    { "virtual_shrink_info", required_argument, nullptr, 0 },
    {"wipe_data_factory_lowlevel", no_argument, nullptr, 0},
    { "wipe_data_at_factoryreset_0", no_argument, nullptr, 0 },
    { nullptr, 0, nullptr, 0 },
};
constexpr float VERIFY_PERCENT = 0.05;
constexpr double FULL_PERCENT = 100.00;

int FactoryReset(FactoryResetMode mode, const std::string &path)
{
    UpdaterInit::GetInstance().InvokeEvent(FACTORY_RESET_INIT_EVENT);
    auto ret = FactoryResetProcess::GetInstance().DoFactoryReset(mode, path);
    if (ret == 0) {
        LOG(INFO) << "restorecon for " << path;
        RestoreconPath(path); // restore selinux context for /data after factory reset success
    }
    return ret;
}

static int OtaUpdatePreCheck(PkgManager::PkgManagerPtr pkgManager, const std::string &packagePath)
{
    UPDATER_INIT_RECORD;
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        UPDATER_LAST_WORD(PKG_INVALID_FILE, "pkgManager is nullptr");
        return UPDATE_CORRUPT;
    }
    char realPath[PATH_MAX + 1] = {0};
    if (realpath(packagePath.c_str(), realPath) == nullptr) {
        LOG(ERROR) << "realpath error";
        UPDATER_LAST_WORD(PKG_INVALID_FILE, "realpath error");
        return PKG_INVALID_FILE;
    }
    if (access(realPath, F_OK) != 0) {
        LOG(ERROR) << "package does not exist!";
        UPDATER_LAST_WORD(PKG_INVALID_FILE, "package does not exist!");
        return PKG_INVALID_FILE;
    }

    int32_t ret = pkgManager->VerifyOtaPackage(packagePath);
    if (ret != PKG_SUCCESS) {
        LOG(INFO) << "VerifyOtaPackage fail ret :" << ret;
        UPDATER_LAST_WORD("sign", ret);
        return ret;
    }

    return UPDATE_SUCCESS;
}

static UpdaterStatus UpdatePreCheck(UpdaterParams &upParams, const std::string pkgPath)
{
    UPDATER_INIT_RECORD;
    int32_t ret = PreProcess::GetInstance().DoUpdateAuth(pkgPath);
    if (ret != 0) {
        return UPDATE_ERROR;
    }

    PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "CreatePackageInstance fail";
        return UPDATE_ERROR;
    }
    if (GetUpdatePackageInfo(pkgManager, pkgPath) != PKG_SUCCESS) {
        PkgManager::ReleasePackageInstance(pkgManager);
        LOG(ERROR) << "Verify update bin file Fail!";
        UPDATER_LAST_WORD(UPDATE_ERROR, "Verify update bin file Fail!");
        return UPDATE_ERROR;
    }
    if (PreProcess::GetInstance().DoUpdatePreProcess(upParams, pkgManager) != PKG_SUCCESS) {
        PkgManager::ReleasePackageInstance(pkgManager);
        LOG(ERROR) << "Version Check Fail!";
        UPDATER_LAST_WORD(UPDATE_ERROR, "Version Check Fail!");
        return UPDATE_ERROR;
    }
    if (PreProcess::GetInstance().DoUpdateClear() != 0) {
        LOG(ERROR) << "DoUpdateClear Fail!";
    }
    PkgManager::ReleasePackageInstance(pkgManager);
    return UPDATE_SUCCESS;
}

__attribute__((weak)) int32_t VerifySpecialPkgs([[maybe_unused]]UpdaterParams &upParams)
{
    return PKG_SUCCESS;
}

__attribute__((weak)) void UpdaterVerifyFailEntry(bool verifyret)
{
    LOG(INFO) << "pre verify package info process";
    return;
}

__attribute__((weak)) UpdaterStatus NotifyActionResult(UpdaterParams &upParams,
    UpdaterStatus &status, const std::vector<NotifyAction> &notifyActionVec)
{
    return UPDATE_SUCCESS;
}

__attribute__((weak)) void NotifyReboot(const std::string& rebootTarget, const std::string &rebootReason,
    const std::string &extData, const std::string &sdExtMode)
{
    Updater::Utils::UpdaterDoReboot(rebootTarget, rebootReason, extData);
}

static UpdaterStatus VerifyPackages(UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    LOG(INFO) << "Verify packages start...";
    UPDATER_UI_INSTANCE.ShowProgressPage();
    UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKG));

    if (upParams.callbackProgress == nullptr) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKGFAIL), true);
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "upParams.callbackProgress is null");
        return UPDATE_CORRUPT;
    }
    upParams.callbackProgress(0.0);
    upParams.installTime.resize(upParams.updatePackage.size(), std::chrono::duration<double>(0));
    ReadInstallTime(upParams);
    for (unsigned int i = upParams.pkgLocation; i < upParams.updatePackage.size(); i++) {
        LOG(INFO) << "Verify package:" << upParams.updatePackage[i];
        auto startTime = std::chrono::system_clock::now();
        PkgManager::PkgManagerPtr manager = PkgManager::CreatePackageInstance();
        if (manager == nullptr) {
            LOG(ERROR) << "CreatePackageInstance fail";
            return UPDATE_ERROR;
        }
        int32_t verifyret = OtaUpdatePreCheck(manager, upParams.updatePackage[i]);
        PkgManager::ReleasePackageInstance(manager);
        if (verifyret == UPDATE_SUCCESS) {
            verifyret = UpdatePreCheck(upParams, upParams.updatePackage[i]);
        }

        if (verifyret != UPDATE_SUCCESS) {
            UpdaterVerifyFailEntry((verifyret == PKG_INVALID_DIGEST) && (upParams.updateMode == HOTA_UPDATE));
            upParams.pkgLocation = i;
            UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKGFAIL), true);
            auto endTime = std::chrono::system_clock::now();
            upParams.installTime[i] = endTime - startTime;
            return UPDATE_CORRUPT;
        }
        auto endTime = std::chrono::system_clock::now();
        upParams.installTime[i] = endTime - startTime;
    }
    if (VerifySpecialPkgs(upParams) != PKG_SUCCESS) {
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "VerifySpecialPkgs failed");
        return UPDATE_CORRUPT;
    }
    ProgressSmoothHandler(UPDATER_UI_INSTANCE.GetCurrentPercent(),
        UPDATER_UI_INSTANCE.GetCurrentPercent() + static_cast<int>(VERIFY_PERCENT * FULL_PERCENT_PROGRESS));
    LOG(INFO) << "Verify packages successfull...";
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

__attribute__((weak)) bool IsSpareBoardBoot(void)
{
    LOG(INFO) << "no need check spareboardboot";
    return false;
}

bool IsBatteryCapacitySufficient()
{
    if (Utils::CheckUpdateMode(OTA_MODE)) {
        LOG(INFO) << "this is OTA update, on need to determine the battery";
        return true;
    }
    if (IsSpareBoardBoot()) {
        LOG(INFO) << "this is spare board boot, no need to determine the battery";
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

    return capacity >= lowLevel;
}

UpdaterStatus InstallUpdaterPackage(UpdaterParams &upParams, PkgManager::PkgManagerPtr manager)
{
    UPDATER_INIT_RECORD;
    UpdaterStatus status = UPDATE_UNKNOWN;
    STAGE(UPDATE_STAGE_BEGIN) << "Install package";
    if (upParams.retryCount == 0) {
        // First time enter updater, record retryCount in case of abnormal reset.
        if (!PartitionRecord::GetInstance().ClearRecordPartitionOffset()) {
            LOG(ERROR) << "ClearRecordPartitionOffset failed";
            UPDATER_LAST_WORD(UPDATE_ERROR, "ClearRecordPartitionOffset failed");
            return UPDATE_ERROR;
        }
        SetMessageToMisc(upParams.miscCmd, upParams.retryCount + 1, "retry_count");
    }
    if (upParams.updateMode == SDCARD_UPDATE) {
        status = DoInstallUpdaterPackage(manager, upParams, SDCARD_UPDATE);
    } else {
        status = DoInstallUpdaterPackage(manager, upParams, HOTA_UPDATE);
    }
    if (status != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_UI_INSTANCE.ShowLog(TR(LOG_UPDFAIL));
        STAGE(UPDATE_STAGE_FAIL) << "Install failed";
        if (status == UPDATE_RETRY) {
            HwFaultRetry::GetInstance().DoRetryAction();
            UPDATER_UI_INSTANCE.ShowFailedPage();
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
    if (upParams.pkgLocation == upParams.updatePackage.size()) {
        updateStartPosition = VERIFY_PERCENT;
        return UPDATE_SUCCESS;
    }
    for (const auto &path : upParams.updatePackage) {
        char realPath[PATH_MAX + 1] = {0};
        if (realpath(path.c_str(), realPath) == nullptr) {
            LOG(WARNING) << "Can not find updatePackage : " << path;
            everyPkgSize.push_back(0);
            continue;
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
        UPDATER_LAST_WORD(UPDATE_ERROR, "All packages's size is 0.");
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

static int CheckMountData()
{
    UPDATER_INIT_RECORD;
    constexpr int retryTime = 3;
    for (int i = 0; i < retryTime; i++) {
        if (SetupPartitions() == 0) {
            return 0;
        }
        LOG(INFO) << "retry mount userdata number:" << i;
        Utils::UsSleep(DISPLAY_TIME);
    }
    UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
    UPDATER_LAST_WORD(UPDATE_ERROR, "retry mount userdata more than three times");
    return UPDATE_ERROR;
}

static UpdaterStatus CheckVerifyPackages(UpdaterParams &upParams)
{
    UpdaterStatus status = UPDATE_SUCCESS;
    if (NotifyActionResult(upParams, status, {PROCESS_PACKAGE, SET_INSTALL_STATUS, GET_INSTALL_STATUS}) !=
        UPDATE_SUCCESS) {
        LOG(ERROR) << "notify action fail";
        return UPDATE_CORRUPT;
    }
    return status;
}

static UpdaterStatus PreUpdatePackages(UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    LOG(INFO) << "start to update packages, start index:" << upParams.pkgLocation;

    UpdaterStatus status = UPDATE_UNKNOWN;

    upParams.installTime.resize(upParams.updatePackage.size(), std::chrono::duration<double>(0));
    if (CheckMountData() != 0) {
        return UPDATE_ERROR;
    }
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    if (access(resultPath.c_str(), F_OK) != -1) {
        (void)DeleteFile(resultPath);
        LOG(INFO) << "delete last upgrade file";
    }

    if (upParams.pkgLocation == upParams.updatePackage.size()) {
        LOG(WARNING) << "all package has been upgraded, skip pre process";
        return UPDATE_SUCCESS;
    }

    // verify package first
    if (VerifyPackages(upParams) != UPDATE_SUCCESS) {
        return UPDATE_CORRUPT; // verify package failed must return UPDATE_CORRUPT, ux need it !!!
    }

    // Only handle UPATE_ERROR and UPDATE_SUCCESS here.Let package verify handle others.
    if (IsSpaceCapacitySufficient(upParams) == UPDATE_ERROR) {
        UPDATER_LAST_WORD(status, "space nott enough");
        return status;
    }
    if (upParams.retryCount == 0 && !IsBatteryCapacitySufficient()) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(LOG_LOWPOWER));
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_LAST_WORD(UPDATE_ERROR, "Battery is not sufficient for install package.");
        LOG(ERROR) << "Battery is not sufficient for install package.";
        return UPDATE_SKIP;
    }

#ifdef UPDATER_USE_PTABLE
    if (!PtablePreProcess::GetInstance().DoPtableProcess(upParams)) {
        LOG(ERROR) << "DoPtableProcess failed";
        UPDATER_LAST_WORD(UPDATE_ERROR, "DoPtableProcess failed");
        return UPDATE_ERROR;
    }
#endif
    if (CheckVerifyPackages(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "verify packages fail";
        return UPDATE_CORRUPT;
    }
    return UPDATE_SUCCESS;
}

static UpdaterStatus DoInstallPackages(UpdaterParams &upParams, std::vector<double> &pkgStartPosition)
{
    UPDATER_INIT_RECORD;
    UpdaterStatus status = UPDATE_UNKNOWN;
    if (upParams.pkgLocation == upParams.updatePackage.size()) {
        LOG(WARNING) << "all packages has been installed, directly return success";
        upParams.callbackProgress(FULL_PERCENT_PROGRESS);
        return UPDATE_SUCCESS;
    }
    for (; upParams.pkgLocation < upParams.updatePackage.size(); upParams.pkgLocation++) {
        PkgManager::PkgManagerPtr manager = PkgManager::CreatePackageInstance();
        if (manager == nullptr) {
            LOG(ERROR) << "CreatePackageInstance fail";
            return UPDATE_ERROR;
        }
        auto startTime = std::chrono::system_clock::now();
        upParams.initialProgress = ((UPDATER_UI_INSTANCE.GetCurrentPercent() / FULL_PERCENT) >
            pkgStartPosition[upParams.pkgLocation]) ?
            (UPDATER_UI_INSTANCE.GetCurrentPercent() / FULL_PERCENT) : pkgStartPosition[upParams.pkgLocation];
        upParams.currentPercentage = pkgStartPosition[upParams.pkgLocation + 1] - upParams.initialProgress;
        LOG(INFO) << "InstallUpdaterPackage pkg is " << upParams.updatePackage[upParams.pkgLocation] <<
            " percent:" << upParams.initialProgress << "~" << pkgStartPosition[upParams.pkgLocation + 1];

        status = InstallUpdaterPackage(upParams, manager);
        auto endTime = std::chrono::system_clock::now();
        upParams.installTime[upParams.pkgLocation] = upParams.installTime[upParams.pkgLocation] + endTime - startTime;
        WriteInstallTime(upParams);
        if (status != UPDATE_SUCCESS) {
            LOG(ERROR) << "InstallUpdaterPackage failed! Pkg is " << upParams.updatePackage[upParams.pkgLocation];
            if (!CheckResultFail()) {
                UPDATER_LAST_WORD(status, "InstallUpdaterPackage failed");
            }
            PkgManager::ReleasePackageInstance(manager);
            return status;
        }
        ProgressSmoothHandler(
            static_cast<int>(upParams.initialProgress * FULL_PERCENT_PROGRESS +
            upParams.currentPercentage * GetTmpProgressValue()),
            static_cast<int>(pkgStartPosition[upParams.pkgLocation + 1] * FULL_PERCENT_PROGRESS));
        SetMessageToMisc(upParams.miscCmd, upParams.pkgLocation + 1, "upgraded_pkg_num");
        PkgManager::ReleasePackageInstance(manager);
    }
    return status;
}

UpdaterStatus DoUpdatePackages(UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    UpdaterStatus status = UPDATE_UNKNOWN;
    std::vector<double> pkgStartPosition {};
    double updateStartPosition = 0.0;
    status = CalcProgress(upParams, pkgStartPosition, updateStartPosition);
    if (status != UPDATE_SUCCESS) {
        UPDATER_LAST_WORD(status, "CalcProgress failed");
        return status;
    }
    for (unsigned int i = 0; i < upParams.updatePackage.size(); i++) {
        LOG(INFO) << "package " << i << ":" << upParams.updatePackage[i] <<
            " percent:" << upParams.currentPercentage;
    }
    if (upParams.callbackProgress == nullptr) {
        LOG(ERROR) << "CallbackProgress is nullptr";
        return UPDATE_CORRUPT;
    }
    float value = (UPDATER_UI_INSTANCE.GetCurrentPercent() > (updateStartPosition * FULL_PERCENT_PROGRESS)) ?
        UPDATER_UI_INSTANCE.GetCurrentPercent() : (updateStartPosition * FULL_PERCENT_PROGRESS);
    upParams.callbackProgress(value);
    status = DoInstallPackages(upParams, pkgStartPosition);
    if (NotifyActionResult(upParams, status, {SET_UPDATE_STATUS}) != UPDATE_SUCCESS) {
        LOG(ERROR) << "set status fail";
        return UPDATE_CORRUPT;
    }
    if (status != UPDATE_SUCCESS) {
        UPDATER_LAST_WORD(status, "DoInstallPackages failed");
        return status;
    }
    if (upParams.forceUpdate) {
        UPDATER_UI_INSTANCE.ShowLogRes(TR(LABEL_UPD_OK_SHUTDOWN));
    }
    if (NotifyActionResult(upParams, status, {GET_UPDATE_STATUS}) != UPDATE_SUCCESS) {
        LOG(ERROR) << "get status fail";
        return UPDATE_CORRUPT;
    }
    UPDATER_UI_INSTANCE.ShowSuccessPage();
    return status;
}

static void PostUpdatePackages(UpdaterParams &upParams, bool updateResult)
{
    std::string writeBuffer;
    std::string buf;
    std::string time;
    if (!updateResult) {
        const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
        std::ifstream fin {resultPath};
        if (!fin.is_open() || !std::getline(fin, buf)) {
            LOG(ERROR) << "read result file error " << resultPath;
            buf = "fail|";
        }
    } else {
        buf = "pass|";
        upParams.pkgLocation = upParams.pkgLocation == 0 ? upParams.pkgLocation : (upParams.pkgLocation - 1);
    }

    for (unsigned int i = 0; i < upParams.pkgLocation; i++) {
        time = DurationToString(upParams.installTime, i);
        writeBuffer += upParams.updatePackage.size() < i + 1 ? "" : upParams.updatePackage[i];
        writeBuffer += "|pass||install_time=" + time + "|\n";
    }
    time = DurationToString(upParams.installTime, upParams.pkgLocation);

    writeBuffer += upParams.updatePackage.size() < upParams.pkgLocation + 1 ? "" :
        upParams.updatePackage[upParams.pkgLocation];
    writeBuffer += "|" + buf + "|install_time=" + time + "|\n";
    for (unsigned int i = upParams.pkgLocation + 1; i < upParams.updatePackage.size(); i++) {
        writeBuffer += upParams.updatePackage[i] + "\n";
    }
    if (writeBuffer != "") {
        writeBuffer.pop_back();
    }
    LOG(INFO) << "post over, writeBuffer = " << writeBuffer;
    WriteDumpResult(writeBuffer, UPDATER_RESULT_FILE);
    DeleteInstallTimeFile();
}

static UpdaterStatus PreSdcardUpdatePackages(UpdaterParams &upParams)
{
    upParams.installTime.resize(upParams.updatePackage.size(), std::chrono::duration<double>(0));
    // verify packages first
    if (upParams.retryCount == 0 && !IsBatteryCapacitySufficient()) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(LOG_LOWPOWER));
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        LOG(ERROR) << "Battery is not sufficient for install package.";
        return UPDATE_SKIP;
    }
    UpdaterStatus status = VerifyPackages(upParams);
    if (status != UPDATE_SUCCESS) {
        return UPDATE_CORRUPT; // verify package failed must return UPDATE_CORRUPT, ux need it !!!
    }
#ifdef UPDATER_USE_PTABLE
    if (!PtablePreProcess::GetInstance().DoPtableProcess(upParams)) {
        LOG(ERROR) << "DoPtableProcess failed";
        return UPDATE_ERROR;
    }
#endif
    if (CheckVerifyPackages(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "verify packages fail";
        return UPDATE_CORRUPT;
    }
    return UPDATE_SUCCESS;
}

static void PostSdcardUpdatePackages(UpdaterParams &upParams, bool updateResult)
{
    if (Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE)) {
        PostUpdatePackages(upParams, updateResult);
    }
}

UpdaterStatus UpdaterFromSdcard(UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    upParams.callbackProgress = [] (float value) { UPDATER_UI_INSTANCE.ShowProgress(value); };
    SetMessageToMisc(upParams.miscCmd, 0, "sdcard_update");
    if (CheckSdcardPkgs(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "can not find sdcard packages";
        return UPDATE_ERROR;
    }
    UpdaterStatus status = PreSdcardUpdatePackages(upParams);
    if (status == UPDATE_SUCCESS) {
        upParams.initialProgress += VERIFY_PERCENT;
        upParams.currentPercentage -= VERIFY_PERCENT;

        STAGE(UPDATE_STAGE_BEGIN) << "UpdaterFromSdcard";
        LOG(INFO) << "UpdaterFromSdcard start, sdcard updaterPath : " << upParams.updatePackage[upParams.pkgLocation];
        UPDATER_UI_INSTANCE.ShowLog(TR(LOG_SDCARD_NOTMOVE));
        status = DoUpdatePackages(upParams);
    }
    PostSdcardUpdatePackages(upParams, status == UPDATE_SUCCESS);
    return status;
}

UpdaterStatus InstallUpdaterPackages(UpdaterParams &upParams)
{
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_PRE_UPDATE_PACKAGE_EVENT);
    UpdaterStatus status = PreUpdatePackages(upParams);
    if (status == UPDATE_SUCCESS) {
        status = DoUpdatePackages(upParams);
    }
    PostUpdatePackages(upParams, status == UPDATE_SUCCESS);
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_POST_UPDATE_PACKAGE_EVENT);
    return status;
}

UpdaterStatus StartUpdaterEntry(UpdaterParams &upParams)
{
    UpdaterStatus status = UPDATE_UNKNOWN;
    status = PreStartUpdaterEntry(upParams, status);
    if (status != UPDATE_SUCCESS) {
        LOG(ERROR) << "PreStartUpdaterEntry failed";
        return status;
    }

    status = DoUpdaterEntry(upParams);
    if (status != UPDATE_SUCCESS) {
        LOG(WARNING) << "DoUpdaterEntry failed";
    }

    status = PostStartUpdaterEntry(upParams, status);
    if (status != UPDATE_SUCCESS) {
        LOG(ERROR) << "PostStartUpdaterEntry failed";
    }
    return status;
}

UpdaterStatus DoUpdaterEntry(UpdaterParams &upParams)
{
    UpdaterStatus status = UPDATE_UNKNOWN;
    if (upParams.updateMode == SDCARD_UPDATE) {
        LOG(INFO) << "start sdcard update";
        UPDATER_UI_INSTANCE.ShowProgressPage();
        status = UpdaterFromSdcard(upParams);
        return status;
    } else if (upParams.updatePackage.size() > 0) {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        status = InstallUpdaterPackages(upParams);
    } else if (upParams.factoryResetMode == "factory_wipe_data") {
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
    } else if (upParams.factoryResetMode == "user_wipe_data" || upParams.factoryResetMode == "menu_wipe_data") {
        UPDATER_UI_INSTANCE.ShowProgressPage();
        LOG(INFO) << "User level FactoryReset begin";
        status = UPDATE_SUCCESS;
#if !defined(UPDATER_UT) && defined(UPDATER_UI_SUPPORT)
        DoProgress();
#endif
        if (FactoryReset(upParams.factoryResetMode == "user_wipe_data" ?
            USER_WIPE_DATA : MENU_WIPE_DATA, "/data") != 0) {
            LOG(ERROR) << "FactoryReset user level failed";
            status = UPDATE_ERROR;
        }
        if (status != UPDATE_SUCCESS) {
            UPDATER_UI_INSTANCE.ShowLogRes(TR(LOGRES_WIPE_FAIL));
        } else {
            UPDATER_UI_INSTANCE.ShowSuccessPage();
            UPDATER_UI_INSTANCE.ShowLogRes(TR(LOGRES_WIPE_FINISH));
            ClearUpdaterParaMisc();
            std::this_thread::sleep_for(std::chrono::milliseconds(UI_SHOW_DURATION));
        }
    }
    return status;
}

std::unordered_map<std::string, std::function<void ()>> InitOptionsFuncTab(char* &optarg,
    PackageUpdateMode &mode, UpdaterParams &upParams)
{
    std::unordered_map<std::string, std::function<void ()>> optionsFuncTab {
        {"update_package", [&]() -> void
        {
            upParams.updatePackage.push_back(optarg);
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_OTA);
            mode = HOTA_UPDATE;
        }},
        {"retry_count", [&]() -> void
        {
            upParams.retryCount = atoi(optarg);
            HwFaultRetry::GetInstance().SetRetryCount(upParams.retryCount);
        }},
        {"factory_wipe_data", [&]() -> void
        {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_REBOOTFACTORYRST);
            upParams.factoryResetMode = "factory_wipe_data";
        }},
        {"wipe_data_at_factoryreset_0", [&]() -> void
        {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_ATFACTORYRST);
            upParams.factoryResetMode = "factory_wipe_data";
        }},
        {"user_wipe_data", [&]() -> void
        {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_REBOOTFACTORYRST);
            upParams.factoryResetMode = "user_wipe_data";
        }},
        {"wipe_data_factory_lowlevel", [&]() -> void
        {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_REBOOTFACTORYRST);
            upParams.factoryResetMode = "user_wipe_data";
        }},
        {"menu_wipe_data", [&]() -> void
        {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_REBOOTFACTORYRST);
            upParams.factoryResetMode = "menu_wipe_data";
        }},
        {"upgraded_pkg_num", [&]() -> void
        {
            upParams.pkgLocation = static_cast<unsigned int>(atoi(optarg));
        }},
        {"sdcard_update", [&]() -> void
        {
            upParams.updateMode = SDCARD_UPDATE;
            upParams.sdExtMode = SDCARD_NORMAL_UPDATE;
        }},
        {"UPDATE:MAINIMG", [&]() -> void
        {
            upParams.updateMode = SDCARD_UPDATE;
            upParams.sdExtMode = SDCARD_MAINIMG;
        }},
        {"factory_sd_update", [&]() -> void
        {
            upParams.updateMode = SDCARD_UPDATE;
            upParams.sdExtMode = SDCARD_NORMAL_UPDATE;
        }},
        {"UPDATE:SD", [&]() -> void
        {
            upParams.updateMode = SDCARD_UPDATE;
            upParams.sdExtMode = SDCARD_NORMAL_UPDATE;
        }},
        {"UPDATE:SDFROMDEV", [&]() -> void
        {
            upParams.updateMode = SDCARD_UPDATE;
            upParams.sdExtMode = SDCARD_UPDATE_FROM_DEV;
        }},
        {"force_update_action", [&]() -> void
        {
            if (std::string(optarg) == POWEROFF) {
                upParams.forceUpdate = true;
            }
        }},
        {"night_update", [&]() -> void
        {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_NIGHTUPDATE);
            upParams.forceReboot = true;
        }},
        {"sdcard_intral_update", [&]() -> void
        {
            upParams.updateMode = SDCARD_UPDATE;
        }},
        {"shrink_info", [&]() -> void
        {
            upParams.shrinkInfo = std::string(optarg);
        }},
        {"virtual_shrink_info", [&]() -> void
        {
            upParams.virtualShrinkInfo = std::string(optarg);
        }}
    };
    return optionsFuncTab;
}

__attribute__((weak)) bool IsSupportOption([[maybe_unused]] const std::string &option)
{
    LOG(INFO) << "option: " << option;
    return false;
}

__attribute__((weak)) UpdaterStatus ProcessOtherOption([[maybe_unused]] const std::string &option,
    [[maybe_unused]] UpdaterParams &upParams, PackageUpdateMode &mode)
{
    return UPDATE_UNKNOWN;
}

static UpdaterStatus StartUpdater(const std::vector<std::string> &args,
    char **argv, PackageUpdateMode &mode, UpdaterParams &upParams)
{
    std::vector<char *> extractedArgs;
    int rc;
    int optionIndex;
    auto optionsFuncTab = InitOptionsFuncTab(optarg, mode, upParams);

    for (const auto &arg : args) {
        extractedArgs.push_back(const_cast<char *>(arg.c_str()));
        STAGE(UPDATE_STAGE_OUT) << "option:" << arg;
        LOG(INFO) << "option:" << arg;
    }
    extractedArgs.push_back(nullptr);
    extractedArgs.insert(extractedArgs.begin(), argv[0]);
    while ((rc = getopt_long(extractedArgs.size() - 1, extractedArgs.data(), "", OPTIONS, &optionIndex)) != -1) {
        switch (rc) {
            case 0: {
                std::string option = OPTIONS[optionIndex].name;
                if (optionsFuncTab.find(option) != optionsFuncTab.end()) {
                    auto optionsFunc = optionsFuncTab.at(option);
                    optionsFunc();
                } else if (IsSupportOption(option)) {
                    return ProcessOtherOption(option, upParams, mode);
                }
                break;
            }
            default:
                LOG(ERROR) << "Invalid argument.";
                break;
        }
    }
    optind = 1;
    if (upParams.pkgLocation == 0) {
        DeleteInstallTimeFile();
    }
    // Sanity checks
    if (upParams.updateMode == SDCARD_UPDATE) {
        (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_SDCARD);
        mode = SDCARD_UPDATE;
    }
    return StartUpdaterEntry(upParams);
}

// add updater mode
REGISTER_MODE(Updater, "updater.hdc.configfs");

__attribute__((weak)) bool IsNeedWipe(const UpdaterParams &upParams)
{
    return false;
}

void RebootAfterUpdateSuccess(const UpdaterParams &upParams)
{
    if (IsNeedWipe(upParams) ||
        upParams.sdExtMode == SDCARD_UPDATE_FROM_DEV ||
        upParams.sdExtMode == SDCARD_UPDATE_FROM_DATA) {
        NotifyReboot("updater", "Updater wipe data after upgrade success", "--user_wipe_data", upParams.sdExtMode);
        return;
    }
    upParams.forceUpdate ? Utils::DoShutdown("Updater update success go shut down") :
        NotifyReboot("", "Updater update success");
}

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
            UpdaterInit::GetInstance().InvokeEvent(UPDATER_POST_INIT_EVENT);
            if (upParams.forceReboot) {
                Utils::UsSleep(5 * DISPLAY_TIME); // 5 : 5s
                PostUpdater(true);
                NotifyReboot("", "Updater night update fail");
                return 0;
            }
        } else if (mode == SDCARD_UPDATE) {
            UPDATER_UI_INSTANCE.ShowLogRes(
                status == UPDATE_CORRUPT ? TR(LOGRES_VERIFY_FAILED) : TR(LOGRES_UPDATE_FAILED));
            UPDATER_UI_INSTANCE.ShowFailedPage();
        } else if (upParams.factoryResetMode == "user_wipe_data" ||
            upParams.factoryResetMode == "menu_wipe_data" || upParams.factoryResetMode == "factory_wipe_data") {
            UPDATER_UI_INSTANCE.ShowFailedPage();
        } else if (CheckUpdateMode(USB_UPDATE_FAIL)) {
            (void)UPDATER_UI_INSTANCE.SetMode(UPDATERMODE_USBUPDATE);
            UPDATER_UI_INSTANCE.ShowFailedPage();
        } else {
            UPDATER_UI_INSTANCE.ShowMainpage();
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
    RebootAfterUpdateSuccess(upParams);
    return 0;
}
} // Updater
