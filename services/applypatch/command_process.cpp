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
namespace Updater {
CommandResult AbortCommandFn::Execute(const Command &params)
{
    return SUCCESS;
}

CommandResult NewCommandFn::Execute(const Command &params)
{
    BlockSet bs;
    bs.ParserAndInsert(params.GetArgumentByPos(1));
    LOG(INFO) << " writing " << bs.TotalBlockSize() << " blocks of new data";
    auto writerThreadInfo = params.GetTransferParams()->writerThreadInfo.get();
    pthread_mutex_lock(&writerThreadInfo->mutex);
    writerThreadInfo->writer = std::make_unique<BlockWriter>(params.GetFileDescriptor(), bs);
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

CommandResult ZeroAndEraseCommandFn::Execute(const Command &params)
{
    bool isErase = false;
    if (params.GetCommandType() == CommandType::ERASE) {
        isErase = true;
        LOG(INFO) << "Start run ERASE command";
    }
    if (isErase) {
        struct stat statBlock {};
        if (fstat(params.GetFileDescriptor(), &statBlock) == -1) {
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
    auto ret = CommandResult(blk.WriteZeroToBlock(params.GetFileDescriptor(), isErase));
    if (ret == SUCCESS) {
        params.GetTransferParams()->written += blk.TotalBlockSize();
    }
    return ret;
}

bool LoadTarget(const Command &params, size_t &pos, std::vector<uint8_t> &buffer,
    BlockSet &targetBlock, CommandResult &result)
{
    CommandType type = params.GetCommandType();
    // Read sha256 of source and target
    std::string srcHash = params.GetArgumentByPos(pos++);
    std::string tgtHash = srcHash;
    if (type != CommandType::MOVE) {
        tgtHash = params.GetArgumentByPos(pos++);
    }

    // Read the target's buffer to determine whether it needs to be written
    size_t tgtBlockSize;
    std::string cmdTmp = params.GetArgumentByPos(pos++);
    targetBlock.ParserAndInsert(cmdTmp);
    tgtBlockSize = targetBlock.TotalBlockSize() * H_BLOCK_SIZE;
    buffer.resize(tgtBlockSize);
    if (targetBlock.ReadDataFromBlock(params.GetFileDescriptor(), buffer) == 0) {
        LOG(ERROR) << "Read data from block error, TotalBlockSize: " << targetBlock.TotalBlockSize();
        result = FAILED;
        return false;
    }
    if (targetBlock.VerifySha256(buffer, targetBlock.TotalBlockSize(), tgtHash) == 0) {
        LOG(ERROR) << "Will write same sha256 blocks to target, no need to write";
        result = SUCCESS;
        return false;
    }

    if (targetBlock.LoadTargetBuffer(params, buffer, tgtBlockSize, pos, srcHash) != 0) {
        LOG(ERROR) << "Failed to load blocks";
        result = FAILED;
        return false;
    }
    return true;
}

int32_t DiffAndMoveCommandFn::WriteDiffToBlock(const Command &params, std::vector<uint8_t> &srcBuffer,
                                               uint8_t *patchBuffer, size_t patchLength, BlockSet &targetBlock)
{
    CommandType type = params.GetCommandType();
    return targetBlock.WriteDiffToBlock(params, srcBuffer, patchBuffer, patchLength, type == CommandType::IMGDIFF);
}

CommandResult DiffAndMoveCommandFn::Execute(const Command &params)
{
    CommandType type = params.GetCommandType();
    size_t pos = H_DIFF_CMD_ARGS_START;
    if (type == CommandType::MOVE) {
        pos = H_MOVE_CMD_ARGS_START;
    }

    BlockSet targetBlock;
    std::vector<uint8_t> buffer;
    CommandResult result = FAILED;
    if (!LoadTarget(params, pos, buffer, targetBlock, result)) {
        return result;
    }

    int32_t ret = -1;
    if (type != CommandType::MOVE) {
        pos = H_MOVE_CMD_ARGS_START;
        size_t offset = Utils::String2Int<size_t>(params.GetArgumentByPos(pos++), Utils::N_DEC);
        size_t patchLength = Utils::String2Int<size_t>(params.GetArgumentByPos(pos++), Utils::N_DEC);
        uint8_t *patchBuffer = params.GetTransferParams()->patchDataBuffer + offset;
        ret = WriteDiffToBlock(params, buffer, patchBuffer, patchLength, targetBlock);
    } else {
        ret = targetBlock.WriteDataToBlock(params.GetFileDescriptor(), buffer) == 0 ? -1 : 0;
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
    LOG(INFO) << "Confirm whether the block is stored";
    if (Store::LoadDataFromStore(storeBase, shaStr, buffer) == 0) {
        LOG(INFO) << "The stash has been stored, skipped";
        return SUCCESS;
    }
    LOG(INFO) << "Read block data to buffer";
    if (srcBlk.ReadDataFromBlock(params.GetFileDescriptor(), buffer) == 0) {
        LOG(ERROR) << "Error to load block data";
        return FAILED;
    }
    if (srcBlk.VerifySha256(buffer, srcBlockSize, shaStr) != 0) {
        return FAILED;
    }
    LOG(INFO) << "store " << srcBlockSize << " blocks to " << storeBase << "/" << shaStr;
    int ret = Store::WriteDataToStore(storeBase, shaStr, buffer, srcBlockSize * H_BLOCK_SIZE);
    return CommandResult(ret);
}
} // namespace Updater
