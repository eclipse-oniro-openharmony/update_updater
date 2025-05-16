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

#include "command_process.h"
#include <cstdio>
#include <fcntl.h>
#include <linux/fs.h>
#include <memory>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "applypatch/block_set.h"
#include "applypatch/block_writer.h"
#include "applypatch/data_writer.h"
#include "applypatch/store.h"
#include "applypatch/transfer_manager.h"
#include "log/log.h"
#include "securec.h"
#include "utils.h"

using namespace Hpackage;
using namespace Updater::Utils;
namespace Updater {
CommandResult AbortCommandFn::Execute(const Command &params)
{
    return SUCCESS;
}

CommandResult NewCommandFn::Execute(const Command &params)
{
    if (params.IsStreamCmd()) {
        return (StreamExecute(params) != 0) ? FAILED : SUCCESS;
    }
    BlockSet bs;
    size_t pos = H_NEW_CMD_ARGS_START;
    bs.ParserAndInsert(params.GetArgumentByPos(pos));
    LOG(INFO) << " writing " << bs.TotalBlockSize() << " blocks of new data";
    auto writerThreadInfo = params.GetTransferParams()->writerThreadInfo.get();
    pthread_mutex_lock(&writerThreadInfo->mutex);
    writerThreadInfo->writer = std::make_unique<BlockWriter>(params.GetTargetFileDescriptor(), bs);
    pthread_cond_broadcast(&writerThreadInfo->cond);
    while (writerThreadInfo->writer != nullptr) {
        LOG(DEBUG) << "wait for new data write done...";
        if (!writerThreadInfo->readyToWrite) {
            LOG(ERROR) << "writer thread could not write blocks. " << bs.TotalBlockSize() * H_BLOCK_SIZE -
                writerThreadInfo->writer->GetTotalWritten() << " bytes lost";
            pthread_mutex_unlock(&writerThreadInfo->mutex);
            writerThreadInfo->writer.reset();
            writerThreadInfo->writer = nullptr;
            return FAILED;
        }
        LOG(DEBUG) << "Writer already written " << writerThreadInfo->writer->GetTotalWritten() << " byte(s)";
        pthread_cond_wait(&writerThreadInfo->cond, &writerThreadInfo->mutex);
    }
    pthread_mutex_unlock(&writerThreadInfo->mutex);

    writerThreadInfo->writer.reset();
    params.GetTransferParams()->written += bs.TotalBlockSize();
    return SUCCESS;
}

int32_t NewCommandFn::StreamExecute(const Command &params)
{
    size_t pos = H_NEW_CMD_ARGS_START;
    uint8_t *addr = params.GetTransferParams()->dataBuffer;
    size_t size = params.GetTransferParams()->dataBufferSize;
    std::string tgtHash = "";
    tgtHash = params.GetArgumentByPos(pos++);
    LOG(INFO) << "StreamExecute size:" << size << " cmd:" << params.GetCommandLine();
    BlockSet bs;
    bs.ParserAndInsert(params.GetArgumentByPos(pos++));
    std::unique_ptr<BlockWriter> writer = std::make_unique<BlockWriter>(params.GetTargetFileDescriptor(), bs);
    while (size > 0) {
        size_t toWrite = std::min(size, writer->GetBlocksSize() - writer->GetTotalWritten());
        LOG(INFO) << "StreamExecute toWrite:" << toWrite;
        // No more data to write.
        if (toWrite == 0) {
            break;
        }
        bool ret = writer->Write(addr, toWrite, nullptr);
        if (!ret) {
            return -1;
        }
        size -= toWrite;
        addr += toWrite;
    }
    size_t tgtBlockSize = bs.TotalBlockSize() * H_BLOCK_SIZE;
    std::vector<uint8_t> tgtBuffer(tgtBlockSize);

    if (bs.ReadDataFromBlock(params.GetTargetFileDescriptor(), tgtBuffer) == 0) {
        LOG(ERROR) << "Read data from block error, TotalBlockSize: " << bs.TotalBlockSize();
        return -1;
    }
    if (bs.VerifySha256(tgtBuffer, bs.TotalBlockSize(), tgtHash) == 0) {
        LOG(ERROR) << "Will write same sha256 blocks to target, no need to write";
        return -1;
    }
    std::vector<uint8_t>().swap(tgtBuffer);
    return 0;
}

CommandResult ZeroAndEraseCommandFn::Execute(const Command &params)
{
    bool isErase = false;
    if (params.GetCommandType() == CommandType::ERASE) {
        isErase = true;
        LOG(INFO) << "Start run ERASE command";
    }
    if (isErase && Utils::IsUpdaterMode()) {
        struct stat statBlock {};
        if (fstat(params.GetTargetFileDescriptor(), &statBlock) == -1) {
            LOG(ERROR) << "Failed to fstat";
            return FAILED;
        }
#ifndef UPDATER_UT
        if (!S_ISBLK(statBlock.st_mode)) {
            LOG(ERROR) << "Invalid block device";
            return FAILED;
        }
#endif
    }

    BlockSet blk;
    blk.ParserAndInsert(params.GetArgumentByPos(1));
    LOG(INFO) << "Parser params to block set";
    auto ret = CommandResult(blk.WriteZeroToBlock(params.GetTargetFileDescriptor(), isErase));
    if (ret == SUCCESS && !isErase) {
        params.GetTransferParams()->written += blk.TotalBlockSize();
    }
    return ret;
}

bool LoadTarget(const Command &params, size_t &pos, std::vector<uint8_t> &buffer,
    BlockSet &targetBlock, CommandResult &result)
{
    CommandType type = params.GetCommandType();
    std::string srcHash = "";
    std::string tgtHash = "";
    // Read sha256 of source and target
    if (type == CommandType::BSDIFF || type == CommandType::IMGDIFF) {
        srcHash = params.GetArgumentByPos(pos++);
        tgtHash = params.GetArgumentByPos(pos++);
    } else if (type == CommandType::MOVE) {
        srcHash = params.GetArgumentByPos(pos++);
        tgtHash = srcHash;
    }

    // Read the target's buffer to determine whether it needs to be written
    std::string cmdTmp = params.GetArgumentByPos(pos++);
    targetBlock.ParserAndInsert(cmdTmp);
    if (type != CommandType::COPY) {
        size_t tgtBlockSize = targetBlock.TotalBlockSize() * H_BLOCK_SIZE;
        std::vector<uint8_t> tgtBuffer(tgtBlockSize);

        if (targetBlock.ReadDataFromBlock(params.GetTargetFileDescriptor(), tgtBuffer) == 0) {
            LOG(ERROR) << "Read data from block error, TotalBlockSize: " << targetBlock.TotalBlockSize();
            result = FAILED;
            return false;
        }
        if (targetBlock.VerifySha256(tgtBuffer, targetBlock.TotalBlockSize(), tgtHash) == 0) {
            result = SUCCESS;
            return false;
        }
        std::vector<uint8_t>().swap(tgtBuffer);
    }
    std::string blockLen = params.GetArgumentByPos(pos++);
    size_t srcBlockSize = String2Int<size_t>(blockLen, N_DEC);
    buffer.resize(srcBlockSize * H_BLOCK_SIZE);
    if (targetBlock.LoadTargetBuffer(params, buffer, srcBlockSize, pos, srcHash) != 0) {
        LOG(ERROR) << "Failed to load blocks";
        result = FAILED;
        return false;
    }
    result = SUCCESS;
    return true;
}

int32_t DiffAndMoveCommandFn::WriteDiffToBlock(const Command &params, std::vector<uint8_t> &srcBuffer,
                                               uint8_t *patchBuffer, size_t patchLength, BlockSet &targetBlock)
{
    CommandType type = params.GetCommandType();
    return targetBlock.WriteDiffToBlock(params, srcBuffer, patchBuffer, patchLength, type == CommandType::IMGDIFF);
}

int32_t DiffAndMoveCommandFn::WriteFileToBlock(const Command &params, std::vector<uint8_t> &srcBuffer,
    size_t offset, size_t patchLength, BlockSet &targetBlock)
{
    std::ifstream fin(params.GetTransferParams()->patchDatFile, std::ios::in | std::ios::binary);
    if (!fin.is_open()) {
        LOG(ERROR) << "open dat file failed " << params.GetTransferParams()->patchDatFile;
        return static_cast<int>(FAILED);
    }
    std::unique_ptr<uint8_t[]> patchBuffer = std::make_unique<uint8_t[]>(patchLength);
    (void)memset_s(patchBuffer.get(), patchLength, 0, patchLength);
    if ((!fin.seekg(static_cast<int>(offset), std::ios::beg)) ||
        (!fin.read(reinterpret_cast<char *>(patchBuffer.get()), patchLength))) {
        LOG(ERROR) << "read dat file failed gcount " << fin.gcount() << ", patch len " << patchLength;
        fin.close();
        return static_cast<int>(FAILED);
    }
    fin.close();
    return WriteDiffToBlock(params, srcBuffer, patchBuffer.get(), patchLength, targetBlock);
}

CommandResult DiffAndMoveCommandFn::Execute(const Command &params)
{
    CommandType type = params.GetCommandType();
    size_t pos = H_DIFF_CMD_ARGS_START;
    if (type == CommandType::MOVE) {
        pos = H_MOVE_CMD_ARGS_START;
    } else if (type == CommandType::COPY) {
        pos = H_COPY_CMD_ARGS_START;
    }

    BlockSet targetBlock;
    std::vector<uint8_t> buffer;
    CommandResult result = FAILED;
    if (!LoadTarget(params, pos, buffer, targetBlock, result)) {
        return result;
    }
    if (!params.GetTransferParams()->canWrite) {
        return result;
    }

    int32_t ret = -1;
    if (type != CommandType::MOVE && type != CommandType::COPY) {
        pos = H_MOVE_CMD_ARGS_START;
        size_t offset;
        if (params.IsStreamCmd()) {
            offset = 0;
            pos++;
        } else {
            offset = Utils::String2Int<size_t>(params.GetArgumentByPos(pos++), Utils::N_DEC);
        }
        size_t patchLength = Utils::String2Int<size_t>(params.GetArgumentByPos(pos++), Utils::N_DEC);
        if (params.GetTransferParams()->isUpdaterMode) {
            uint8_t *patchBuffer = params.GetTransferParams()->patchDataBuffer + offset;
            ret = WriteDiffToBlock(params, buffer, patchBuffer, patchLength, targetBlock);
        } else {
            ret = WriteFileToBlock(params, buffer, offset, patchLength, targetBlock);
        }
    } else {
        ret = targetBlock.WriteDataToBlock(params.GetTargetFileDescriptor(), buffer) == 0 ? -1 : 0;
    }
    if (ret != 0) {
        LOG(ERROR) << "fail to write block data.";
        return errno == EIO ? NEED_RETRY : FAILED;
    }
    std::string storeBase = params.GetTransferParams()->storeBase;
    std::string freeStash = params.GetTransferParams()->freeStash;
    if (!freeStash.empty()) {
        if (Store::FreeStore(storeBase, freeStash) != 0) {
            LOG(WARNING) << "fail to delete file: " << freeStash;
        }
        params.GetTransferParams()->freeStash.clear();
    }
    params.GetTransferParams()->written += targetBlock.TotalBlockSize();
    return SUCCESS;
}

CommandResult FreeCommandFn::Execute(const Command &params)
{
    std::string shaStr = params.GetArgumentByPos(1);
    std::string storeBase = params.GetTransferParams()->storeBase;
    if (params.GetTransferParams()->storeCreated == 0) {
        return CommandResult(Store::FreeStore(storeBase, shaStr));
    }
    return SUCCESS;
}

CommandResult StashCommandFn::Execute(const Command &params)
{
    size_t pos = 1;
    const std::string shaStr = params.GetArgumentByPos(pos++);
    BlockSet srcBlk;
    LOG(INFO) << "Get source block info to block set";
    srcBlk.ParserAndInsert(params.GetArgumentByPos(pos++));
    size_t srcBlockSize = srcBlk.TotalBlockSize();
    std::vector<uint8_t> buffer;
    buffer.resize(srcBlockSize * H_BLOCK_SIZE);
    std::string storeBase = params.GetTransferParams()->storeBase;
    LOG(DEBUG) << "Confirm whether the block is stored";
    if (Store::LoadDataFromStore(storeBase, shaStr, buffer) == 0) {
        LOG(INFO) << "The stash has been stored, skipped";
        return SUCCESS;
    }
    LOG(DEBUG) << "Read block data to buffer";
    if (srcBlk.ReadDataFromBlock(params.GetSrcFileDescriptor(), buffer) == 0) {
        LOG(ERROR) << "Error to load block data";
        return FAILED;
    }
    int32_t res = srcBlk.VerifySha256(buffer, srcBlockSize, shaStr);
    if (res != 0 && !params.GetTransferParams()->canWrite) {
        res = BlockVerify(params, buffer, srcBlockSize, shaStr, pos);
    }
    if (res != 0) {
        LOG(WARNING) << "failed to load source blocks for stash";
        return SUCCESS;
    }
    LOG(INFO) << "store " << srcBlockSize << " blocks to " << storeBase << "/" << shaStr;
    int ret = Store::WriteDataToStore(storeBase, shaStr, buffer, srcBlockSize * H_BLOCK_SIZE);
    return CommandResult(ret);
}
} // namespace Updater
