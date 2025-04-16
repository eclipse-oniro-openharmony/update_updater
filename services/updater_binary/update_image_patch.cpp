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
#include <memory>
#include <vector>
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
#include "updater/hwfault_retry.h"
#include "utils.h"
#include "slot_info/slot_info.h"

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
#ifndef UPDATER_UT
    if (para.partName != "/userdata") {
        std::string suffix = "";
        GetPartitionSuffix(suffix);
        para.devPath += suffix;
    }
#else
    para.devPath = "/data/updater" + para.partName;
#endif
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

int32_t USInstrImagePatch::ApplyPatch(const ImagePatchPara &para, const UpdatePatch::MemMapInfo &srcData,
    const PkgBuffer &patchData)
{
    std::vector<uint8_t> empty;
    UpdatePatch::PatchParam patchParam = {
        srcData.memory, srcData.length, patchData.buffer, patchData.length
    };
    std::unique_ptr<DataWriter> writer = DataWriter::CreateDataWriter(WRITE_RAW, para.devPath);
    if (writer.get() == nullptr) {
        LOG(ERROR) << "Cannot create block writer, pkgdiff patch abort!";
        return -1;
    }
    std::string resultSha = para.destHash;
    std::transform(resultSha.begin(), resultSha.end(), resultSha.begin(), ::tolower);
    int32_t ret = UpdatePatch::UpdateApplyPatch::ApplyImagePatch(patchParam, empty,
        [&](size_t start, const UpdatePatch::BlockBuffer &data, size_t size) -> int {
            return (writer->Write(data.buffer, size, nullptr)) ? 0 : -1;
        }, resultSha);
    writer.reset();
    if (ret != 0) {
        LOG(ERROR) << "Fail to ApplyImagePatch";
        return -1;
    }
    return USCRIPT_SUCCESS;
}

int32_t USInstrImagePatch::CreatePatchStream(Uscript::UScriptEnv &env, const ImagePatchPara &para,
    PkgManager::StreamPtr &patchStream)
{
    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return -1;
    }

    std::string patchName = para.patchFile;
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(patchName);
    if (info == nullptr) {
        LOG(WARNING) << "Error to get file info " << para.patchFile; // 兼容旧升级包
        patchName = para.partName;
        info = env.GetPkgManager()->GetFileInfo(patchName);
        if (info == nullptr) {
            return -1;
        }
    }

    std::string patchFile = UPDATER_PATH + para.patchFile;
    int32_t ret = env.GetPkgManager()->CreatePkgStream(patchStream,
        patchFile, info->unpackedSize, PkgStream::PkgStreamType_MemoryMap);
    if (ret != PKG_SUCCESS || patchStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return -1;
    }

    ret = env.GetPkgManager()->ExtractFile(patchName, patchStream);
    if (ret != PKG_SUCCESS) {
        env.GetPkgManager()->ClosePkgStream(patchStream);
        LOG(ERROR) << "Error to extract file " << para.patchFile;
        return -1;
    }

    LOG(INFO) << "USInstrImagePatch::CreatePatchStream " << para.partName;
    return USCRIPT_SUCCESS;
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
    UPDATER_INIT_RECORD;
    ImagePatchPara para {};
    int32_t ret = GetParam(context, para);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(ret, "GetParam error");
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

    std::string srcFile = GetSourceFile(para);
    if (srcFile.empty()) {
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE, "get source file error");
        LOG(ERROR) << "get source file error";
        return USCRIPT_ERROR_EXECUTE;
    }
    UpdatePatch::MemMapInfo srcData {};
    ret = UpdatePatch::PatchMapFile(srcFile, srcData);
    if (ret != 0) {
        UPDATER_LAST_WORD(ret, "Failed to mmap src file error");
        LOG(ERROR) << "Failed to mmap src file error:" << ret;
        return -1;
    }

    PkgManager::StreamPtr patchStream = nullptr;
    ret = CreatePatchStream(env, para, patchStream);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE, "CreatePatchStream error");
        LOG(ERROR) << "CreatePatchStream error";
        return USCRIPT_ERROR_EXECUTE;
    }
    PkgBuffer patchData = {};
    patchStream->GetBuffer(patchData);

    ret = ApplyPatch(para, srcData, patchData);
    if (ret != USCRIPT_SUCCESS) {
        env.GetPkgManager()->ClosePkgStream(patchStream);
        return ret;
    }

    PartitionRecord::GetInstance().RecordPartitionUpdateStatus(para.partName, true);
    env.GetPkgManager()->ClosePkgStream(patchStream);
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
#ifndef UPDATER_UT
    if (para.partName != "/userdata") {
        std::string suffix = "";
        GetPartitionSuffix(suffix);
        para.devPath += suffix;
    }
#else
    para.devPath = "/data/updater" + para.partName;
#endif
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
    if (!std::all_of(para.srcSize.begin(), para.srcSize.end(), ::isdigit)) {
        LOG(ERROR) << "para size error " << para.srcSize;
        return USCRIPT_ERROR_EXECUTE;
    }
    uint32_t length = 0;
    if (!Utils::ConvertToUnsignedLong(para.srcSize, length)) {
        LOG(ERROR) << "ConvertToUnsignedLong error";
        return USCRIPT_ERROR_EXECUTE;
    }
    UpdatePatch::BlockBuffer data = { mapBuffer.memory, length };
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
    UPDATER_INIT_RECORD;
    if (env.IsRetry() && !Utils::CheckFaultInfo(VERIFY_FAILED_REBOOT)) {
        return USCRIPT_SUCCESS;
    }

    CheckPara para {};
    int32_t ret = GetParam(context, para);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(ret, "GetParam error");
        LOG(ERROR) << "GetParam error";
        return ret;
    }

    ret = CheckHash(para);
    if (ret != USCRIPT_SUCCESS) {
        UPDATER_LAST_WORD(ret, "CheckHash error");
        env.PostMessage(UPDATER_RETRY_TAG, VERIFY_FAILED_REBOOT);
        LOG(ERROR) << "CheckHash error";
        return ret;
    }

    LOG(INFO) << "USInstrImageCheck::Execute Success";
    return USCRIPT_SUCCESS;
}
}

