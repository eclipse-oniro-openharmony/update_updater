/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "update_image_patch.h"
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "applypatch/block_set.h"
#include "applypatch/store.h"
#include "applypatch/transfer_manager.h"
#include "applypatch/partition_record.h"
#include "diffpatch/diffpatch.h"
#include "dump.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "patch/update_patch.h"
#include "updater/updater_const.h"
#include "utils.h"

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
constexpr uint32_t IMAGE_PATCH_CMD_LEN = 6;
constexpr uint32_t IMAGE_PATCH_CHECK_CMD_LEN = 5;

int32_t USInstrImagePatch::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t result = ExecuteImagePatch(env, context);
    context.PushParam(result);
    return result;
}

int32_t USInstrImagePatch::GetParam(Uscript::UScriptContext &context, ImagePatchPara &para)
{
    if (context.GetParamCount() != IMAGE_PATCH_CMD_LEN) {
        LOG(ERROR) << "para count error " << context.GetParamCount();
        return USCRIPT_INVALID_PARAM;
    }

    int index = 0;
    uint32_t ret = static_cast<uint32_t>(context.GetParam(index++, para.partName));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.srcSize));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.srcHash));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.destSize));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.destHash));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.patchFile));
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "para get error";
        return USCRIPT_INVALID_PARAM;
    }
    para.devPath = GetBlockDeviceByMountPoint(para.partName);
    if (para.devPath.empty()) {
        LOG(ERROR) << "get " << para.partName << " dev path error";
        return USCRIPT_ERROR_EXECUTE;
    }
    return USCRIPT_SUCCESS;
}

std::string USInstrImagePatch::GetFileHash(const std::string &file)
{
    UpdatePatch::MemMapInfo mapBuffer {};
    if (PatchMapFile(file, mapBuffer) != UpdatePatch::PATCH_SUCCESS) {
        LOG(ERROR) << "PatchMapFile error";
        return "";
    }
    UpdatePatch::BlockBuffer data = { mapBuffer.memory, mapBuffer.length };
    std::string resultSha = UpdatePatch::GeneraterBufferHash(data);
    std::transform(resultSha.begin(), resultSha.end(), resultSha.begin(), ::toupper);
    return resultSha;
}

int32_t USInstrImagePatch::ApplyPatch(const ImagePatchPara &para, const std::string &patchFile)
{
    std::string newFile = UPDATER_PATH + para.partName + ".new";
    int32_t ret = UpdatePatch::UpdateApplyPatch::ApplyPatch(patchFile, para.devPath, newFile);
    if (ret != UpdatePatch::PATCH_SUCCESS) {
        UPDATER_LAST_WORD(ret);
        LOG(ERROR) << "ApplyPatch error:" << ret;
        return ret;
    }

    std::string resultSha = GetFileHash(newFile);
    if (resultSha != para.destHash) {
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        LOG(ERROR) << "apply patch fail resultSha:" << resultSha << " destHash:" << para.destHash;
        return USCRIPT_ERROR_EXECUTE;
    }

    if (!Utils::CopyFile(newFile, para.devPath)) {
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        LOG(ERROR) << "write " << newFile << " to " << para.devPath << " failed";
        return USCRIPT_ERROR_EXECUTE;
    }
    unlink(newFile.c_str());
    return USCRIPT_SUCCESS;
}

std::string USInstrImagePatch::GetPatchFile(Uscript::UScriptEnv &env, const ImagePatchPara &para)
{
    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return "";
    }

    const FileInfo *info = env.GetPkgManager()->GetFileInfo(para.partName);
    if (info == nullptr) {
        LOG(ERROR) << "Error to get file info";
        return "";
    }

    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    std::string patchFile = UPDATER_PATH + para.partName;
    int32_t ret = env.GetPkgManager()->CreatePkgStream(outStream,
        patchFile, info->unpackedSize, PkgStream::PkgStreamType_Write);
    if (ret != PKG_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return "";
    }

    ret = env.GetPkgManager()->ExtractFile(para.partName, outStream);
    env.GetPkgManager()->ClosePkgStream(outStream);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        return "";
    }

    LOG(INFO) << "USInstrImageShaCheck::Execute patchFile " << patchFile;
    return patchFile;
}

std::string USInstrImagePatch::GetSourceFile(const ImagePatchPara &para)
{
    // Back up partitions to prevent power failures during the upgrade.
    std::string srcFile = UPDATER_PATH + para.partName + ".backup";

    if (access(srcFile.c_str(), F_OK) == 0 && GetFileHash(srcFile) != para.srcHash) {
        LOG(INFO) << "using backup file:" << srcFile;
        return srcFile;
    }

    if (!Utils::CopyFile(para.devPath, srcFile)) {
        LOG(ERROR) << "copy " << para.devPath << " to " << srcFile << " failed";
        return "";
    }
    return srcFile;
}

int32_t USInstrImagePatch::ExecuteImagePatch(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    ImagePatchPara para {};
    int32_t ret = GetParam(context, para);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(ret);
        LOG(ERROR) << "GetParam error";
        return ret;
    }

    if (env.IsRetry()) {
        LOG(DEBUG) << "Retry updater, check if current partition updatered already during last time";
        if (PartitionRecord::GetInstance().IsPartitionUpdated(para.partName)) {
            LOG(INFO) << para.partName << " already updated, skip";
            return USCRIPT_SUCCESS;
        }
    }

    std::string patchFile = GetPatchFile(env, para);
    if (patchFile.empty()) {
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        LOG(ERROR) << "get patch file error";
        return USCRIPT_ERROR_EXECUTE;
    }

    std::string srcFile = GetSourceFile(para);
    if (srcFile.empty()) {
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        LOG(ERROR) << "get source file error";
        return USCRIPT_ERROR_EXECUTE;
    }

    ret = ApplyPatch(para, patchFile);
    if (ret == USCRIPT_SUCCESS) {
        PartitionRecord::GetInstance().RecordPartitionUpdateStatus(para.partName, true);
    }
    unlink(patchFile.c_str());
    unlink(srcFile.c_str());
    LOG(INFO) << "USInstrImageCheck::Execute ret:" << ret;
    return ret;
}

int32_t USInstrImageShaCheck::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t result = ExecuteShaCheck(env, context);
    context.PushParam(result);
    return result;
}

int32_t USInstrImageShaCheck::GetParam(Uscript::UScriptContext &context, CheckPara &para)
{
    if (context.GetParamCount() != IMAGE_PATCH_CHECK_CMD_LEN) {
        LOG(ERROR) << "para count error " << context.GetParamCount();
        return USCRIPT_INVALID_PARAM;
    }
    int index = 0;
    uint32_t ret = static_cast<uint32_t>(context.GetParam(index++, para.partName));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.srcSize));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.srcHash));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.destSize));
    ret |= static_cast<uint32_t>(context.GetParam(index++, para.destHash));
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "para get error";
        return USCRIPT_INVALID_PARAM;
    }

    para.devPath = GetBlockDeviceByMountPoint(para.partName);
    if (para.devPath.empty()) {
        LOG(ERROR) << "cannot get block device of partition" << para.partName;
        return USCRIPT_ERROR_EXECUTE;
    }
    LOG(INFO) << "dev path: " << para.devPath;
    return USCRIPT_SUCCESS;
}

int32_t USInstrImageShaCheck::CheckHash(const CheckPara &para)
{
    UpdatePatch::MemMapInfo mapBuffer {};
    if (PatchMapFile(para.devPath, mapBuffer) != UpdatePatch::PATCH_SUCCESS) {
        LOG(ERROR) << "PatchMapFile error";
        return USCRIPT_ERROR_EXECUTE;
    }

    UpdatePatch::BlockBuffer data = { mapBuffer.memory, mapBuffer.length };
    std::string resultSha = UpdatePatch::GeneraterBufferHash(data);
    std::transform(resultSha.begin(), resultSha.end(), resultSha.begin(), ::toupper);
    if (resultSha != para.srcHash) {
        LOG(ERROR) << "resultSha:" << resultSha << " srcHash:" << para.srcHash;
        return USCRIPT_INVALID_PARAM;
    }
    return USCRIPT_SUCCESS;
}

int32_t USInstrImageShaCheck::ExecuteShaCheck(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    if (env.IsRetry()) {
        return USCRIPT_SUCCESS;
    }

    CheckPara para {};
    int32_t ret = GetParam(context, para);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(ret);
        LOG(ERROR) << "GetParam error";
        return ret;
    }

    ret = CheckHash(para);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(ret);
        LOG(ERROR) << "CheckHash error";
        return ret;
    }

    LOG(INFO) << "USInstrImageCheck::Execute Success";
    return USCRIPT_SUCCESS;
}
}

