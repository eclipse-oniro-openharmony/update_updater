/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "applypatch/transfer_manager.h"
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include "applypatch/command_function.h"
#include "log/log.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "applypatch/update_progress.h"
#include "thread_pool.h"

namespace Updater {
using namespace Updater::Utils;

TransferManager::TransferManager()
{
    transferParams_ = std::make_unique<TransferParams>();
    transferParams_->writerThreadInfo = std::make_unique<WriterThreadInfo>();
}

bool TransferManager::CommandsExecute(int fd, Command &cmd)
{
    cmd.SetFileDescriptor(fd);
    std::unique_ptr<CommandFunction> cf = CommandFunctionFactory::GetCommandFunction(cmd.GetCommandType());
    if (cf == nullptr) {
        LOG(ERROR) << "Failed to get cmd exec";
        return false;
    }
    CommandResult ret = cf->Execute(cmd);
    CommandFunctionFactory::ReleaseCommandFunction(cf);
    if (!CheckResult(ret, cmd.GetCommandLine(), cmd.GetCommandType())) {
        return false;
    }
    return true;
}

bool TransferManager::CommandsParser(int fd, const std::vector<std::string> &context)
{
    if (context.size() < 1) {
        LOG(ERROR) << "too small context in transfer file";
        return false;
    }
    if (transferParams_ == nullptr) {
        LOG(ERROR) << "transferParams_ is nullptr";
        return false;
    }

    std::vector<std::string>::const_iterator ct = context.begin();
    transferParams_->version = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    transferParams_->blockCount = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    transferParams_->maxEntries = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    transferParams_->maxBlocks = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    size_t totalSize = transferParams_->blockCount;
    std::string retryCmd = "";
    if (transferParams_->env != nullptr && transferParams_->env->IsRetry()) {
        retryCmd = ReloadForRetry();
    }
    size_t initBlock = 0;
    for (; ct != context.end(); ct++) {
        std::unique_ptr<Command> cmd = std::make_unique<Command>(this);
        if (cmd == nullptr) {
            LOG(ERROR) << "Failed to parse command line.";
            return false;
        }
        if (!cmd->Init(*ct) || cmd->GetCommandType() == CommandType::LAST || transferParams_->env == nullptr) {
            continue;
        }
        if (!retryCmd.empty() && transferParams_->env->IsRetry()) {
            if (*ct == retryCmd) {
                retryCmd.clear();
            }
            if (cmd->GetCommandType() != CommandType::NEW) {
                LOG(INFO) << "Retry: Command " << *ct << " passed";
                continue;
            }
        }
        if (!CommandsExecute(fd, *cmd)) {
            LOG(ERROR) << "Running command : " << cmd->GetArgumentByPos(0) << " fail";
            return false;
        }
        if (initBlock == 0) {
            initBlock = transferParams_->written;
        }
        if (totalSize != 0 && NeedSetProgress(cmd->GetCommandType())) {
            UpdateProgress(initBlock, totalSize);
        }
    }
    return true;
}

void TransferManager::UpdateProgress(size_t &initBlock, size_t totalSize)
{
    float p = static_cast<float>(transferParams_->written - initBlock) / totalSize\
                                    * Uscript::GetScriptProportion();
    SetUpdateProgress(p);
    initBlock = transferParams_->written;
}

bool TransferManager::RegisterForRetry(const std::string &cmd)
{
    std::string path = transferParams_->retryFile;
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        LOG(ERROR) << "Failed to create";
        return false;
    }
    bool ret = Utils::WriteStringToFile(fd, cmd);
    if (ret == false) {
        LOG(ERROR) << "Write retry flag error";
    }
    fsync(fd);
    close(fd);
    return ret;
}

std::string TransferManager::ReloadForRetry() const
{
    std::string path = transferParams_->retryFile;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        LOG(ERROR) << "Failed to open";
        return "";
    }
    (void)lseek(fd, 0, SEEK_SET);
    std::string cmd = "";
    if (!Utils::ReadFileToString(fd, cmd)) {
        LOG(ERROR) << "Error to read retry flag";
    }
    close(fd);
    return cmd;
}

bool TransferManager::NeedSetProgress(const CommandType &type)
{
    return type == CommandType::NEW ||
        type == CommandType::IMGDIFF ||
        type == CommandType::BSDIFF ||
        type == CommandType::ZERO;
}

bool TransferManager::CheckResult(const CommandResult result, const std::string &cmd, const CommandType &type)
{
    switch (result) {
        case SUCCESS:
            if (type != CommandType::NEW) {
                RegisterForRetry(cmd);
            }
            break;
        case NEED_RETRY:
            LOG(INFO) << "Running command need retry!";
            if (transferParams_->env != nullptr) {
                transferParams_->env->PostMessage("retry_update", IO_FAILED_REBOOT);
            }
            return false;
        case FAILED:
        default:
            LOG(ERROR) << "Running command failed";
            return false;
    }
    return true;
}
} // namespace Updater
