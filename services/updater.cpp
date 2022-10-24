/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "include/updater/updater.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <string>
#include <sched.h>
#include <syscall.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "fs_manager/mount.h"
#include "language/language_ui.h"
#include "log/dump.h"
#include "log/log.h"
#include "package/pkg_manager.h"
#include "package/packages_info.h"
#include "parameter.h"
#include "misc_info/misc_info.h"
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#endif // WITH_SELINUX
#ifdef UPDATER_USE_PTABLE
#include "ptable_parse/ptable_manager.h"
#endif
#include "updater/updater_const.h"
#include "updater_main.h"
#include "updater_ui_stub.h"
#include "utils.h"

namespace Updater {
using Updater::Utils::SplitString;
using Updater::Utils::Trim;
using namespace Hpackage;

int g_percentage;
int g_tmpProgressValue;
int g_tmpValue;

namespace {
int32_t ExtractUpdaterBinary(PkgManager::PkgManagerPtr manager, const std::string &updaterBinary)
{
    PkgManager::StreamPtr outStream = nullptr;
    int32_t ret = manager->CreatePkgStream(outStream,  GetWorkPath() + updaterBinary,
        0, PkgStream::PkgStreamType_Write);
    UPDATER_ERROR_CHECK(ret == PKG_SUCCESS, "ExtractUpdaterBinary create stream fail",
        UPDATER_LAST_WORD(UPDATE_CORRUPT); return UPDATE_CORRUPT);
    ret = manager->ExtractFile(updaterBinary, outStream);
    manager->ClosePkgStream(outStream);
    return ret;
}
}

int GetUpdatePackageInfo(PkgManager::PkgManagerPtr pkgManager, const std::string &path)
{
    std::vector<std::string> components;
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Fail to GetPackageInstance";
        return UPDATE_CORRUPT;
    }
    int32_t ret = pkgManager->LoadPackage(path, Utils::GetCertName(), components);
    if (ret != PKG_SUCCESS) {
        LOG(INFO) << "LoadPackage fail ret :"<< ret;
        return ret;
    }
    return PKG_SUCCESS;
}

int OtaUpdatePreCheck(PkgManager::PkgManagerPtr pkgManager, const std::string &packagePath)
{
    UPDATER_INIT_RECORD;
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Fail to GetPackageInstance";
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return UPDATE_CORRUPT;
    }
    char realPath[PATH_MAX + 1] = {0};
    if (realpath(packagePath.c_str(), realPath) == nullptr) {
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

int UpdatePreProcess(PkgManager::PkgManagerPtr pkgManager, const std::string &path)
{
    int ret = -1;
    if (pkgManager == nullptr) {
        return PKG_INVALID_VERSION;
    }
    PackagesInfoPtr pkginfomanager = PackagesInfo::GetPackagesInfoInstance();
    const char *verPtr = GetDisplayVersion();
    if ((pkginfomanager == nullptr) || (verPtr == nullptr)) {
        LOG(ERROR) << "Fail to GetPackageInstance";
        return PKG_INVALID_VERSION;
    }
    std::string verStr(verPtr);
    LOG(INFO) << "current version:" << verStr;
    std::vector<std::string> targetVersions = pkginfomanager->GetOTAVersion(pkgManager, "/version_list", GetWorkPath());
    for (size_t i = 0; i < targetVersions.size(); i++) {
        ret = verStr.compare(targetVersions[i]);
        if (ret == 0) {
            break;
        }
    }
    // check broad info
    if (ret != 0) {
        PackagesInfo::ReleasePackagesInfoInstance(pkginfomanager);
        return ret;
    }
    LOG(WARNING) << "Check version success ";
    std::string localBoardId = Utils::GetLocalBoardId();
    if (localBoardId.empty()) {
        PackagesInfo::ReleasePackagesInfoInstance(pkginfomanager);
        return 0;
    }
    std::vector<std::string> boardIdList = pkginfomanager->GetBoardID(pkgManager, "/board_list", "");
    for (size_t i = 0; i < boardIdList.size(); i++) {
        ret = localBoardId.compare(boardIdList[i]);
        if (ret == 0) {
            LOG(WARNING) << "Check board list success ";
            break;
        }
    }
    PackagesInfo::ReleasePackagesInfoInstance(pkginfomanager);
    return ret;
}

UpdaterStatus IsSpaceCapacitySufficient(const std::string &packagePath)
{
    PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return UPDATE_CORRUPT;
    }
    std::vector<std::string> fileIds;
    int ret = pkgManager->LoadPackageWithoutUnPack(packagePath, fileIds);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "LoadPackageWithoutUnPack failed";
        PkgManager::ReleasePackageInstance(pkgManager);
        return UPDATE_CORRUPT;
    }

    const FileInfo *info = pkgManager->GetFileInfo("update.bin");
    if (info == nullptr) {
        LOG(ERROR) << "update.bin is not exist";
        PkgManager::ReleasePackageInstance(pkgManager);
        return UPDATE_CORRUPT;
    }
    PkgManager::ReleasePackageInstance(pkgManager);

    struct statvfs64 updaterVfs;
    if (access("/sdcard/updater", 0) == 0) {
        if (statvfs64("/sdcard", &updaterVfs) < 0) {
            LOG(ERROR) << "Statvfs read /sdcard error!";
            return UPDATE_ERROR;
        }
    } else {
        if (statvfs64("/data", &updaterVfs) < 0) {
            LOG(ERROR) << "Statvfs read /data error!";
            return UPDATE_ERROR;
        }
    }

    auto freeSpaceSize = static_cast<uint64_t>(updaterVfs.f_bfree);
    auto blockSize = static_cast<uint64_t>(updaterVfs.f_bsize);
    uint64_t totalFreeSize = freeSpaceSize * blockSize;
    if (totalFreeSize <= static_cast<uint64_t>(info->unpackedSize + MAX_LOG_SPACE)) {
        LOG(ERROR) << "Can not update, free space is not enough";
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SPACE_NOTENOUGH), true);
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

void ProgressSmoothHandler()
{
#ifdef UPDATER_UI_SUPPORT
    while (g_tmpProgressValue < FULL_PERCENT_PROGRESS) {
        int increase = (FULL_PERCENT_PROGRESS - g_tmpProgressValue) / PROGRESS_VALUE_CONST;
        g_tmpProgressValue += increase;
        if (g_tmpProgressValue >= FULL_PERCENT_PROGRESS || increase == 0) {
            break;
        } else {
            UPDATER_UI_INSTANCE.ShowProgress(g_tmpProgressValue);
            UPDATER_UI_INSTANCE.Sleep(SHOW_FULL_PROGRESS_TIME);
        }
    }
#endif
}

#ifdef UPDATER_USE_PTABLE
bool PtableProcess(PkgManager::PkgManagerPtr pkgManager, PackageUpdateMode updateMode)
{
    DevicePtable& devicePtb = DevicePtable::GetInstance();
    devicePtb.LoadPartitionInfo();
    PackagePtable& packagePtb = PackagePtable::GetInstance();
    packagePtb.LoadPartitionInfo(pkgManager);
    if (!devicePtb.ComparePtable(packagePtb)) {
        LOG(INFO) << "Ptable NOT changed, no need to process!";
        return true;
    }
    if (updateMode == HOTA_UPDATE) {
        if (devicePtb.ComparePartition(packagePtb, "USERDATA")) {
            LOG(ERROR) << "Hota update not allow userdata partition change!";
            return false;
        }
    }
    if (!packagePtb.WritePtableToDevice()) {
        LOG(ERROR) << "Ptable changed, write new ptable failed!";
        return false;
    }
    return true;
}
#endif

bool GetUpdaterPackagePath(std::vector<std::string> &args)
{
    UpdateMessage boot {};
    bool readMiscResult = ReadUpdaterMiscMsg(boot);
    if (!readMiscResult) {
        LOG(ERROR) << "read misc failed";
        return readMiscResult;
    }
    std::vector<std::string> miscInfo = SplitString(boot.update, "\n");
    if (miscInfo.size() < 2) { // 2 : the message before path
        LOG(ERROR) << "SplitString update failed";
        return false;
    }
    std::vector<std::string> allPkgPath = SplitString(miscInfo[1], ", =");
    if (allPkgPath.size() < 1) {
        LOG(ERROR) << "SplitString miscInfo failed";
        return false;
    }
    for(int32_t i = 1; i < allPkgPath.size(); i++) {
        args.push_back(allPkgPath[i]);
    }
    return true;
}

UpdaterStatus ShowHistoricalProgress(const std::string &packagePath)
{
    int32_t i = 0;
    size_t allPkgSize = 0;
    size_t nowSize = 0;
    bool tmp = false;
    std::vector<std::string> args;
    bool getPathRet = GetUpdaterPackagePath(args);
    if (!getPathRet) {
        LOG(ERROR) << "GetUpdaterPackagePath failed";
        return UPDATE_ERROR;
    }
    for(i = 0; i < args.size(); i++) {
        char realPath[PATH_MAX + 1] = {0};
        if(realpath(args[i].c_str(),realPath) == nullptr) {
            continue;
        }
        if (packagePath == args[i]) {
            tmp = true;
        }
        std::ifstream fin(realPath);
        if(fin.is_open()) {
            fin.seekg(0, std::ios::end);
            allPkgSize += fin.tellg();
            if (!tmp) {
                nowSize += fin.tellg();
            }
            fin.close();
        }
    }
    UPDATER_UI_INSTANCE.ShowProgress(static_cast<float>(nowSize) / static_cast<float>(allPkgSize) * 100);
    return UPDATE_SUCCESS;
}

UpdaterStatus DoInstallUpdaterPackage(PkgManager::PkgManagerPtr pkgManager, const std::string &packagePath,
    int retryCount, PackageUpdateMode updateMode)
{
    UPDATER_INIT_RECORD;
    UPDATER_UI_INSTANCE.ShowProgressPage();
    if (updateMode == HOTA_UPDATE) {
        UpdaterStatus ret = ShowHistoricalProgress(packagePath);
        if (ret != UPDATE_SUCCESS) {
            LOG(ERROR) << "ShowHistoricalProgress failed";
            return ret;
        }
    }
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "Fail to GetPackageInstance", UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT);
    UPDATER_CHECK_ONLY_RETURN(SetupPartitions(updateMode) == 0,
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);

    LOG(INFO) << "Verify package...";
    UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKG));

    UPDATER_ERROR_CHECK(access(packagePath.c_str(), 0) == 0, "package is not exist",
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_NOPKG), true);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);

    int32_t verifyret = OtaUpdatePreCheck(pkgManager, packagePath);
    UPDATER_ERROR_CHECK(verifyret == PKG_SUCCESS, "package verify failed",
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYPKGFAIL), true);
        UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT);

    if (retryCount > 0) {
        LOG(INFO) << "Retry for " << retryCount << " time(s)";
    } else {
        UpdaterStatus ret = IsSpaceCapacitySufficient(packagePath);
        // Only handle UPATE_ERROR and UPDATE_SUCCESS here.
        // If it returns UPDATE_CORRUPT, which means something wrong with package manager.
        // Let package verify handle this.
        if (ret == UPDATE_ERROR) {
            return ret;
        } else if (ret == UPDATE_SUCCESS) {
            pkgManager = PkgManager::GetPackageInstance();
        }
    }

    verifyret = GetUpdatePackageInfo(pkgManager, packagePath);
    UPDATER_ERROR_CHECK(verifyret == PKG_SUCCESS, "Verify package Fail...",
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYFAIL), true);
        return UPDATE_CORRUPT);
    LOG(INFO) << "Package verified. start to install package...";
    int32_t versionRet = UpdatePreProcess(pkgManager, packagePath);
    UPDATER_ERROR_CHECK(versionRet == PKG_SUCCESS, "Version Check Fail...", return UPDATE_CORRUPT);

#ifdef UPDATER_USE_PTABLE
    if (!PtableProcess(pkgManager, updateMode)) {
        LOG(ERROR) << "Ptable process failed!";
        return UPDATE_CORRUPT;
    }
#endif

    int maxTemperature;
    UpdaterStatus updateRet = StartUpdaterProc(pkgManager, packagePath, retryCount, maxTemperature);
    if (updateRet != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_INSTALL_FAIL));
        LOG(ERROR) << "Install package failed.";
    }
    return updateRet;
}

namespace {
#ifdef UPDATER_UI_SUPPORT
void SetProgress(const std::vector<std::string> &output, const std::string &packagePath)
{
    int i = 0;
    size_t allPkgSize = 0;
    std::vector<std::string> args;
    bool getPathRet = GetUpdaterPackagePath(args);
    if (!getPathRet) {
        LOG(ERROR) << "GetUpdaterPackagePath faile";
        return;
    }
    std::vector<size_t> nowSize;
    float nowPersent = 0.0;
    int pkgLocation = -1;
    for(i = 0; i < args.size(); i++) {
        char realPath[PATH_MAX + 1] = {0};
        if(realpath(args[i].c_str(),realPath) == nullptr) {
            continue;
        }
        if (packagePath == args[i]) {
            pkgLocation = 1;
        }
        std::ifstream fin(realPath);
        if(fin.is_open()) {
            fin.seekg(0, std::ios::end);
            nowSize.push_back(fin.tellg());
            allPkgSize += nowSize[i];
            fin.close();
        }
    }
    for(i = 0; i < nowSize.size(); i++) {
        if (i == pkgLocation) {
            break;
        }
        nowPersent += static_cast<float>(nowSize[i]) / static_cast<float>(allPkgSize);
    }
    if (output.size() < DEFAULT_PROCESS_NUM) {
        LOG(ERROR) << "check output fail";
        return;
    }
    auto outputInfo = Trim(output[1]);
    float frac = std::stof(output[1]);
    int tmpProgressValue = 0;
    if (frac >= -EPSINON && frac <= EPSINON) {
        return;
    } else {
        tmpProgressValue = static_cast<int>(frac * g_percentage);
    }
    if (frac >= FULL_EPSINON && g_tmpValue + g_percentage < FULL_PERCENT_PROGRESS) {
        g_tmpValue += g_percentage;
        g_tmpProgressValue = g_tmpValue;
        UPDATER_UI_INSTANCE.ShowProgress(g_tmpProgressValue *
            (static_cast<float>(nowSize[pkgLocation]) / static_cast<float>(allPkgSize)) + nowPersent * 100);
        return;
    }
    g_tmpProgressValue = tmpProgressValue + g_tmpValue;
    if (g_tmpProgressValue == 0) {
        return;
    }
    UPDATER_UI_INSTANCE.ShowProgress(g_tmpProgressValue *
        (static_cast<float>(nowSize[pkgLocation]) / static_cast<float>(allPkgSize)) + nowPersent * 100);
}
#endif

void HandleChildOutput(const std::string &buffer, int32_t bufferLen, bool &retryUpdate,
    const std::string &packagePath)
{
    if (bufferLen == 0) {
        return;
    }
    std::string str = buffer;
    std::vector<std::string> output = SplitString(str, ":");
    if (output.size() < 1) {
        LOG(ERROR) << "check output fail";
        return;
    }
    auto outputHeader = Trim(output[0]);
    if (outputHeader == "write_log") {
        if (output.size() < DEFAULT_PROCESS_NUM) {
            LOG(ERROR) << "check output fail";
            return;
        }
        auto outputInfo = Trim(output[1]);
        LOG(INFO) << outputInfo;
    } else if (outputHeader == "retry_update") {
        retryUpdate = true;
#ifdef UPDATER_UI_SUPPORT
    } else if (outputHeader == "ui_log") {
        if (output.size() < DEFAULT_PROCESS_NUM) {
            LOG(ERROR) << "check output fail";
            return;
        }
        auto outputInfo = Trim(output[1]);
    } else if (outputHeader == "show_progress") {
        if (output.size() < DEFAULT_PROCESS_NUM) {
            LOG(ERROR) << "check output fail";
            return;
        }
        g_tmpValue = g_tmpProgressValue;
        auto outputInfo = Trim(output[1]);
        float frac;
        std::vector<std::string> progress = SplitString(outputInfo, ",");
        if (progress.size() != DEFAULT_PROCESS_NUM) {
            LOG(ERROR) << "show progress with wrong arguments";
        } else {
            UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_INSTALL_START));
            frac = std::stof(progress[0]);
            g_percentage = static_cast<int>(frac * FULL_PERCENT_PROGRESS);
        }
    } else if (outputHeader == "set_progress") {
        SetProgress(output, packagePath);
    } else {
        LOG(WARNING) << "Child process returns unexpected message.";
#endif
    }
}
}

UpdaterStatus StartUpdaterProc(PkgManager::PkgManagerPtr pkgManager, const std::string &packagePath,
    int retryCount, int &maxTemperature)
{
    UPDATER_INIT_RECORD;
    int pfd[DEFAULT_PIPE_NUM]; /* communication between parent and child */
    UPDATER_FILE_CHECK(pipe(pfd) >= 0, "Create pipe failed: ", UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "Fail to GetPackageInstance", UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT);
    int pipeRead = pfd[0];
    int pipeWrite = pfd[1];

    UPDATER_ERROR_CHECK(ExtractUpdaterBinary(pkgManager, UPDATER_BINARY) == 0,
        "Updater: cannot extract updater binary from update package.", UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT);
    g_tmpProgressValue = 0;
    pid_t pid = fork();
    UPDATER_CHECK_ONLY_RETURN(pid >= 0, ERROR_CODE(CODE_FORK_FAIL);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);
    if (pid == 0) { // child
        close(pipeRead);   // close read endpoint
        std::string fullPath = GetWorkPath() + std::string(UPDATER_BINARY);
#ifdef UPDATER_UT
        if (packagePath.find("updater_binary_abnormal") != std::string::npos) {
            fullPath = "/system/bin/updater_binary_abnormal";
        } else {
            fullPath = "/system/bin/test_update_binary";
        }
#endif
        UPDATER_ERROR_CHECK_NOT_RETURN(chmod(fullPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0,
            "Failed to change mode");

#ifdef WITH_SELINUX
        Restorecon(fullPath.c_str());
#endif // WITH_SELINUX

        // Set process scheduler to normal if current scheduler is
        // SCHED_FIFO, which may cause bad performance.
        int policy = syscall(SYS_sched_getscheduler, getpid());
        if (policy == -1) {
            LOG(INFO) << "Cannnot get current process scheduler";
        } else if (policy == SCHED_FIFO) {
            LOG(DEBUG) << "Current process with scheduler SCHED_FIFO";
            struct sched_param sp = {
                .sched_priority = 0,
            };
            if (syscall(SYS_sched_setscheduler, getpid(), SCHED_OTHER, &sp) < 0) {
                LOG(WARNING) << "Cannot set current process schedule with SCHED_OTHER";
            }
        }
        if (retryCount > 0) {
            execl(fullPath.c_str(), packagePath.c_str(), std::to_string(pipeWrite).c_str(), "retry", nullptr);
        } else {
            execl(fullPath.c_str(), packagePath.c_str(), std::to_string(pipeWrite).c_str(), nullptr);
        }
        LOG(INFO) << "Execute updater binary failed";
        UPDATER_LAST_WORD(UPDATE_ERROR);
        exit(-1);
    }

    close(pipeWrite); // close write endpoint
    char buffer[MAX_BUFFER_SIZE] = {0};
    bool retryUpdate = false;
    FILE* fromChild = fdopen(pipeRead, "r");
    UPDATER_ERROR_CHECK(fromChild != nullptr, "fdopen pipeRead failed", return UPDATE_ERROR);
    while (fgets(buffer, MAX_BUFFER_SIZE - 1, fromChild) != nullptr) {
        char *pch = strrchr(buffer, '\n');
        if (pch != nullptr) {
            *pch = '\0';
        }
        HandleChildOutput(buffer, MAX_BUFFER_SIZE, retryUpdate, packagePath);
    }
    fclose(fromChild);

    int status;
    waitpid(pid, &status, 0);
    UPDATER_CHECK_ONLY_RETURN(!retryUpdate, return UPDATE_RETRY);
    UPDATER_ERROR_CHECK(!(!WIFEXITED(status) || WEXITSTATUS(status) != 0),
        "Updater process exit with status: " << WEXITSTATUS(status), return UPDATE_ERROR);
    LOG(DEBUG) << "Updater process finished.";
    return UPDATE_SUCCESS;
}

std::string GetWorkPath()
{
    if (Utils::IsUpdaterMode()) {
        return G_WORK_PATH;
    }

    return std::string(UPDATER_PATH) + "/";
}
} // namespace Updater
