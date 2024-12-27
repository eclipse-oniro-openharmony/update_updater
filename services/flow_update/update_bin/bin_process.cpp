/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "bin_process.h"
#include <string>
#include <thread>
#include "applypatch/partition_record.h"
#include "log.h"
#include "pkg_manager_impl.h"
#include "pkg_package/pkg_pkgfile.h"
#include "pkg_utils.h"
#include "ring_buffer/ring_buffer.h"
#include "script_manager.h"
#include "threadpool/thread_pool.h"
#include "scope_guard.h"
#include "securec.h"
#include "updater/updater_const.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;

namespace Updater {
constexpr uint32_t STASH_BUFFER_SIZE = 4 * 1024 * 1024;
constexpr uint32_t MAX_BUFFER_NUM = 16;
constexpr uint8_t ES_IMAGE = 6;
constexpr uint8_t CS_IMAGE = 7;
constexpr uint8_t NEED_VERIFY_CS_IMAGE = 8;

int32_t UScriptInstructionBinFlowWrite::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    std::string upgradeFileName;
    int32_t ret = context.GetParam(0, upgradeFileName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get bin file";
        return ret;
    }

    LOG(INFO) << "UScriptInstructionUpdateFromZip::Execute " << upgradeFileName;
    PkgManager::PkgManagerPtr pkgManager = env.GetPkgManager();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_INVALID_PARAM;
    }

    RingBuffer ringBuffer;
    if (!ringBuffer.Init(STASH_BUFFER_SIZE, MAX_BUFFER_NUM)) {
        LOG(ERROR) << "Error to get ringbuffer";
        return USCRIPT_INVALID_PARAM;
    }

    fullUpdateProportion_ = GetScriptProportion();
    stashBuffer_.data.resize(STASH_BUFFER_SIZE);
    stashBuffer_.buffer = stashBuffer_.data.data();
    PkgManager::StreamPtr binFlowStream = nullptr;
    const FileInfo *info = pkgManager->GetFileInfo(upgradeFileName);
    if (info == nullptr) {
        LOG(ERROR) << "Get file info fail " << upgradeFileName;
        return PKG_INVALID_FILE;
    }
    ret = pkgManager->CreatePkgStream(binFlowStream, upgradeFileName, info->unpackedSize, &ringBuffer);
    if (ret != USCRIPT_SUCCESS || binFlowStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_INVALID_PARAM;
    }

    std::thread consumer([this, &env, &context, binFlowStream] {
        this->ProcessBinFile(env, context, binFlowStream);
        });
    std::thread producer([this, &env, &context, binFlowStream] {
        this->ExtractBinFile(env, context, binFlowStream);
        });
    consumer.join();
    producer.join();
    if (isStopRun_) {
        LOG(ERROR) << "Error to Execute bin file update";
        env.PostMessage(UPDATER_RETRY_TAG, PROCESS_BIN_FAIL_RETRY);
        return USCRIPT_ERROR_EXECUTE;
    }
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionBinFlowWrite::ExtractBinFile(Uscript::UScriptEnv &env, Uscript::UScriptContext &context,
    PkgManager::StreamPtr stream)
{
    ON_SCOPE_EXIT(failExecute) {
        isStopRun_ = true;
        stream->Stop();
    };
    std::string upgradeFileName;
    int32_t ret = context.GetParam(0, upgradeFileName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get bin file";
        return ret;
    }

    LOG(INFO) << "UScriptInstructionBinFlowWrite::ExtractBinFile " << upgradeFileName;
    PkgManager::PkgManagerPtr pkgManager = env.GetPkgManager();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_INVALID_PARAM;
    }

    PkgManager::StreamPtr processStream = nullptr;
    PkgStream::ExtractFileProcessor processor =
        [this](const PkgBuffer &buffer, size_t size, size_t start, bool isFinish, const void *context) {
            return this->UnCompressDataProducer(buffer, size, start, isFinish, context);
        };
    ret = pkgManager->CreatePkgStream(processStream, upgradeFileName, processor, stream);
    if (ret != USCRIPT_SUCCESS || processStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_INVALID_PARAM;
    }

    ret = pkgManager->ExtractFile(upgradeFileName, processStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract" << upgradeFileName;
        pkgManager->ClosePkgStream(processStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    pkgManager->ClosePkgStream(processStream);
    CANCEL_SCOPE_EXIT_GUARD(failExecute);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionBinFlowWrite::UnCompressDataProducer(const PkgBuffer &buffer, size_t size, size_t start,
                                                               bool isFinish, const void *context)
{
    if (isStopRun_) {
        LOG(ERROR) << "recive stop single, UnCompressDataProducer stop run";
        return USCRIPT_ERROR_EXECUTE;
    }

    void *p = const_cast<void *>(context);
    PkgStream *flowStream = static_cast<PkgStream *>(p);
    if (flowStream == nullptr) {
        LOG(ERROR) << "ring buffer is nullptr";
        return PKG_INVALID_STREAM;
    }

    if (buffer.buffer == nullptr && size == 0 && isFinish) {
        // 最后一块数据
        if (stashDataSize_ != 0) {
            size_t writeOffset = flowStream->GetFileLength() - stashDataSize_;
            if (flowStream->Write(stashBuffer_, stashDataSize_, writeOffset) != USCRIPT_SUCCESS) {
                LOG(ERROR) << "UnCompress flowStream write fail";
                return USCRIPT_ERROR_EXECUTE;
            }
            stashDataSize_ = 0;
        }
        LOG(INFO) << "extract finished, start";
        return USCRIPT_SUCCESS;
    }

    if (buffer.buffer == nullptr || size == 0 || start < stashDataSize_) {
        LOG(ERROR) << "invalid para, size: " << size << "start: " << start;
        return USCRIPT_ERROR_EXECUTE;
    }

    size_t writeSize = 0;
    size_t copyLen = 0;
    // 缓存4M再写入数据流
    while (size - writeSize > STASH_BUFFER_SIZE - stashDataSize_) {
        copyLen = STASH_BUFFER_SIZE - stashDataSize_;
        if (memcpy_s(stashBuffer_.buffer + stashDataSize_, copyLen, buffer.buffer + writeSize, copyLen) != EOK) {
            return USCRIPT_ERROR_EXECUTE;
        }

        if (flowStream->Write(stashBuffer_, STASH_BUFFER_SIZE, start - stashDataSize_) != USCRIPT_SUCCESS) {
            LOG(ERROR) << "UnCompress flowStream write fail";
            return USCRIPT_ERROR_EXECUTE;
        }
        writeSize += copyLen;
        stashDataSize_ = 0;
    }

    copyLen = size - writeSize;
    if (memcpy_s(stashBuffer_.buffer + stashDataSize_, copyLen, buffer.buffer + writeSize, copyLen) != EOK) {
        return USCRIPT_ERROR_EXECUTE;
    }
    stashDataSize_ += copyLen;
    if (stashDataSize_ == STASH_BUFFER_SIZE) {
        if (flowStream->Write(stashBuffer_, stashDataSize_, start - stashDataSize_ + copyLen) != USCRIPT_SUCCESS) {
            LOG(ERROR) << "UnCompress flowStream write fail";
            return USCRIPT_ERROR_EXECUTE;
        }
        stashDataSize_ = 0;
    }
    return PKG_SUCCESS;
}

int32_t UScriptInstructionBinFlowWrite::ProcessBinFile(Uscript::UScriptEnv &env, Uscript::UScriptContext &context,
                                                       PkgManager::StreamPtr stream)
{
    if (stream == nullptr) {
        LOG(ERROR) << "Error to get file stream";
        return USCRIPT_ERROR_EXECUTE;
    }
    ON_SCOPE_EXIT(failExecute) {
        isStopRun_ = true;
        stream->Stop();
    };
    std::string pkgFileName;
    int32_t ret = context.GetParam(0, pkgFileName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get pkgFileName";
        return ret;
    }

    LOG(INFO) << "UScriptInstructionBinFlowWrite::Execute " << pkgFileName;
    PkgManager::PkgManagerPtr manager = env.GetPkgManager();
    if (manager == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_INVALID_PARAM;
    }
    std::vector<std::string> innerFileNames;
    ret = manager->LoadPackageWithStream(pkgFileName, Utils::GetCertName(),
        innerFileNames, PkgFile::PKG_TYPE_UPGRADE, stream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to load flow data stream";
        return USCRIPT_ERROR_EXECUTE;
    }

    for (const auto &iter : innerFileNames) {
        // 根据镜像名称获取分区名称和大小
        std::string partitionName = iter;
        const FileInfo *info = manager->GetFileInfo(partitionName);
        if (info == nullptr) {
            LOG(ERROR) << "Error to get file info";
            return USCRIPT_ERROR_EXECUTE;
        }

        LOG(INFO) << " start process Component " << partitionName << " unpackedSize " << info->unpackedSize;
        if (ComponentProcess(env, stream, partitionName, *info) != USCRIPT_SUCCESS) {
            LOG(ERROR) << "Error to process " << partitionName;
            return USCRIPT_ERROR_EXECUTE;
        }
    }
    CANCEL_SCOPE_EXIT_GUARD(failExecute);
    LOG(INFO)<<"UScriptInstructionBinFlowWrite finish";
    return USCRIPT_SUCCESS;
}

bool UScriptInstructionBinFlowWrite::IsMatchedCsEsIamge(const Hpackage::FileInfo &fileInfo)
{
    if ((fileInfo.resType == ES_IMAGE && !Utils::IsEsDevice()) ||
        (fileInfo.resType == CS_IMAGE && Utils::IsEsDevice())) {
        LOG(INFO) << "not matched cs es image, skip write";
        return false;
    }
    return true;
}

bool UScriptInstructionBinFlowWrite::CheckEsDeviceUpdate(const Hpackage::FileInfo &fileInfo)
{
    if (fileInfo.resType == NEED_VERIFY_CS_IMAGE && Utils::IsEsDevice()) {
        LOG(ERROR) << "pkg just cs image, but device is es";
        return false;
    }
    return true;
}

int32_t UScriptInstructionBinFlowWrite::ComponentProcess(Uscript::UScriptEnv &env, PkgManager::StreamPtr stream,
                                                         const std::string &name, const Hpackage::FileInfo &fileInfo)
{
    size_t fileSize = fileInfo.unpackedSize;
    // 根据镜像名获取组件处理类名
    std::unique_ptr<ComponentProcessor> processor =
        ComponentProcessorFactory::GetInstance().GetProcessor(name, fileSize);

    if (env.IsRetry()) {
        LOG(DEBUG) << "Retry updater, check if current partition updated already during last time";
        bool isUpdated = PartitionRecord::GetInstance().IsPartitionUpdated(name);
        if (isUpdated) {
            LOG(INFO) << name << " already updated, skip";
            processor.reset();
            processor = std::make_unique<SkipImgProcessor>(name, fileSize);
        }
    }

    if (!CheckEsDeviceUpdate(fileInfo)) {
        LOG(ERROR) << "pkg just cs image, es device not allow update";
        return USCRIPT_ERROR_EXECUTE;
    }

    if ((processor == nullptr && fileInfo.resType == UPGRADE_FILE_COMP_OTHER_TPYE) ||
        !IsMatchedCsEsIamge(fileInfo)) {
        LOG(INFO) << name << " comp is not register and comp file is not image, or not match cs es image, skip";
        processor.reset();
        processor = std::make_unique<SkipImgProcessor>(name, fileSize);
    }

    if (processor == nullptr) {
        LOG(ERROR) << "GetProcessor failed, partition name: " << name;
        return USCRIPT_ERROR_EXECUTE;
    }
    processor->SetPkgFileInfo(stream->GetReadOffset(), stream->GetFileLength(), fullUpdateProportion_);
    LOG(INFO) << "component read offset " << stream->GetReadOffset();

    if (processor->PreProcess(env) != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to PreProcess " << name;
        return USCRIPT_ERROR_EXECUTE;
    }

    if (processor->DoProcess(env) != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to DoProcess " << name;
        return USCRIPT_ERROR_EXECUTE;
    }

    if (processor->PostProcess(env) != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to PostProcess " << name;
        return USCRIPT_ERROR_EXECUTE;
    }

    return USCRIPT_SUCCESS;
}
} // namespace Updater
