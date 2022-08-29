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
#include "utils.h"

namespace Updater {
using namespace Updater::Utils;
static TransferManagerPtr g_transferManagerInstance = nullptr;
TransferManagerPtr TransferManager::GetTransferManagerInstance()
{
    if (g_transferManagerInstance == nullptr) {
        g_transferManagerInstance = new TransferManager();
        g_transferManagerInstance->Init();
    }
    return g_transferManagerInstance;
}

void TransferManager::ReleaseTransferManagerInstance(TransferManagerPtr transferManager)
{
    delete transferManager;
    g_transferManagerInstance = nullptr;
    transferManager = nullptr;
}

TransferManager::~TransferManager()
{
    if (globalParams != nullptr) {
        if (globalParams->writerThreadInfo != nullptr) {
            globalParams->writerThreadInfo.reset();
        }
        globalParams.reset();
    }
}

bool TransferManager::CommandsParser(int fd, const std::vector<std::string> &context)
{
    UPDATER_ERROR_CHECK(context.size() >= 1, "too small context in transfer file", return false);
    UPDATER_ERROR_CHECK(globalParams != nullptr, "globalParams is nullptr", return false);
    std::vector<std::string>::const_iterator ct = context.begin();
    globalParams->version = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    globalParams->blockCount = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    globalParams->maxEntries = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    globalParams->maxBlocks = Utils::String2Int<size_t>(*ct++, Utils::N_DEC);
    size_t totalSize = globalParams->blockCount;
    std::string retryCmd = "";
    if (globalParams->env != nullptr && globalParams->env->IsRetry()) {
        retryCmd = ReloadForRetry();
    }
    std::unique_ptr<Command> cmd;
    size_t initBlock = 0;
    for (; ct != context.end(); ct++) {
        cmd = std::make_unique<Command>();
        UPDATER_ERROR_CHECK(cmd != nullptr, "Failed to parse command line.", return false);
        if (!cmd->Init(*ct) || cmd->GetCommandType() == CommandType::LAST || globalParams->env == nullptr) {
            continue;
        }
        if (!retryCmd.empty() && globalParams->env->IsRetry()) {
            if (*ct == retryCmd) {
                retryCmd.clear();
            }
            if (cmd->GetCommandType() != CommandType::NEW) {
                LOG(INFO) << "Retry: Command " << *ct << " passed";
                continue;
            }
        }
        cmd->SetFileDescriptor(fd);
        std::unique_ptr<CommandFunction> cf = CommandFunctionFactory::GetCommandFunction(cmd->GetCommandType());
        UPDATER_ERROR_CHECK(cf != nullptr, "Failed to get cmd exec", return false);
        CommandResult ret = cf->Execute(const_cast<Command &>(*cmd.get()));
        CommandFunctionFactory::ReleaseCommandFunction(cf);
        if (CheckResult(ret, cmd->GetCommandLine(), cmd->GetCommandType()) == false) {
            return false;
        }
        if (initBlock == 0) {
            initBlock = globalParams->written;
        }
        if (totalSize != 0 && globalParams->env != nullptr && NeedSetProgress(cmd->GetCommandType())) {
            globalParams->env->PostMessage("set_progress",
                std::to_string((static_cast<double>(globalParams->written) - initBlock) / totalSize));
        }
        LOG(INFO) << "Running command : " << cmd->GetArgumentByPos(0) << " success";
    }
    return true;
}

void TransferManager::Init()
{
    globalParams = std::make_unique<GlobalParams>();
    globalParams->writerThreadInfo = std::make_unique<WriterThreadInfo>();
    blocksetMap.clear();
}

bool TransferManager::RegisterForRetry(const std::string &cmd)
{
    std::string path = globalParams->retryFile;
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        LOG(ERROR) << "Failed to create";
        return false;
    }
    if (fchown(fd, O_USER_GROUP_ID, O_USER_GROUP_ID) != 0) {
        LOG(ERROR) << "Failed to chown";
        close(fd);
        return -1;
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
    std::string path = globalParams->retryFile;
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
            if (globalParams->env != nullptr) {
                globalParams->env->PostMessage("retry_update", cmd);
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
