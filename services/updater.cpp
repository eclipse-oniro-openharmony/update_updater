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
#include "updater/updater_preprocess.h"
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
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "ExtractUpdaterBinary create stream fail";
        UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT;
    }
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

UpdaterStatus IsSpaceCapacitySufficient(const std::vector<std::string> &packagePath)
{
    uint64_t totalPkgSize = 0;
    for (auto path : packagePath) {
        PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
        if (pkgManager == nullptr) {
            LOG(ERROR) << "pkgManager is nullptr";
            return UPDATE_CORRUPT;
        }
        std::vector<std::string> fileIds;
        int ret = pkgManager->LoadPackageWithoutUnPack(path, fileIds);
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
        totalPkgSize += static_cast<uint64_t>(info->unpackedSize + MAX_LOG_SPACE);
    }

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

    if (static_cast<uint64_t>(updaterVfs.f_bfree) * static_cast<uint64_t>(updaterVfs.f_bsize) <= totalPkgSize) {
        LOG(ERROR) << "Can not update, free space is not enough";
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SPACE_NOTENOUGH), true);
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

int GetTmpProgressValue()
{
    return g_tmpProgressValue;
}

void ProgressSmoothHandler(int beginProgress, int endProgress)
{
#ifdef UPDATER_UI_SUPPORT
    if (endProgress < 0 || endProgress > FULL_PERCENT_PROGRESS || beginProgress < 0) {
        return;
    }
    while (beginProgress < endProgress) {
        int increase = (endProgress - beginProgress) / PROGRESS_VALUE_CONST;
        beginProgress += increase;
        if (beginProgress >= endProgress || increase == 0) {
            break;
        } else {
            UPDATER_UI_INSTANCE.ShowProgress(beginProgress);
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

UpdaterStatus DoInstallUpdaterPackage(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams,
    PackageUpdateMode updateMode)
{
    UPDATER_INIT_RECORD;
    UPDATER_UI_INSTANCE.ShowProgressPage();
    UPDATER_UI_INSTANCE.ShowProgress(upParams.initialProgress * FULL_PERCENT_PROGRESS);
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "Fail to GetPackageInstance", UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT);
    UPDATER_CHECK_ONLY_RETURN(SetupPartitions(updateMode) == 0,
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);

    if (upParams.retryCount > 0) {
        LOG(INFO) << "Retry for " << upParams.retryCount << " time(s)";
    } else {
        pkgManager = PkgManager::GetPackageInstance();
    }
    int32_t verifyret = GetUpdatePackageInfo(pkgManager, upParams.updatePackage[upParams.pkgLocation]);
    g_tmpProgressValue = 0;
    UPDATER_ERROR_CHECK(verifyret == PKG_SUCCESS, "Verify package bin file Fail...",
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_VERIFYFAIL), true);
        return UPDATE_CORRUPT);
    LOG(INFO) << "Package bin file verified. start to install package...";
    int32_t versionRet = PreProcess::GetInstance().DoUpdatePreProcess(pkgManager);
    UPDATER_ERROR_CHECK(versionRet == PKG_SUCCESS, "Version Check Fail...", return UPDATE_CORRUPT);

#ifdef UPDATER_USE_PTABLE
    if (!PtableProcess(pkgManager, updateMode)) {
        LOG(ERROR) << "Ptable process failed!";
        return UPDATE_CORRUPT;
    }
#endif

    int maxTemperature;
    UpdaterStatus updateRet = StartUpdaterProc(pkgManager, upParams, maxTemperature);
    if (updateRet != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_INSTALL_FAIL));
        LOG(ERROR) << "Install package failed.";
    }
    return updateRet;
}

namespace {
#ifdef UPDATER_UI_SUPPORT
void SetProgress(const std::vector<std::string> &output, UpdaterParams &upParams)
{
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
            upParams.currentPercentage + upParams.initialProgress * FULL_PERCENT_PROGRESS);
        return;
    }
    g_tmpProgressValue = tmpProgressValue + g_tmpValue;
    if (g_tmpProgressValue == 0) {
        return;
    }
    UPDATER_UI_INSTANCE.ShowProgress(g_tmpProgressValue *
        upParams.currentPercentage + upParams.initialProgress * FULL_PERCENT_PROGRESS);
}
#endif

void HandleChildOutput(const std::string &buffer, int32_t bufferLen, bool &retryUpdate, UpdaterParams &upParams)
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
        SetProgress(output, upParams);
    } else {
        LOG(WARNING) << "Child process returns unexpected message.";
#endif
    }
}
}

UpdaterStatus StartUpdaterProc(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams, int &maxTemperature)
{
    UPDATER_INIT_RECORD;
    int pfd[DEFAULT_PIPE_NUM]; /* communication between parent and child */
    UPDATER_FILE_CHECK(pipe(pfd) >= 0, "Create pipe failed: ", UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "Fail to GetPackageInstance", UPDATER_LAST_WORD(UPDATE_CORRUPT);
        return UPDATE_CORRUPT);
    int pipeRead = pfd[0];
    int pipeWrite = pfd[1];
    std::string fullPath = GetWorkPath() + std::string(UPDATER_BINARY);
    (void)Utils::DeleteFile(fullPath);
    if (ExtractUpdaterBinary(pkgManager, UPDATER_BINARY) != 0) {
        LOG(INFO) << "There is no updater_binary in package, use updater_binary in device";
        fullPath = "/bin/updater_binary";
    }
    pid_t pid = fork();
    UPDATER_CHECK_ONLY_RETURN(pid >= 0, ERROR_CODE(CODE_FORK_FAIL);
        UPDATER_LAST_WORD(UPDATE_ERROR);
        return UPDATE_ERROR);
    if (pid == 0) { // child
        close(pipeRead);   // close read endpoint
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
        if (upParams.retryCount > 0) {
            execl(fullPath.c_str(), upParams.updatePackage[upParams.pkgLocation].c_str(),
                std::to_string(pipeWrite).c_str(), "retry", nullptr);
        } else {
            execl(fullPath.c_str(), upParams.updatePackage[upParams.pkgLocation].c_str(),
                std::to_string(pipeWrite).c_str(), nullptr);
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
        HandleChildOutput(buffer, MAX_BUFFER_SIZE, retryUpdate, upParams);
    }
    fclose(fromChild);

    int status;
    pid_t w = waitpid(pid, &status, 0);
    if (w == -1) {
        LOG(ERROR) << "waitpid error";
        return UPDATE_ERROR;
    }
    UPDATER_CHECK_ONLY_RETURN(!retryUpdate, return UPDATE_RETRY);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (WIFEXITED(status)) {
            LOG(ERROR) << "exited, status= " << WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            LOG(ERROR) << "killed by signal " << WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            LOG(ERROR) << "stopped by signal " << WSTOPSIG(status);
        }
        return UPDATE_ERROR;
    }
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
