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
#include "update_processor.h"
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include "securec.h"
#include "applypatch/data_writer.h"
#include "applypatch/partition_record.h"
#include "applypatch/update_progress.h"
#include "dump.h"
#include "log.h"
#include "package/hash_data_verifier.h"
#include "pkg_manager.h"
#ifdef UPDATER_USE_PTABLE
#include "ptable_manager.h"
#endif
#include "script_instruction.h"
#include "script_manager.h"
#include "slot_info/slot_info.h"
#include "update_image_block.h"
#include "update_image_patch.h"
#include "update_partitions.h"
#include "updater_main.h"
#include "updater/updater_const.h"
#include "update_bin/bin_process.h"
#include "scope_guard.h"

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
size_t UScriptInstructionRawImageWrite::totalSize_ = 0;
size_t UScriptInstructionRawImageWrite::readSize_ = 0;
size_t UScriptInstructionUpdateFromBin::stashDataSize_ = 0;

__attribute__((weak)) int32_t GetFinalBinaryResult(int32_t result)
{
    return result;
}

UpdaterEnv::~UpdaterEnv()
{
    if (factory_ != nullptr) {
        delete factory_;
        factory_ = nullptr;
    }
}

void UpdaterEnv::PostMessage(const std::string &cmd, std::string content)
{
    if (postMessage_ != nullptr) {
        std::lock_guard<std::mutex> lock(messageLock_);
        postMessage_(cmd.c_str(), content.c_str());
    }
}

UScriptInstructionFactoryPtr UpdaterEnv::GetInstructionFactory()
{
    if (factory_ == nullptr) {
        factory_ = new UpdaterInstructionFactory();
    }
    return factory_;
}

const std::vector<std::string> UpdaterEnv::GetInstructionNames() const
{
    static std::vector<std::string> updaterCmds = {
        "sha_check", "first_block_check", "block_update",
        "raw_image_write", "update_partitions", "image_patch",
        "image_sha_check", "pkg_extract", "pkg_extract_no_ret", "update_from_bin"
    };
    return updaterCmds;
}

int32_t UpdaterInstructionFactory::CreateInstructionInstance(UScriptInstructionPtr& instr,
    const std::string& name)
{
    if (name == "sha_check") {
        instr = new UScriptInstructionShaCheck();
    } else if (name == "first_block_check") {
        instr = new UScriptInstructionBlockCheck();
    } else if (name == "block_update") {
        instr = new UScriptInstructionBlockUpdate();
    } else if (name == "raw_image_write") {
        instr = new UScriptInstructionRawImageWrite();
    } else if (name == "update_partitions") {
        instr = new UpdatePartitions();
    } else if (name == "image_patch") {
        instr = new USInstrImagePatch();
    } else if (name == "image_sha_check") {
        instr = new USInstrImageShaCheck();
    } else if (name == "pkg_extract") {
        instr = new UScriptInstructionPkgExtract();
    } else if (name == "pkg_extract_no_ret") {
        instr = new UScriptInstructionPkgExtractRetSuc();
    } else if (name == "update_from_bin") {
        instr = new UScriptInstructionBinFlowWrite();
    }
    return USCRIPT_SUCCESS;
}

int UScriptInstructionRawImageWrite::RawImageWriteProcessor(const PkgBuffer &buffer, size_t size, size_t start,
                                                            bool isFinish, const void* context)
{
    void *p = const_cast<void *>(context);
    DataWriter *writer = static_cast<DataWriter *>(p);
    if (writer == nullptr) {
        LOG(ERROR) << "Data writer is null";
        return PKG_INVALID_STREAM;
    }

    // maybe extract from package is finished. just return.
    if (buffer.buffer == nullptr || size == 0) {
        return PKG_SUCCESS;
    }

    bool ret = writer->Write(const_cast<uint8_t*>(buffer.buffer), size, nullptr);
    if (!ret) {
        LOG(ERROR) << "Write " << size << " byte(s) failed";
        return PKG_INVALID_STREAM;
    }

    if (totalSize_ != 0) {
        readSize_ += size;
        writer->GetUpdaterEnv()->PostMessage("set_progress", std::to_string((float)readSize_ / totalSize_));
    }

    return PKG_SUCCESS;
}

bool UScriptInstructionRawImageWrite::WriteRawImage(const std::string &partitionName,
    const std::unique_ptr<DataWriter> &writer, [[maybe_unused]] uint64_t partitionSize, Uscript::UScriptEnv &env)
{
    UPDATER_INIT_RECORD;
    // Extract partition information
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(partitionName);
    if (info == nullptr) {
        LOG(ERROR) << "Error to get file info";
        UPDATER_LAST_WORD(false);
        return false;
    }
    totalSize_ = info->unpackedSize;
#ifdef UPDATER_USE_PTABLE
    if (partitionSize < totalSize_) {
        LOG(ERROR) << "partition size: " << partitionSize << " is short than image size: " << totalSize_;
        UPDATER_LAST_WORD(false);
        return false;
    }
#endif

    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    int ret = env.GetPkgManager()->CreatePkgStream(outStream,
        partitionName, RawImageWriteProcessor, writer.get());
    if (ret != USCRIPT_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        UPDATER_LAST_WORD(false);
        return false;
    }

    ret = env.GetPkgManager()->ExtractFile(partitionName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        env.GetPkgManager()->ClosePkgStream(outStream);
        UPDATER_LAST_WORD(false);
        return false;
    }
    env.GetPkgManager()->ClosePkgStream(outStream);
    return true;
}

int32_t UScriptInstructionRawImageWrite::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    UPDATER_INIT_RECORD;
    std::string partitionName;
    int32_t ret = context.GetParam(0, partitionName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get partitionName";
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    if (env.IsRetry()) {
        LOG(DEBUG) << "Retry updater, check if current partition updated already during last time";
        bool isUpdated = PartitionRecord::GetInstance().IsPartitionUpdated(partitionName);
        if (isUpdated) {
            LOG(INFO) << partitionName << " already updated, skip";
            return USCRIPT_SUCCESS;
        }
    }
    LOG(INFO) << "UScriptInstructionRawImageWrite::Execute " << partitionName;
    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }

    std::string writePath;
    uint64_t offset = 0;
    uint64_t partitionSize = 0;
    if (GetWritePathAndOffset(partitionName, writePath, offset, partitionSize) != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Get partition:%s WritePathAndOffset fail \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }

    std::unique_ptr<DataWriter> writer = DataWriter::CreateDataWriter(WRITE_RAW, writePath,
        static_cast<UpdaterEnv *>(&env), offset);
    if (writer == nullptr) {
        LOG(ERROR) << "Error to create writer";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    if (!WriteRawImage(partitionName, writer, partitionSize, env)) {
        DataWriter::ReleaseDataWriter(writer);
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    PartitionRecord::GetInstance().RecordPartitionUpdateStatus(partitionName, true);
    DataWriter::ReleaseDataWriter(writer);
    totalSize_ = 0;
    readSize_ = 0;
    LOG(INFO) << "UScriptInstructionRawImageWrite finish";
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionPkgExtract::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    UPDATER_INIT_RECORD;
    std::string pkgFileName;
    int32_t ret = context.GetParam(0, pkgFileName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get pkgFileName";
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    std::string destPath;
    ret = context.GetParam(1, destPath);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get destPath";
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    LOG(INFO) << "UScriptInstructionPkgExtract::Execute " << pkgFileName;
    PkgManager::PkgManagerPtr manager = env.GetPkgManager();
    if (manager == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_INVALID_PARAM;
    }

    const FileInfo *info = manager->GetFileInfo(pkgFileName);
    if (info == nullptr) {
        LOG(ERROR) << "Error to get file info";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_INVALID_PARAM;
    }

    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    ret = manager->CreatePkgStream(outStream, destPath + "/" + pkgFileName, info->unpackedSize,
        PkgStream::PkgStreamType_Write);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to create output stream";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_ERROR_EXECUTE;
    }

    ret = manager->ExtractFile(pkgFileName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        manager->ClosePkgStream(outStream);
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_ERROR_EXECUTE;
    }

    manager->ClosePkgStream(outStream);
    LOG(INFO)<<"UScriptInstructionPkgExtract finish";
    return ret;
}

int32_t UScriptInstructionPkgExtractRetSuc::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t ret = UScriptInstructionPkgExtract::Execute(env, context);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file, ret = " << ret;
    }
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionUpdateFromBin::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    std::string upgradeFileName;
    int32_t ret = context.GetParam(0, upgradeFileName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get partitionName";
        return ret;
    }

    LOG(INFO) << "UScriptInstructionUpdateFromBin::Execute " << upgradeFileName;

    PkgManager::PkgManagerPtr pkgManager = env.GetPkgManager();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_INVALID_PARAM;
    }

    RingBuffer ringBuffer;
    if (!ringBuffer.Init(STASH_BUFFER_SIZE, BUFFER_NUM)) {
        LOG(ERROR) << "Error to get ringbuffer";
        return USCRIPT_INVALID_PARAM;
    }

    PkgManager::StreamPtr outStream = nullptr;
    ret = pkgManager->CreatePkgStream(outStream, upgradeFileName, UnCompressDataProducer, &ringBuffer);
    if (ret != USCRIPT_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_INVALID_PARAM;
    }

    ret = pkgManager->ExtractFile(upgradeFileName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract" << upgradeFileName;
        pkgManager->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    pkgManager->ClosePkgStream(outStream);
    return USCRIPT_ERROR_EXECUTE;
}

int UScriptInstructionUpdateFromBin::UnCompressDataProducer(const PkgBuffer &buffer, size_t size, size_t start,
                                                            bool isFinish, const void* context)
{
    static PkgBuffer stashBuffer(STASH_BUFFER_SIZE);
    size_t bufferStart = 0;
    void *p = const_cast<void *>(context);
    RingBuffer *ringBuffer = static_cast<RingBuffer *>(p);
    if (ringBuffer == nullptr) {
        LOG(ERROR) << "ring buffer is nullptr";
        return PKG_INVALID_STREAM;
    }

    while (stashDataSize_ + size >= STASH_BUFFER_SIZE) {
        size_t readLen = STASH_BUFFER_SIZE - stashDataSize_;
        if (memcpy_s(stashBuffer.buffer + stashDataSize_, readLen, buffer.buffer + bufferStart, readLen) != 0) {
                return USCRIPT_ERROR_EXECUTE;
        }
        ringBuffer->Push(stashBuffer.buffer, STASH_BUFFER_SIZE);
        stashDataSize_ = 0;
        size -= readLen;
        bufferStart += readLen;
    }
    if (size == 0 && stashDataSize_ == 0) {
        return PKG_SUCCESS;
    } else if (size == 0 || memcpy_s(stashBuffer.buffer + stashDataSize_, STASH_BUFFER_SIZE - stashDataSize_,
        buffer.buffer + bufferStart, size) == 0) {
        if (isFinish) {
            ringBuffer->Push(stashBuffer.buffer, stashDataSize_ + size);
            stashDataSize_ = 0;
        } else {
            stashDataSize_ += size;
        }
        return PKG_SUCCESS;
    } else {
        return USCRIPT_ERROR_EXECUTE;
    }
}

int ExecUpdate(PkgManager::PkgManagerPtr pkgManager, int retry, const std::string &pkgPath,
    PostMessageFunction postMessage)
{
    UPDATER_INIT_RECORD;
    Hpackage::HashDataVerifier scriptVerifier {pkgManager};
    if (!scriptVerifier.LoadHashDataAndPkcs7(pkgPath)) {
        LOG(ERROR) << "Fail to load hash data";
        UPDATER_LAST_WORD(EXIT_VERIFY_SCRIPT_ERROR);
        return EXIT_VERIFY_SCRIPT_ERROR;
    }
    UpdaterEnv* env = new (std::nothrow) UpdaterEnv(pkgManager, postMessage, retry);
    if (env == nullptr) {
        LOG(ERROR) << "Fail to creat env";
        UPDATER_LAST_WORD(EXIT_PARSE_SCRIPT_ERROR);
        return EXIT_PARSE_SCRIPT_ERROR;
    }
    int ret = 0;
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env, &scriptVerifier);
    if (scriptManager == nullptr) {
        LOG(ERROR) << "Fail to creat scriptManager";
        ScriptManager::ReleaseScriptManager();
        delete env;
        UPDATER_LAST_WORD(EXIT_PARSE_SCRIPT_ERROR);
        return EXIT_PARSE_SCRIPT_ERROR;
    }

    UpdaterInit::GetInstance().InvokeEvent(UPDATER_BINARY_INIT_DONE_EVENT);

    pthread_t thread;
    ret = CreateProgressThread(env, thread);
    if (ret != 0) {
        LOG(ERROR) << "Fail to create progress thread";
        ScriptManager::ReleaseScriptManager();
        delete env;
        env = nullptr;
        UPDATER_LAST_WORD(USCRIPT_ERROR_CREATE_THREAD);
        return USCRIPT_ERROR_CREATE_THREAD;
    }

    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        if (ret != USCRIPT_SUCCESS) {
            LOG(ERROR) << "Fail to execute script";
            break;
        }
    }
    SetProgressExitFlag(thread);
    ScriptManager::ReleaseScriptManager();
    delete env;
    env = nullptr;
    return GetFinalBinaryResult(ret);
}

int UScriptInstructionRawImageWrite::GetWritePathAndOffset(const std::string &partitionName, std::string &writePath,
    uint64_t &offset, uint64_t &partitionSize)
{
#ifdef UPDATER_USE_PTABLE
    DevicePtable& devicePtb = DevicePtable::GetInstance();
    Ptable::PtnInfo ptnInfo;
    if (!devicePtb.GetPartionInfoByName(partitionName, ptnInfo)) {
        LOG(ERROR) << "Datawriter: cannot find device path for partition \'" <<
                partitionName.substr(1, partitionName.size()) << "\'.";
        return USCRIPT_ERROR_EXECUTE;
    }
    writePath = ptnInfo.writePath;
    offset = ptnInfo.startAddr;
    partitionSize = ptnInfo.partitionSize;
#else
    writePath = GetBlockDeviceByMountPoint(partitionName);
    if (writePath.empty()) {
        LOG(ERROR) << "Datawriter: cannot find device path for partition \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        return USCRIPT_ERROR_EXECUTE;
    }

#ifndef UPDATER_UT
    if (partitionName != "/userdata") {
        std::string suffix = "";
        GetPartitionSuffix(suffix);
        writePath += suffix;
    }
    LOG(INFO) << "write partition path: " << writePath;
#endif
#endif
    return USCRIPT_SUCCESS;
}

int ProcessUpdater(bool retry, int pipeFd, const std::string &packagePath, const std::string &keyPath)
{
    UPDATER_INIT_RECORD;
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_BINARY_INIT_EVENT);
    Dump::GetInstance().RegisterDump("DumpHelperLog", std::make_unique<DumpHelperLog>());
    std::unique_ptr<FILE, decltype(&fclose)> pipeWrite(fdopen(pipeFd, "w"), fclose);
    if (pipeWrite == nullptr) {
        LOG(ERROR) << "Fail to fdopen, err: " << strerror(errno);
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS);
        return EXIT_INVALID_ARGS;
    }
    int ret = -1;
    Detail::ScopeGuard guard([&] {
        (void)fprintf(pipeWrite.get(), "subProcessResult:%d\n", ret);
        (void)fflush(pipeWrite.get());
    });
    // line buffered, make sure parent read per line.
    setlinebuf(pipeWrite.get());
    PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS);
        return EXIT_INVALID_ARGS;
    }

    std::vector<std::string> components;
    ret = pkgManager->LoadPackage(packagePath, keyPath, components);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "Fail to load package";
        PkgManager::ReleasePackageInstance(pkgManager);
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS);
        return EXIT_INVALID_ARGS;
    }
#ifdef UPDATER_USE_PTABLE
    DevicePtable::GetInstance().LoadPartitionInfo();
#endif

    ret = Updater::ExecUpdate(pkgManager, retry, packagePath,
        [&pipeWrite](const char *cmd, const char *content) {
            if (pipeWrite.get() != nullptr) {
                (void)fprintf(pipeWrite.get(), "%s:%s\n", cmd, content);
                (void)fflush(pipeWrite.get());
            }
        });
    PkgManager::ReleasePackageInstance(pkgManager);
#ifndef UPDATER_UT
    if (ret == 0) {
        SetActiveSlot();
    }
#endif
    return ret;
}
} // Updater