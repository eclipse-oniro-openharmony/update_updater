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
#include <algorithm>
#include "fs_manager/mount.h"
#include "language/language_ui.h"
#include "log/dump.h"
#include "log/log.h"
#include "package/hash_data_verifier.h"
#include "package/pkg_manager.h"
#include "package/packages_info.h"
#include "parameter.h"
#include "misc_info/misc_info.h"
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#include "selinux/selinux.h"
#endif // WITH_SELINUX
#ifdef UPDATER_USE_PTABLE
#include "ptable_parse/ptable_manager.h"
#endif
#include "scope_guard.h"
#include "updater/hwfault_retry.h"
#include "updater/updater_preprocess.h"
#include "updater/updater_const.h"
#include "updater_main.h"
#include "updater_ui_stub.h"
#include "utils.h"
#include "write_state/write_state.h"

namespace Updater {
using Updater::Utils::SplitString;
using Updater::Utils::Trim;
using namespace Hpackage;

int g_percentage = 100;
int g_tmpProgressValue;
int g_tmpValue;

int32_t ExtractUpdaterBinary(PkgManager::PkgManagerPtr manager, std::string &packagePath,
    const std::string &updaterBinary)
{
    UPDATER_INIT_RECORD;
    PkgManager::StreamPtr outStream = nullptr;
    int32_t ret = manager->CreatePkgStream(outStream,  GetWorkPath() + updaterBinary,
        0, PkgStream::PkgStreamType_Write);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "ExtractUpdaterBinary create stream fail";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "ExtractUpdaterBinary create stream fail");
        return UPDATE_CORRUPT;
    }
    ret = manager->ExtractFile(updaterBinary, outStream);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "ExtractUpdaterBinary extract file failed";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "ExtractUpdaterBinary extract file failed");
        return UPDATE_CORRUPT;
    }
    HashDataVerifier verifier {manager};
    if (!verifier.LoadHashDataAndPkcs7(packagePath) ||
        !verifier.VerifyHashData("build_tools/", updaterBinary, outStream)) {
        LOG(ERROR) << "verify updater_binary failed";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "verify updater_binary failed");
        return UPDATE_CORRUPT;
    }
    manager->ClosePkgStream(outStream);
    return UPDATE_SUCCESS;
}

int GetUpdatePackageInfo(PkgManager::PkgManagerPtr pkgManager, const std::string &path)
{
    std::vector<std::string> components;
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return UPDATE_CORRUPT;
    }
    int32_t ret = pkgManager->LoadPackage(path, Utils::GetCertName(), components);
    if (ret != PKG_SUCCESS) {
        LOG(INFO) << "LoadPackage fail ret :"<< ret;
        return ret;
    }
    return PKG_SUCCESS;
}

UpdaterStatus IsSpaceCapacitySufficient(const UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    std::vector<uint64_t> stashSizeList = GetStashSizeList(upParams);
    if (stashSizeList.size() == 0) {
        LOG(ERROR) << "get stash size error";
        UPDATER_LAST_WORD(UPDATE_ERROR, "get stash size error");
        return UPDATE_ERROR;
    }
    uint64_t maxStashSize =  *max_element(stashSizeList.begin(), stashSizeList.end());
    LOG(INFO) << "get max stash size: " << maxStashSize;
    uint64_t totalPkgSize = maxStashSize + MIN_UPDATE_SPACE;
    LOG(INFO) << "needed totalPkgSize = " << totalPkgSize;
    if (CheckStatvfs(totalPkgSize) != UPDATE_SUCCESS) {
        LOG(ERROR) << "CheckStatvfs error";
        UPDATER_LAST_WORD(UPDATE_ERROR, "CheckStatvfs error");
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

static bool ConvertStrToLongLong(PkgManager::StreamPtr outStream, int64_t &value)
{
    UPDATER_INIT_RECORD;
    PkgBuffer data {};
    if (outStream == nullptr) {
        LOG(ERROR) << "outStream is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "outStream is nullptr");
        return false;
    }
    outStream->GetBuffer(data);
    if (data.buffer == nullptr) {
        LOG(ERROR) << "data.buffer is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "data.buffer is nullptr");
        return false;
    }
    std::string str(reinterpret_cast<char*>(data.buffer), data.length);
    if (!Utils::ConvertToLongLong(str, value)) {
        LOG(ERROR) << "ConvertToLongLong failed";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "ConvertToLongLong failed");
        return false;
    }
    return true;
}

std::vector<uint64_t> GetStashSizeList(const UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    const std::string maxStashFileName = "all_max_stash";
    std::vector<uint64_t> stashSizeList;
    for (unsigned int i = upParams.pkgLocation; i < upParams.updatePackage.size(); i++) {
        PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
        if (pkgManager == nullptr) {
            LOG(ERROR) << "pkgManager is nullptr";
            UPDATER_LAST_WORD(UPDATE_CORRUPT, "pkgManager is nullptr");
            return std::vector<uint64_t> {};
        }
        ON_SCOPE_EXIT(releasePackage) {
            PkgManager::ReleasePackageInstance(pkgManager);
        };

        std::vector<std::string> fileIds;
        if (pkgManager->LoadPackageWithoutUnPack(upParams.updatePackage[i], fileIds) != PKG_SUCCESS) {
            LOG(ERROR) << "LoadPackageWithoutUnPack failed " << upParams.updatePackage[i];
            UPDATER_LAST_WORD(UPDATE_CORRUPT, "LoadPackageWithoutUnPack failed");
            return std::vector<uint64_t> {};
        }

        const FileInfo *info = pkgManager->GetFileInfo(maxStashFileName);
        if (info == nullptr) {
            LOG(INFO) << "all_max_stash not exist " << upParams.updatePackage[i];
            stashSizeList.push_back(0);
            continue;
        }

        PkgManager::StreamPtr outStream = nullptr;
        int ret = pkgManager->CreatePkgStream(outStream, maxStashFileName, info->unpackedSize,
            PkgStream::PkgStreamType_MemoryMap);
        if (outStream == nullptr || ret != PKG_SUCCESS) {
            LOG(ERROR) << "Create stream fail " << maxStashFileName << " in " << upParams.updatePackage[i];
            UPDATER_LAST_WORD(UPDATE_CORRUPT, "CreatePkgStream failed");
            return std::vector<uint64_t> {};
        }

        if (pkgManager->ExtractFile(maxStashFileName, outStream) != PKG_SUCCESS) {
            LOG(ERROR) << "ExtractFile fail " << maxStashFileName << " in " << upParams.updatePackage[i];
            UPDATER_LAST_WORD(UPDATE_CORRUPT, "ExtractFile failed");
            return std::vector<uint64_t> {};
        }
        int64_t maxStashSize = 0;
        if (!ConvertStrToLongLong(outStream, maxStashSize)) {
            UPDATER_LAST_WORD(UPDATE_CORRUPT, "ConvertStrToLongLong failed");
            return std::vector<uint64_t> {};
        }
        stashSizeList.push_back(static_cast<uint64_t>(maxStashSize));
    }
    return stashSizeList;
}

int CheckStatvfs(const uint64_t totalPkgSize)
{
    UPDATER_INIT_RECORD;
    struct statvfs64 updaterVfs;
    if (access("/sdcard/updater", 0) == 0) {
        if (statvfs64("/sdcard", &updaterVfs) < 0) {
            LOG(ERROR) << "Statvfs read /sdcard error!";
            UPDATER_LAST_WORD(UPDATE_ERROR, "Statvfs read /sdcard error!");
            return UPDATE_ERROR;
        }
    } else {
        if (statvfs64("/data", &updaterVfs) < 0) {
            LOG(ERROR) << "Statvfs read /data error!";
            UPDATER_LAST_WORD(UPDATE_ERROR, "Statvfs read /data error!");
            return UPDATE_ERROR;
        }
    }
    LOG(INFO) << "Number of free blocks = " << updaterVfs.f_bfree << ", Number of free inodes = " << updaterVfs.f_ffree;
    if (static_cast<uint64_t>(updaterVfs.f_bfree) * static_cast<uint64_t>(updaterVfs.f_bsize) <= totalPkgSize) {
        LOG(ERROR) << "Can not update, free space is not enough";
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SPACE_NOTENOUGH), true);
        UPDATER_UI_INSTANCE.Sleep(UI_SHOW_DURATION);
        UPDATER_LAST_WORD(UPDATE_ERROR, "Can not update, free space is not enough");
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

int GetTmpProgressValue()
{
    return g_tmpProgressValue;
}

void SetTmpProgressValue(int value)
{
    g_tmpProgressValue = value;
}

void ProgressSmoothHandler(int beginProgress, int endProgress)
{
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
}

__attribute__((weak)) bool PreStartBinaryEntry([[maybe_unused]] const std::string &path)
{
    LOG(INFO) << "pre binary process";
    return true;
}

bool IsUpdateBasePkg(UpdaterParams &upParams)
{
    for (auto pkgPath : upParams.updatePackage) {
        if (pkgPath.find("_base") != std::string::npos) {
            LOG(INFO) << "this update include base pkg";
            return true;
        }
    }
    return false;
}
 
UpdaterStatus SetUpdateSlotParam(UpdaterParams &upParams, bool isUpdateCurrSlot)
{
    if (!Utils::IsVabDevice()) {
        return UPDATE_SUCCESS;
    }
    if (!isUpdateCurrSlot && !IsUpdateBasePkg(upParams)) {
        isUpdateCurrSlot = true;
    }
    int currentSlot = GetCurrentSlot();
    if (currentSlot < 1 || currentSlot > 2) { // 2 : max slot
        LOG(ERROR) << "GetCurrentSlot failed";
        return UPDATE_ERROR;
    }
    bool isUpdateSlotA = (currentSlot == SLOT_A && isUpdateCurrSlot) ||
        (currentSlot == SLOT_B && !isUpdateCurrSlot);
    int updateSlot = isUpdateSlotA ? SLOT_A : SLOT_B;
    if (!Utils::SetUpdateSlot(updateSlot)) {
        LOG(ERROR) << "set update.part.slot fail";
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}
 
UpdaterStatus ClearUpdateSlotParam()
{
    if (!Utils::IsVabDevice()) {
        return UPDATE_SUCCESS;
    }
    int updateSlot = -10; // -10 : default value
    if (!Utils::SetUpdateSlot(updateSlot)) {
        LOG(ERROR) << "clear update.part.slot fail";
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

UpdaterStatus DoInstallUpdaterBinfile(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams,
    PackageUpdateMode updateMode)
{
    UPDATER_INIT_RECORD;
    UPDATER_UI_INSTANCE.ShowProgressPage();
    if (upParams.callbackProgress == nullptr) {
        LOG(ERROR) << "CallbackProgress is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "CallbackProgress is nullptr");
        return UPDATE_CORRUPT;
    }
    upParams.callbackProgress(upParams.initialProgress * FULL_PERCENT_PROGRESS);
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "pkgManager is nullptr");
        return UPDATE_CORRUPT;
    }

    if (SetupPartitions(updateMode != SDCARD_UPDATE || upParams.sdExtMode == SDCARD_UPDATE_FROM_DEV ||
        upParams.sdExtMode == SDCARD_UPDATE_FROM_DATA || Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE) ||
        Utils::CheckUpdateMode(Updater::FACTORY_INTERNAL_MODE),
        Utils::IsVabDevice() && updateMode == HOTA_UPDATE) != 0) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
        UPDATER_LAST_WORD(UPDATE_ERROR, "SetupPartitions failed");
        return UPDATE_ERROR;
    }

    if (upParams.retryCount > 0) {
        LOG(INFO) << "Retry for " << upParams.retryCount << " time(s)";
    }

    // 获取zip信息
    int ret = GetUpdatePackageInfo(pkgManager, STREAM_ZIP_PATH);
    if (ret != 0) {
        LOG(ERROR) << "get update package info fail";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "GetUpdatePackageInfo failed");
        return UPDATE_CORRUPT;
    }
    if (!PreStartBinaryEntry(upParams.updateBin[upParams.pkgLocation])) {
        LOG(ERROR) << "pre binary process failed";
        UPDATER_LAST_WORD(UPDATE_ERROR, "PreStartBinaryEntry failed");
        return UPDATE_ERROR;
    }

    g_tmpProgressValue = 0;
    // 从bin文件开启进程
    UpdaterStatus updateRet = StartUpdaterProc(pkgManager, upParams);
    if (updateRet != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_INSTALL_FAIL));
        UPDATER_LAST_WORD(updateRet, "StartUpdaterProc failed");
        LOG(ERROR) << "Install package failed.";
    }
    if (WriteResult(upParams.updateBin[upParams.pkgLocation],
        updateRet == UPDATE_SUCCESS ? "verify_success" : "verify_fail") != UPDATE_SUCCESS) {
        LOG(ERROR) << "write update state fail";
    }
    return updateRet;
}

UpdaterStatus DoInstallUpdaterPackage(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams,
    PackageUpdateMode updateMode)
{
    UPDATER_INIT_RECORD;
    UPDATER_UI_INSTANCE.ShowProgressPage();
    if (upParams.callbackProgress == nullptr) {
        LOG(ERROR) << "CallbackProgress is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "CallbackProgress is nullptr");
        return UPDATE_CORRUPT;
    }
    upParams.callbackProgress(upParams.initialProgress * FULL_PERCENT_PROGRESS);
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "pkgManager is nullptr");
        return UPDATE_CORRUPT;
    }

    if (SetupPartitions(updateMode != SDCARD_UPDATE || upParams.sdExtMode == SDCARD_UPDATE_FROM_DEV ||
        upParams.sdExtMode == SDCARD_UPDATE_FROM_DATA || Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE) ||
        Utils::CheckUpdateMode(Updater::FACTORY_INTERNAL_MODE),
        Utils::IsVabDevice() && updateMode == HOTA_UPDATE) != 0) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_SETPART_FAIL), true);
        UPDATER_LAST_WORD(UPDATE_ERROR, "SetupPartitions failed");
        return UPDATE_ERROR;
    }

    if (upParams.retryCount > 0) {
        LOG(INFO) << "Retry for " << upParams.retryCount << " time(s)";
    }
    int ret = GetUpdatePackageInfo(pkgManager, upParams.updatePackage[upParams.pkgLocation]);
    if (ret != 0) {
        LOG(ERROR) << "get update package info fail";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "GetUpdatePackageInfo failed");
        return UPDATE_CORRUPT;
    }
    if (!PreStartBinaryEntry(upParams.updatePackage[upParams.pkgLocation])) {
        LOG(ERROR) << "pre binary process failed";
        UPDATER_LAST_WORD(UPDATE_ERROR, "PreStartBinaryEntry failed");
        return UPDATE_ERROR;
    }

    g_tmpProgressValue = 0;
    UpdaterStatus updateRet = StartUpdaterProc(pkgManager, upParams);
    if (updateRet != UPDATE_SUCCESS) {
        UPDATER_UI_INSTANCE.ShowUpdInfo(TR(UPD_INSTALL_FAIL));
        UPDATER_LAST_WORD(updateRet, "StartUpdaterProc failed");
        LOG(ERROR) << "Install package failed.";
    }
    if (WriteResult(upParams.updatePackage[upParams.pkgLocation],
        updateRet == UPDATE_SUCCESS ? "verify_success" : "verify_fail") != UPDATE_SUCCESS) {
        LOG(ERROR) << "write update state fail";
    }
    return updateRet;
}

namespace {
void SetProgress(const std::vector<std::string> &output, UpdaterParams &upParams)
{
    if (upParams.callbackProgress == nullptr) {
        LOG(ERROR) << "CallbackProgress is nullptr";
        return;
    }
    if (output.size() < DEFAULT_PROCESS_NUM) {
        LOG(ERROR) << "check output fail";
        return;
    }
    auto outputInfo = Trim(output[1]);
    float frac = 0;
    if (!Utils::ConvertToFloat(output[1], frac)) {
        LOG(ERROR) << "ConvertToFloat failed";
        return;
    }
    int tmpProgressValue = 0;
    if (frac >= -EPSINON && frac <= EPSINON) {
        return;
    } else {
        tmpProgressValue = static_cast<int>(frac * g_percentage);
    }
    if (frac >= FULL_EPSINON && g_tmpValue + g_percentage < FULL_PERCENT_PROGRESS) {
        g_tmpValue += g_percentage;
        g_tmpProgressValue = g_tmpValue;
        upParams.callbackProgress(g_tmpProgressValue *
            upParams.currentPercentage + upParams.initialProgress * FULL_PERCENT_PROGRESS);
        return;
    }
    g_tmpProgressValue = tmpProgressValue + g_tmpValue;
    if (g_tmpProgressValue == 0) {
        return;
    }
    upParams.callbackProgress(g_tmpProgressValue *
        upParams.currentPercentage + upParams.initialProgress * FULL_PERCENT_PROGRESS);
}
}

void HandleChildOutput(const std::string &buffer, int32_t bufferLen, bool &retryUpdate, UpdaterParams &upParams)
{
    if (bufferLen == 0) {
        return;
    }
    std::string str = buffer;
    std::vector<std::string> output = SplitString(str, ":");
    if (output.size() < DEFAULT_PROCESS_NUM) {
        LOG(ERROR) << "check output fail";
        return;
    }
    auto outputHeader = Trim(output[0]);
    if (outputHeader == "write_log") {
        auto outputInfo = Trim(output[1]);
        LOG(INFO) << outputInfo;
    } else if (outputHeader == "retry_update") {
        retryUpdate = true;
        auto outputInfo = Trim(output[1]);
        HwFaultRetry::GetInstance().SetFaultInfo(outputInfo);
    } else if (outputHeader == "ui_log") {
        auto outputInfo = Trim(output[1]);
    } else if (outputHeader == "show_progress") {
        g_tmpValue = g_tmpProgressValue;
        auto outputInfo = Trim(output[1]);
        float frac;
        std::vector<std::string> progress = SplitString(outputInfo, ",");
        if (progress.size() != DEFAULT_PROCESS_NUM) {
            LOG(ERROR) << "show progress with wrong arguments";
        } else {
            if (!Utils::ConvertToFloat(progress[0], frac)) {
                LOG(ERROR) << "ConvertToFloat failed";
                return;
            }
            g_percentage = static_cast<int>(frac * FULL_PERCENT_PROGRESS);
        }
    } else if (outputHeader == "set_progress") {
        SetProgress(output, upParams);
    } else {
        LOG(WARNING) << "Child process returns unexpected message.";
    }
}

void ExcuteSubProc(const UpdaterParams &upParams, const std::string &fullPath, int pipeWrite)
{
    UPDATER_INIT_RECORD;
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
    const std::string retryPara = upParams.retryCount > 0 ? "retry=1" : "retry=0";
    if (upParams.updateBin.size() > 0) {
        LOG(INFO) << "Binary Path:" << upParams.updateBin[upParams.pkgLocation].c_str();
        execl(fullPath.c_str(), fullPath.c_str(), upParams.updateBin[upParams.pkgLocation].c_str(),
            std::to_string(pipeWrite).c_str(), retryPara.c_str(), nullptr);
    } else if (upParams.updatePackage.size() > 0) {
        LOG(INFO) << "Binary Path:" << upParams.updatePackage[upParams.pkgLocation].c_str();
        execl(fullPath.c_str(), fullPath.c_str(), upParams.updatePackage[upParams.pkgLocation].c_str(),
            std::to_string(pipeWrite).c_str(), retryPara.c_str(), nullptr);
    }
    LOG(ERROR) << "Execute updater binary failed";
    UPDATER_LAST_WORD(UPDATE_ERROR, "Execute updater binary failed");
    exit(-1);
}

UpdaterStatus HandlePipeMsg(UpdaterParams &upParams, int pipeRead, bool &retryUpdate)
{
    UPDATER_INIT_RECORD;
    char buffer[MAX_BUFFER_SIZE] = {0};
    FILE* fromChild = fdopen(pipeRead, "r");
    if (fromChild == nullptr) {
        LOG(ERROR) << "fdopen pipeRead failed";
        UPDATER_LAST_WORD(UPDATE_ERROR, "fdopen pipeRead failed");
        return UPDATE_ERROR;
    }
    while (fgets(buffer, MAX_BUFFER_SIZE - 1, fromChild) != nullptr) {
        char *pch = strrchr(buffer, '\n');
        if (pch != nullptr) {
            *pch = '\0';
        }
        if (strstr(buffer, "subProcessResult") != nullptr) {
            LOG(INFO) << "subProcessResult: " << buffer;
            break;
        }
        HandleChildOutput(buffer, MAX_BUFFER_SIZE, retryUpdate, upParams);
    }
    LOG(INFO) << "HandlePipeMsg end";
    fclose(fromChild);
    return UPDATE_SUCCESS;
}

UpdaterStatus CheckProcStatus(UpdaterParams &upParams, bool retryUpdate)
{
    int status;
    ON_SCOPE_EXIT(resetBinaryPid) {
        upParams.binaryPid = -1;
    };
    if (waitpid(upParams.binaryPid, &status, 0) == -1) {
        LOG(ERROR) << "waitpid error";
        return UPDATE_ERROR;
    }
    if (retryUpdate) {
        return UPDATE_RETRY;
    }

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

static std::string GetBinaryPathFromBin(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams)
{
    std::string fullPath = GetWorkPath() + std::string(UPDATER_BINARY);
    (void)Utils::DeleteFile(fullPath);
    std::string toolPath = "/data/updater/update_stream.zip";
    if (ExtractUpdaterBinary(pkgManager, toolPath, UPDATER_BINARY) != 0) {
        LOG(INFO) << "There is no valid updater_binary in package, use updater_binary in device";
        fullPath = "/bin/updater_binary";
    }

#ifdef UPDATER_UT
    fullPath = "/data/updater/updater_binary";
#endif
    return fullPath;
}

static std::string GetBinaryPath(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams)
{
    std::string fullPath = GetWorkPath() + std::string(UPDATER_BINARY);
    (void)Utils::DeleteFile(fullPath);
    if (access("/data/updater/rollback", F_OK) == 0) {
        LOG(INFO) << "There is rollback, use updater_binary in device";
        fullPath = "/bin/updater_binary";
    } else if (ExtractUpdaterBinary(pkgManager, upParams.updatePackage[upParams.pkgLocation], UPDATER_BINARY) != 0) {
        LOG(INFO) << "There is no valid updater_binary in package, use updater_binary in device";
        fullPath = "/bin/updater_binary";
    }

#ifdef UPDATER_UT
    fullPath = "/data/updater/updater_binary";
#endif
    return fullPath;
}

UpdaterStatus StartUpdaterProc(PkgManager::PkgManagerPtr pkgManager, UpdaterParams &upParams)
{
    UPDATER_INIT_RECORD;
    int pfd[DEFAULT_PIPE_NUM]; /* communication between parent and child */
    if (pipe(pfd) < 0) {
        LOG(ERROR) << "Create pipe failed: ";
        UPDATER_LAST_WORD(UPDATE_ERROR, "Create pipe failed");
        return UPDATE_ERROR;
    }
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        UPDATER_LAST_WORD(UPDATE_CORRUPT, "pkgManager is nullptr");
        return UPDATE_CORRUPT;
    }

    int pipeRead = pfd[0];
    int pipeWrite = pfd[1];
    std::string fullPath = "";
    if (upParams.updateBin.size() > 0) {
        fullPath = GetBinaryPathFromBin(pkgManager, upParams);
    } else if (upParams.updatePackage.size() > 0) {
        fullPath = GetBinaryPath(pkgManager, upParams);
    }
    if (chmod(fullPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
        LOG(ERROR) << "Failed to change mode";
    }

#ifdef WITH_SELINUX
    Restorecon(fullPath.c_str());
#endif // WITH_SELINUX

    pid_t pid = fork();
    if (pid < 0) {
        ERROR_CODE(CODE_FORK_FAIL);
        upParams.binaryPid = -1;
        UPDATER_LAST_WORD(UPDATE_ERROR, "fork failed");
        return UPDATE_ERROR;
    }

    if (pid == 0) { // child
        #ifdef WITH_SELINUX
        setcon("u:r:updater_binary:s0");
        #endif // WITH_SELINUX
        close(pipeRead);   // close read endpoint
        ExcuteSubProc(upParams, fullPath, pipeWrite);
    }

    upParams.binaryPid = pid;
    close(pipeWrite); // close write endpoint
    bool retryUpdate = false;
    if (HandlePipeMsg(upParams, pipeRead, retryUpdate) != UPDATE_SUCCESS) {
        UPDATER_LAST_WORD(UPDATE_ERROR, "HandlePipeMsg failed");
        return UPDATE_ERROR;
    }

    return CheckProcStatus(upParams, retryUpdate);
}

std::string GetWorkPath()
{
    if (Utils::IsUpdaterMode()) {
        return G_WORK_PATH;
    }

    return std::string(SYS_INSTALLER_PATH) + "/";
}
} // namespace Updater
