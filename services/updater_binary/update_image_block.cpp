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
#include "update_image_block.h"
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "applypatch/block_set.h"
#include "applypatch/store.h"
#include "applypatch/transfer_manager.h"
#include "applypatch/partition_record.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "utils.h"

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
constexpr int32_t SHA_CHECK_SECOND = 2;
constexpr int32_t SHA_CHECK_PARAMS = 3;
static int ExtractNewData(const PkgBuffer &buffer, size_t size, size_t start, bool isFinish, const void* context)
{
    void *p = const_cast<void *>(context);
    WriterThreadInfo *info = static_cast<WriterThreadInfo *>(p);
    uint8_t *addr = buffer.buffer;
    while (size > 0) {
        pthread_mutex_lock(&info->mutex);
        while (info->writer == nullptr) {
            if (!info->readyToWrite) {
                LOG(WARNING) << "writer is not ready to write.";
                pthread_mutex_unlock(&info->mutex);
                return Hpackage::PKG_INVALID_STREAM;
            }
            pthread_cond_wait(&info->cond, &info->mutex);
        }
        pthread_mutex_unlock(&info->mutex);
        size_t toWrite = std::min(size, info->writer->GetBlocksSize() - info->writer->GetTotalWritten());
        // No more data to write.
        if (toWrite == 0) {
            break;
        }
        bool ret = info->writer->Write(addr, toWrite, nullptr);
        std::ostringstream logMessage;
        logMessage << "Write " << toWrite << " byte(s) failed";
        if (!ret) {
            LOG(ERROR) << logMessage.str();
            return Hpackage::PKG_INVALID_STREAM;
        }
        size -= toWrite;
        addr += toWrite;

        if (info->writer->IsWriteDone()) {
            pthread_mutex_lock(&info->mutex);
            info->writer.reset();
            pthread_cond_broadcast(&info->cond);
            pthread_mutex_unlock(&info->mutex);
        }
    }
    return Hpackage::PKG_SUCCESS;
}

void* UnpackNewData(void *arg)
{
    WriterThreadInfo *info = static_cast<WriterThreadInfo *>(arg);
    Hpackage::PkgManager::StreamPtr stream = nullptr;
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    if (info->newPatch.empty()) {
        LOG(ERROR) << "new patch file name is empty. thread quit.";
        pthread_mutex_lock(&info->mutex);
        info->readyToWrite = false;
        if (info->writer != nullptr) {
            pthread_cond_broadcast(&info->cond);
        }
        pthread_mutex_unlock(&info->mutex);
        return nullptr;
    }
    LOG(DEBUG) << "new patch file name: " << info->newPatch;
    auto env = tm->GetGlobalParams()->env;
    const FileInfo *file = env->GetPkgManager()->GetFileInfo(info->newPatch);
    if (file == nullptr) {
        LOG(ERROR) << "Cannot get file info of :" << info->newPatch;
        pthread_mutex_lock(&info->mutex);
        info->readyToWrite = false;
        if (info->writer != nullptr) {
            pthread_cond_broadcast(&info->cond);
        }
        pthread_mutex_unlock(&info->mutex);
        return nullptr;
    }
    LOG(DEBUG) << info->newPatch << " info: size " << file->packedSize << " unpacked size " <<
        file->unpackedSize << " name " << file->identity;
    int32_t ret = env->GetPkgManager()->CreatePkgStream(stream, info->newPatch, ExtractNewData, info);
    if (ret != Hpackage::PKG_SUCCESS || stream == nullptr) {
        LOG(ERROR) << "Cannot extract " << info->newPatch << " from package.";
        pthread_mutex_lock(&info->mutex);
        info->readyToWrite = false;
        if (info->writer != nullptr) {
            pthread_cond_broadcast(&info->cond);
        }
        pthread_mutex_unlock(&info->mutex);
        return nullptr;
    }
    ret = env->GetPkgManager()->ExtractFile(info->newPatch, stream);
    env->GetPkgManager()->ClosePkgStream(stream);
    pthread_mutex_lock(&info->mutex);
    LOG(DEBUG) << "new data writer ending...";
    // extract new data done.
    // tell command.
    info->readyToWrite = false;
    UPDATER_WARING_CHECK (info->writer == nullptr, "writer is null", pthread_cond_broadcast(&info->cond));
    pthread_mutex_unlock(&info->mutex);
    return nullptr;
}

static int32_t ReturnAndPushParam(int32_t returnValue, Uscript::UScriptContext &context)
{
    context.PushParam(returnValue);
    return returnValue;
}

struct UpdateBlockInfo {
    std::string partitionName;
    std::string transferName;
    std::string newDataName;
    std::string patchDataName;
    std::string devPath;
};

static int32_t GetUpdateBlockInfo(struct UpdateBlockInfo &infos, Uscript::UScriptEnv &env,
    Uscript::UScriptContext &context)
{
    if (context.GetParamCount() != 4) { // 4:Determine the number of parameters
        LOG(ERROR) << "Invalid param";
        return ReturnAndPushParam(USCRIPT_INVALID_PARAM, context);
    }

    // Get partition Name first.
    // Use partition name as zip file name. ${partition name}.zip
    // load ${partition name}.zip from updater package.
    // Try to unzip ${partition name}.zip， extract transfer.list, net.dat, patch.dat
    size_t pos = 0;
    int32_t ret = context.GetParam(pos++, infos.partitionName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get param 1";
        return ret;
    }
    ret = context.GetParam(pos++, infos.transferName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get param 2";
        return ret;
    }
    ret = context.GetParam(pos++, infos.newDataName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get param 3";
        return ret;
    }
    ret = context.GetParam(pos++, infos.patchDataName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to get param 4";
        return ret;
    }

    LOG(INFO) << "ExecuteUpdateBlock::updating  " << infos.partitionName << " ...";
    infos.devPath = GetBlockDeviceByMountPoint(infos.partitionName);
    LOG(INFO) << "ExecuteUpdateBlock::updating  dev path : " << infos.devPath;
    if (infos.devPath.empty()) {
        LOG(ERROR) << "cannot get block device of partition";
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }
    return USCRIPT_SUCCESS;
}

static int32_t ExecuteTransferCommand(int fd, const std::vector<std::string> &lines, Uscript::UScriptEnv &env,
    Uscript::UScriptContext &context, const std::string &partitionName)
{
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    auto globalParams = tm->GetGlobalParams();
    auto writerThreadInfo = globalParams->writerThreadInfo.get();

    globalParams->storeBase = "/data/updater/update_tmp";
    globalParams->retryFile = std::string("/data/updater") + partitionName + "_retry";
    LOG(INFO) << "Store base path is " << globalParams->storeBase;
    int32_t ret = Store::CreateNewSpace(globalParams->storeBase, !globalParams->env->IsRetry());
    if (ret == -1) {
        LOG(ERROR) << "Error to create new store space";
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }
    globalParams->storeCreated = ret;

    if (!tm->CommandsParser(fd, lines)) {
        return USCRIPT_ERROR_EXECUTE;
    }
    pthread_mutex_lock(&writerThreadInfo->mutex);
    if (writerThreadInfo->readyToWrite) {
        LOG(WARNING) << "New data writer thread is still available...";
    }

    writerThreadInfo->readyToWrite = false;
    pthread_cond_broadcast(&writerThreadInfo->cond);
    pthread_mutex_unlock(&writerThreadInfo->mutex);
    ret = pthread_join(globalParams->thread, nullptr);
    std::ostringstream logMessage;
    logMessage << "pthread join returned with " << ret;
    if (ret != 0) {
        LOG(WARNING) << logMessage.str();
    }
    if (globalParams->storeCreated != -1) {
        Store::DoFreeSpace(globalParams->storeBase);
    }
    return USCRIPT_SUCCESS;
}

static int InitThread(const struct UpdateBlockInfo &infos, Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    auto globalParams = tm->GetGlobalParams();
    auto writerThreadInfo = globalParams->writerThreadInfo.get();
    writerThreadInfo->readyToWrite = true;
    pthread_mutex_init(&writerThreadInfo->mutex, nullptr);
    pthread_cond_init(&writerThreadInfo->cond, nullptr);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    writerThreadInfo->newPatch = infos.newDataName;
    int error = pthread_create(&globalParams->thread, &attr, UnpackNewData, writerThreadInfo);
    return error;
}

static int32_t ExtractDiffPackageAndLoad(const UpdateBlockInfo &infos, Uscript::UScriptEnv &env,
    Uscript::UScriptContext &context)
{
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    LOG(DEBUG) << "partitionName is " << infos.partitionName;
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(infos.partitionName);
    if (info == nullptr) {
        LOG(ERROR) << "Error to get file info";
        return USCRIPT_ERROR_EXECUTE;
    }
    std::string diffPackage = std::string("/data/updater") + infos.partitionName;
    int32_t ret = env.GetPkgManager()->CreatePkgStream(outStream,
        diffPackage, info->unpackedSize, PkgStream::PkgStreamType_Write);
    if (outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_ERROR_EXECUTE;
    }

    ret = env.GetPkgManager()->ExtractFile(infos.partitionName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    env.GetPkgManager()->ClosePkgStream(outStream);
    std::string diffPackageZip = diffPackage + ".zip";
    if (rename(diffPackage.c_str(), diffPackageZip.c_str()) != 0) {
        LOG(ERROR) << "rename failed";
        return USCRIPT_ERROR_EXECUTE;
    }
    LOG(DEBUG) << "Rename " << diffPackage << " to zip\nExtract " << diffPackage << " done\nReload " << diffPackageZip;
    std::vector<std::string> diffPackageComponents;
    ret = env.GetPkgManager()->LoadPackage(diffPackageZip, Updater::Utils::GetCertName(), diffPackageComponents);
    if (diffPackageComponents.size() < 1) {
        LOG(ERROR) << "Diff package is empty";
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }
    return USCRIPT_SUCCESS;
}

static int32_t DoExecuteUpdateBlock(const UpdateBlockInfo &infos, Uscript::UScriptEnv &env,
    Hpackage::PkgManager::StreamPtr &outStream, const std::vector<std::string> &lines, Uscript::UScriptContext &context)
{
    int fd = open(infos.devPath.c_str(), O_RDWR | O_LARGEFILE);
    if (fd == -1) {
        LOG(ERROR) << "Failed to open block";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    int32_t ret = ExecuteTransferCommand(fd, lines, env, context, infos.partitionName);
    fsync(fd);
    close(fd);
    fd = -1;
    env.GetPkgManager()->ClosePkgStream(outStream);
    if (ret == USCRIPT_SUCCESS) {
        PartitionRecord::GetInstance().RecordPartitionUpdateStatus(infos.partitionName, true);
    }
    return ret;
}

static int32_t ExecuteUpdateBlock(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    UpdateBlockInfo infos {};
    UPDATER_CHECK_ONLY_RETURN(!GetUpdateBlockInfo(infos, env, context), return USCRIPT_ERROR_EXECUTE);
    UPDATER_ERROR_CHECK(env.GetPkgManager() != nullptr, "Error to get pkg manager", return USCRIPT_ERROR_EXECUTE);

    if (env.IsRetry()) {
        LOG(DEBUG) << "Retry updater, check if current partition updatered already during last time";
        bool isUpdated = PartitionRecord::GetInstance().IsPartitionUpdated(infos.partitionName);
        if (isUpdated) {
            LOG(INFO) << infos.partitionName << " already updated, skip";
            return USCRIPT_SUCCESS;
        }
    }

    int32_t ret = ExtractDiffPackageAndLoad(infos, env, context);
    UPDATER_CHECK_ONLY_RETURN(ret == USCRIPT_SUCCESS, return USCRIPT_ERROR_EXECUTE);

    const FileInfo *info = env.GetPkgManager()->GetFileInfo(infos.transferName);
    UPDATER_ERROR_CHECK(info != nullptr, "GetFileInfo fail", return USCRIPT_ERROR_EXECUTE);
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    ret = env.GetPkgManager()->CreatePkgStream(outStream,
        infos.transferName, info->unpackedSize, PkgStream::PkgStreamType_MemoryMap);
    ret = env.GetPkgManager()->ExtractFile(infos.transferName, outStream);
    uint8_t *transferListBuffer = nullptr;
    size_t transferListSize = 0;
    ret = outStream->GetBuffer(transferListBuffer, transferListSize);
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    auto globalParams = tm->GetGlobalParams();
    /* Save Script Env to transfer manager */
    globalParams->env = &env;
    std::vector<std::string> lines =
        Updater::Utils::SplitString(std::string(reinterpret_cast<const char*>(transferListBuffer)), "\n");
    // Close stream opened before.
    env.GetPkgManager()->ClosePkgStream(outStream);

    LOG(INFO) << "Start unpack new data thread done. Get patch data: " << infos.patchDataName;
    info = env.GetPkgManager()->GetFileInfo(infos.patchDataName);
    UPDATER_ERROR_CHECK(info != nullptr, "GetFileInfo fail", return USCRIPT_ERROR_EXECUTE);

    ret = env.GetPkgManager()->CreatePkgStream(outStream,
        infos.patchDataName, info->unpackedSize, PkgStream::PkgStreamType_MemoryMap);
    UPDATER_ERROR_CHECK(outStream != nullptr, "Error to create output stream", return USCRIPT_ERROR_EXECUTE);
    ret = env.GetPkgManager()->ExtractFile(infos.patchDataName, outStream);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Error to extract file",
        env.GetPkgManager()->ClosePkgStream(outStream); return USCRIPT_ERROR_EXECUTE);
    outStream->GetBuffer(globalParams->patchDataBuffer, globalParams->patchDataSize);
    LOG(DEBUG) << "Patch data size is: " << globalParams->patchDataSize;

    LOG(INFO) << "Ready to start a thread to handle new data processing";
    UPDATER_ERROR_CHECK (InitThread(infos, env, context) == 0, "Failed to create pthread",
        env.GetPkgManager()->ClosePkgStream(outStream); return USCRIPT_ERROR_EXECUTE);

    ret = DoExecuteUpdateBlock(infos, env, outStream, lines, context);
    TransferManager::ReleaseTransferManagerInstance(tm);
    return ret;
}

int32_t UScriptInstructionBlockUpdate::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t result = ExecuteUpdateBlock(env, context);
    context.PushParam(result);
    return result;
}

int32_t UScriptInstructionBlockCheck::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    UPDATER_ERROR_CHECK(context.GetParamCount() == 1, "Invalid param",
        return ReturnAndPushParam(USCRIPT_INVALID_PARAM, context));
    if (env.IsRetry()) {
        return ReturnAndPushParam(USCRIPT_SUCCESS, context);
    }
    std::string partitionName;
    int32_t ret = context.GetParam(0, partitionName);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Failed to get param",
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    LOG(INFO) << "UScriptInstructionBlockCheck::dev path : " << devPath;
    UPDATER_ERROR_CHECK(!devPath.empty(), "cannot get block device of partition",
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    int fd = open(devPath.c_str(), O_RDWR | O_LARGEFILE);
    UPDATER_ERROR_CHECK(fd != -1, "Failed to open file",
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    std::vector<uint8_t> block_buff(H_BLOCK_SIZE);
    BlockSet blk0(std::vector<BlockPair> {BlockPair{0, 1}});

    size_t pos = 0;
    std::vector<BlockPair>::iterator it = blk0.Begin();
    for (; it != blk0.End(); ++it) {
        LOG(INFO) << "BlockSet::ReadDataFromBlock lseek64";
        ret = lseek64(fd, static_cast<off64_t>(it->first * H_BLOCK_SIZE), SEEK_SET);
        UPDATER_ERROR_CHECK(ret != -1, "Failed to seek",
            close(fd); return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
        size_t size = (it->second - it->first) * H_BLOCK_SIZE;
        LOG(INFO) << "BlockSet::ReadDataFromBlock Read " << size << " from block";
        UPDATER_ERROR_CHECK(Utils::ReadFully(fd, block_buff.data() + pos, size), "Failed to read",
            close(fd); return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
        pos += size;
    }

    time_t mountTime = *reinterpret_cast<uint32_t *>(&block_buff[0x400 + 0x2C]);
    uint16_t mountCount = *reinterpret_cast<uint16_t *>(&block_buff[0x400 + 0x34]);

    if (mountCount > 0) {
        std::ostringstream ostr;
        ostr << "Device was remounted R/W " << mountCount << "times\nLast remount happened on " <<
            ctime(&mountTime) << std::endl;
        std::string message = ostr.str();
        env.PostMessage("ui_log", message);
    }
    LOG(INFO) << "UScriptInstructionBlockCheck::Execute Success";
    context.PushParam(USCRIPT_SUCCESS);
    close(fd);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionShaCheck::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    UPDATER_ERROR_CHECK(context.GetParamCount() == SHA_CHECK_PARAMS, "Invalid param",
                        return ReturnAndPushParam(USCRIPT_INVALID_PARAM, context));
    UPDATER_CHECK_ONLY_RETURN(!env.IsRetry(), return ReturnAndPushParam(USCRIPT_SUCCESS, context));

    std::string partitionName;
    int32_t ret = context.GetParam(0, partitionName);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Failed to get param",
                        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    std::string blockPairs;
    ret = context.GetParam(1, blockPairs);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Failed to get param",
                        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    std::string contrastSha;
    ret = context.GetParam(SHA_CHECK_SECOND, contrastSha);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Failed to get param",
                        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    LOG(INFO) << "UScriptInstructionShaCheck::dev path : " << devPath;
    UPDATER_ERROR_CHECK(!devPath.empty(), "cannot get block device of partition",
                        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    int fd = open(devPath.c_str(), O_RDWR | O_LARGEFILE);
    UPDATER_ERROR_CHECK(fd != -1, "Failed to open file",
                        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));

    BlockSet blk;
    blk.ParserAndInsert(blockPairs);
    std::vector<uint8_t> block_buff(H_BLOCK_SIZE);

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::vector<BlockPair>::iterator it = blk.Begin();
    for (; it != blk.End(); ++it) {
        ret = lseek64(fd, static_cast<off64_t>(it->first * H_BLOCK_SIZE), SEEK_SET);
        UPDATER_ERROR_CHECK(ret != -1, "Failed to seek",
            close(fd); return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));

        for (size_t i = it->first; i < it->second; ++i) {
            UPDATER_ERROR_CHECK(Utils::ReadFully(fd, block_buff.data(), H_BLOCK_SIZE), "Failed to read",
                close(fd); return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
            SHA256_Update(&ctx, block_buff.data(), H_BLOCK_SIZE);
        }
    }
    uint8_t digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &ctx);
    std::string resultSha = Utils::ConvertSha256Hex(digest, SHA256_DIGEST_LENGTH);
    UPDATER_ERROR_CHECK(resultSha == contrastSha, "Different sha256, cannot continue",
        close(fd); return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context));
    LOG(INFO) << "UScriptInstructionShaCheck::Execute Success";
    context.PushParam(USCRIPT_SUCCESS);
    close(fd);
    return USCRIPT_SUCCESS;
}
}
