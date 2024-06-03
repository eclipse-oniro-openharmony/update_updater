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
#include "log/dump.h"
#include "log/log.h"
#include "updater/updater_const.h"
#include "updater/hwfault_retry.h"
#include "utils.h"

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
constexpr int32_t SHA_CHECK_SECOND = 2;
constexpr int32_t SHA_CHECK_PARAMS = 3;
constexpr int32_t SHA_CHECK_TARGETPAIRS_INDEX = 3;
constexpr int32_t SHA_CHECK_TARGETSHA_INDEX = 4;
constexpr int32_t SHA_CHECK_TARGET_PARAMS = 5;
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

static inline void CondBroadcast(WriterThreadInfo *info)
{
    pthread_mutex_lock(&info->mutex);
    info->readyToWrite = false;
    if (info->writer != nullptr) {
        pthread_cond_broadcast(&info->cond);
    }
    pthread_mutex_unlock(&info->mutex);
}

void* UnpackNewData(void *arg)
{
    TransferManagerPtr tm = static_cast<TransferManagerPtr>(arg);
    WriterThreadInfo *info = tm->GetTransferParams()->writerThreadInfo.get();
    Hpackage::PkgManager::StreamPtr stream = nullptr;
    if (info->newPatch.empty()) {
        LOG(ERROR) << "new patch file name is empty. thread quit.";
        CondBroadcast(info);
        return nullptr;
    }
    LOG(DEBUG) << "new patch file name: " << info->newPatch;
    auto env = tm->GetTransferParams()->env;
    const FileInfo *file = env->GetPkgManager()->GetFileInfo(info->newPatch);
    if (file == nullptr) {
        LOG(ERROR) << "Cannot get file info of :" << info->newPatch;
        CondBroadcast(info);
        return nullptr;
    }
    LOG(DEBUG) << info->newPatch << " info: size " << file->packedSize << " unpacked size " <<
        file->unpackedSize << " name " << file->identity;
    int32_t ret = env->GetPkgManager()->CreatePkgStream(stream, info->newPatch, ExtractNewData, info);
    if (ret != Hpackage::PKG_SUCCESS || stream == nullptr) {
        LOG(ERROR) << "Cannot extract " << info->newPatch << " from package.";
        CondBroadcast(info);
        return nullptr;
    }
    ret = env->GetPkgManager()->ExtractFile(info->newPatch, stream);
    env->GetPkgManager()->ClosePkgStream(stream);
    LOG(DEBUG) << "new data writer ending...";
    // extract new data done.
    // tell command.
    CondBroadcast(info);
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

static int32_t ExecuteTransferCommand(int fd, const std::vector<std::string> &lines, TransferManagerPtr tm,
    Uscript::UScriptContext &context, const std::string &partitionName)
{
    auto transferParams = tm->GetTransferParams();
    auto writerThreadInfo = transferParams->writerThreadInfo.get();

    transferParams->storeBase = std::string("/data/updater") + partitionName + "_tmp";
    transferParams->retryFile = std::string("/data/updater") + partitionName + "_retry";
    LOG(INFO) << "Store base path is " << transferParams->storeBase;
    int32_t ret = Store::CreateNewSpace(transferParams->storeBase, !transferParams->env->IsRetry());
    if (ret == -1) {
        LOG(ERROR) << "Error to create new store space";
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }
    transferParams->storeCreated = ret;

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
    ret = pthread_join(transferParams->thread, nullptr);
    std::ostringstream logMessage;
    logMessage << "pthread join returned with " << ret;
    if (ret != 0) {
        LOG(WARNING) << logMessage.str();
    }
    if (transferParams->storeCreated != -1) {
        Store::DoFreeSpace(transferParams->storeBase);
    }
    return USCRIPT_SUCCESS;
}

static int InitThread(const struct UpdateBlockInfo &infos, TransferManagerPtr tm)
{
    auto transferParams = tm->GetTransferParams();
    auto writerThreadInfo = transferParams->writerThreadInfo.get();
    writerThreadInfo->readyToWrite = true;
    pthread_mutex_init(&writerThreadInfo->mutex, nullptr);
    pthread_cond_init(&writerThreadInfo->cond, nullptr);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    writerThreadInfo->newPatch = infos.newDataName;
    int error = pthread_create(&transferParams->thread, &attr, UnpackNewData, tm);
    return error;
}

static int32_t ExtractDiffPackageAndLoad(const UpdateBlockInfo &infos, Uscript::UScriptEnv &env,
    Uscript::UScriptContext &context)
{
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    LOG(DEBUG) << "partitionName is " << infos.partitionName;
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(infos.partitionName);
    if (info == nullptr) {
        LOG(WARNING) << "Error to get file info";
        return USCRIPT_SUCCESS;
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

static int32_t DoExecuteUpdateBlock(const UpdateBlockInfo &infos, TransferManagerPtr tm,
    Hpackage::PkgManager::StreamPtr &outStream, const std::vector<std::string> &lines, Uscript::UScriptContext &context)
{
    int fd = open(infos.devPath.c_str(), O_RDWR | O_LARGEFILE);
    auto env = tm->GetTransferParams()->env;
    if (fd == -1) {
        LOG(ERROR) << "Failed to open block";
        env->GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    int32_t ret = ExecuteTransferCommand(fd, lines, tm, context, infos.partitionName);
    fsync(fd);
    close(fd);
    fd = -1;
    env->GetPkgManager()->ClosePkgStream(outStream);
    if (ret == USCRIPT_SUCCESS) {
        PartitionRecord::GetInstance().RecordPartitionUpdateStatus(infos.partitionName, true);
    }
    return ret;
}

static int32_t ExtractFileByName(Uscript::UScriptEnv &env, const std::string &fileName,
    Hpackage::PkgManager::StreamPtr &outStream, uint8_t *&outBuf, size_t &buffSize)
{
    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_ERROR_EXECUTE;
    }

    const FileInfo *info = env.GetPkgManager()->GetFileInfo(fileName);
    if (info == nullptr) {
        LOG(ERROR) << "GetFileInfo fail";
        return USCRIPT_ERROR_EXECUTE;
    }
    auto ret = env.GetPkgManager()->CreatePkgStream(outStream,
        fileName, info->unpackedSize, PkgStream::PkgStreamType_MemoryMap);
    if (ret != USCRIPT_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_ERROR_EXECUTE;
    }
    ret = env.GetPkgManager()->ExtractFile(fileName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    ret = outStream->GetBuffer(outBuf, buffSize);
    LOG(DEBUG) << "outBuf data size is: " << buffSize;

    return USCRIPT_SUCCESS;
}

static int32_t ExecuteUpdateBlock(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    UpdateBlockInfo infos {};
    if (GetUpdateBlockInfo(infos, env, context) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    if (env.IsRetry()) {
        LOG(DEBUG) << "Retry updater, check if current partition updatered already during last time";
        if (PartitionRecord::GetInstance().IsPartitionUpdated(infos.partitionName)) {
            LOG(INFO) << infos.partitionName << " already updated, skip";
            return USCRIPT_SUCCESS;
        }
    }

    if (ExtractDiffPackageAndLoad(infos, env, context) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    uint8_t *transferListBuffer = nullptr;
    size_t transferListSize = 0;
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    if (ExtractFileByName(env, infos.transferName, outStream,
                          transferListBuffer, transferListSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    std::unique_ptr<TransferManager> tm = std::make_unique<TransferManager>();

    auto transferParams = tm->GetTransferParams();
    /* Save Script Env to transfer manager */
    transferParams->env = &env;

    std::vector<std::string> lines =
        Updater::Utils::SplitString(std::string(reinterpret_cast<const char*>(transferListBuffer)), "\n");
    // Close stream opened before.
    env.GetPkgManager()->ClosePkgStream(outStream);

    LOG(INFO) << "Start unpack new data thread done. Get patch data: " << infos.patchDataName;
    if (ExtractFileByName(env, infos.patchDataName, outStream,
        transferParams->patchDataBuffer, transferParams->patchDataSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    LOG(INFO) << "Ready to start a thread to handle new data processing";
    if (InitThread(infos, tm.get()) != 0) {
        LOG(ERROR) << "Failed to create pthread";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }

    return DoExecuteUpdateBlock(infos, tm.get(), outStream, lines, context);
}

int32_t UScriptInstructionBlockUpdate::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t result = ExecuteUpdateBlock(env, context);
    context.PushParam(result);
    return result;
}

bool UScriptInstructionBlockCheck::ExecReadBlockInfo(const std::string &devPath, Uscript::UScriptContext &context,
    time_t &mountTime, uint16_t &mountCount)
{
    UPDATER_INIT_RECORD;
    int fd = open(devPath.c_str(), O_RDWR | O_LARGEFILE);
    if (fd == -1) {
        LOG(ERROR) << "Failed to open file";
        UPDATER_LAST_WORD(false);
        return false;
    }
    std::vector<uint8_t> block_buff(H_BLOCK_SIZE);
    BlockSet blk0(std::vector<BlockPair> {BlockPair{0, 1}});

    size_t pos = 0;
    std::vector<BlockPair>::iterator it = blk0.Begin();
    for (; it != blk0.End(); ++it) {
        LOG(INFO) << "BlockSet::ReadDataFromBlock lseek64";
        if (lseek64(fd, static_cast<off64_t>(it->first * H_BLOCK_SIZE), SEEK_SET) == -1) {
            LOG(ERROR) << "Failed to seek";
            close(fd);
            UPDATER_LAST_WORD(false);
            return false;
        }
        size_t size = (it->second - it->first) * H_BLOCK_SIZE;
        LOG(INFO) << "BlockSet::ReadDataFromBlock Read " << size << " from block";
        if (!Utils::ReadFully(fd, block_buff.data() + pos, size)) {
            LOG(ERROR) << "Failed to read";
            close(fd);
            UPDATER_LAST_WORD(false);
            return false;
        }
        pos += size;
    }
    close(fd);
    mountTime = *reinterpret_cast<uint32_t *>(&block_buff[0x400 + 0x2C]);
    mountCount = *reinterpret_cast<uint16_t *>(&block_buff[0x400 + 0x34]);
    return true;
}

int32_t UScriptInstructionBlockCheck::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    if (context.GetParamCount() != 1) {
        LOG(ERROR) << "Invalid param";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return ReturnAndPushParam(USCRIPT_INVALID_PARAM, context);
    }
    if (env.IsRetry()) {
        return ReturnAndPushParam(USCRIPT_SUCCESS, context);
    }
    std::string partitionName;
    int32_t ret = context.GetParam(0, partitionName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Failed to get param";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }
    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    LOG(INFO) << "UScriptInstructionBlockCheck::dev path : " << devPath;
    time_t mountTime = 0;
    uint16_t mountCount = 0;
    if (devPath.empty() || (!ExecReadBlockInfo(devPath, context, mountTime, mountCount))) {
        LOG(ERROR) << "cannot get block device of partition";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }

    if (mountCount > 0) {
        std::ostringstream ostr;
        ostr << "Device was remounted R/W " << mountCount << "times\nLast remount happened on " <<
            ctime(&mountTime) << std::endl;
        std::string message = ostr.str();
        env.PostMessage("ui_log", message);
        LOG(ERROR) << message;
    }
    LOG(INFO) << "UScriptInstructionBlockCheck::Execute Success";
    context.PushParam(USCRIPT_SUCCESS);
    return USCRIPT_SUCCESS;
}

int UScriptInstructionShaCheck::ExecReadShaInfo(Uscript::UScriptEnv &env, const std::string &devPath,
    const ShaInfo &shaInfo)
{
    UPDATER_INIT_RECORD;
    std::string resultSha = CalculateBlockSha(devPath, shaInfo.blockPairs);
    std::string tgtResultSha = CalculateBlockSha(devPath, shaInfo.targetPairs);
    if (resultSha.empty() && tgtResultSha.empty()) {
        LOG(ERROR) << "All sha is empty";
        return USCRIPT_ERROR_EXECUTE;
    }

    bool isTargetDiff = tgtResultSha.empty() ? true : (tgtResultSha != shaInfo.targetSha);
    if (resultSha != shaInfo.contrastSha && isTargetDiff) {
        LOG(ERROR) << "Different sha256, cannot continue";
        LOG(ERROR) << "blockPairs:" << shaInfo.blockPairs;
        PrintAbnormalBlockHash(devPath, shaInfo.blockPairs);
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        env.PostMessage(UPDATER_RETRY_TAG, VERIFY_FAILED_REBOOT);
        return USCRIPT_ERROR_EXECUTE;
    }
    LOG(INFO) << "UScriptInstructionShaCheck::Execute Success";
    return USCRIPT_SUCCESS;
}

void UScriptInstructionShaCheck::PrintAbnormalBlockHash(const std::string &devPath, const std::string &blockPairs)
{
    int fd = open(devPath.c_str(), O_RDWR | O_LARGEFILE);
    if (fd == -1) {
        LOG(ERROR) << "Failed to open file";
        return;
    }

    BlockSet blk;
    blk.ParserAndInsert(blockPairs);
    std::vector<uint8_t> block_buff(H_BLOCK_SIZE);
    std::vector<BlockPair>::iterator it = blk.Begin();
    for (; it != blk.End(); ++it) {
        if (lseek64(fd, static_cast<off64_t>(it->first * H_BLOCK_SIZE), SEEK_SET) == -1) {
            LOG(ERROR) << "Failed to seek";
            close(fd);
            return;
        }
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        for (size_t i = it->first; i < it->second; ++i) {
            if (!Utils::ReadFully(fd, block_buff.data(), H_BLOCK_SIZE)) {
                LOG(ERROR) << "Failed to read";
                close(fd);
                return;
            }
            SHA256_Update(&ctx, block_buff.data(), H_BLOCK_SIZE);
        }
        uint8_t digest[SHA256_DIGEST_LENGTH] = {0};
        SHA256_Final(digest, &ctx);
        LOG(ERROR) << "block id:" << it->first << "-" << it->second <<
            " hex:" << Utils::ConvertSha256Hex(digest, SHA256_DIGEST_LENGTH);
    }
    close(fd);
}

std::string UScriptInstructionShaCheck::CalculateBlockSha(const std::string &devPath, const std::string &blockPairs)
{
    if (blockPairs.empty()) {
        LOG(ERROR) << "Failed to get blockPairs";
        return "";
    }

    int fd = open(devPath.c_str(), O_RDWR | O_LARGEFILE);
    if (fd == -1) {
        LOG(ERROR) << "Failed to open file";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return "";
    }

    BlockSet blk;
    blk.ParserAndInsert(blockPairs);
    std::vector<uint8_t> block_buff(H_BLOCK_SIZE);
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    std::vector<BlockPair>::iterator it = blk.Begin();
    for (; it != blk.End(); ++it) {
        if (lseek64(fd, static_cast<off64_t>(it->first * H_BLOCK_SIZE), SEEK_SET) == -1) {
            LOG(ERROR) << "Failed to seek";
            close(fd);
            UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
            return "";
        }
        for (size_t i = it->first; i < it->second; ++i) {
            if (!Utils::ReadFully(fd, block_buff.data(), H_BLOCK_SIZE)) {
                LOG(ERROR) << "Failed to read";
                close(fd);
                UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
                return "";
            }
            SHA256_Update(&ctx, block_buff.data(), H_BLOCK_SIZE);
        }
    }
    close(fd);

    uint8_t digest[SHA256_DIGEST_LENGTH] = {0};
    SHA256_Final(digest, &ctx);
    return Utils::ConvertSha256Hex(digest, SHA256_DIGEST_LENGTH);
}

int32_t UScriptInstructionShaCheck::SetShaInfo(Uscript::UScriptContext &context, ShaInfo &shaInfo)
{
    int32_t ret = context.GetParam(1, shaInfo.blockPairs);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Failed to get param blockPairs";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }

    ret = context.GetParam(SHA_CHECK_SECOND, shaInfo.contrastSha);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Failed to get param contrastSha";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }

    // Only three parameters can be obtained for the upgrade package of an earlier version.
    ret = context.GetParam(SHA_CHECK_TARGETPAIRS_INDEX, shaInfo.targetPairs);
    if (ret != USCRIPT_SUCCESS) {
        LOG(WARNING) << "Failed to get param targetPairs";
    }

    ret = context.GetParam(SHA_CHECK_TARGETSHA_INDEX, shaInfo.targetSha);
    if (ret != USCRIPT_SUCCESS) {
        LOG(WARNING) << "Failed to get param targetSha";
    }

    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionShaCheck::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t paramCount = context.GetParamCount();
    if (paramCount != SHA_CHECK_PARAMS && paramCount != SHA_CHECK_TARGET_PARAMS) {
        LOG(ERROR) << "Invalid param";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return ReturnAndPushParam(USCRIPT_INVALID_PARAM, context);
    }
    if (env.IsRetry() && !Utils::CheckFaultInfo(VERIFY_FAILED_REBOOT)) {
        return ReturnAndPushParam(USCRIPT_SUCCESS, context);
    }

    std::string partitionName;
    int32_t ret = context.GetParam(0, partitionName);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Failed to get param";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }

    ShaInfo shaInfo {};
    ret = SetShaInfo(context, shaInfo);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Failed to set sha info";
        return ReturnAndPushParam(ret, context);
    }

    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    LOG(INFO) << "UScriptInstructionShaCheck::dev path : " << devPath;
    if (devPath.empty()) {
        LOG(ERROR) << "cannot get block device of partition";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return ReturnAndPushParam(USCRIPT_ERROR_EXECUTE, context);
    }
    ret = ExecReadShaInfo(env, devPath, shaInfo);
    return ReturnAndPushParam(ret, context);
}
}
