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
#ifndef USCRIPT_TRANSFERLIST_H
#define USCRIPT_TRANSFERLIST_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "applypatch/command_const.h"
#include "applypatch/command.h"
#include "script_instruction.h"
#include "script_manager.h"


namespace Updater {

struct WriterThreadInfo {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    BlockSet bs;
    std::unique_ptr<BlockWriter> writer;
    bool readyToWrite;
    Uscript::UScriptEnv *env;
    Hpackage::PkgManager::FileInfoPtr fileInfo;
    std::string newPatch;
};

struct TransferParams {
    size_t version;
    size_t blockCount;
    size_t maxEntries;
    size_t maxBlocks;
    size_t written;
    pthread_t thread;
    Uscript::UScriptEnv *env;
    std::unique_ptr<WriterThreadInfo> writerThreadInfo;
    int storeCreated;
    std::string storeBase;
    std::string freeStash;
    std::string retryFile;
    uint8_t *patchDataBuffer;
    size_t patchDataSize;
};

class TransferManager;
using TransferManagerPtr = TransferManager *;
class TransferManager {
public:
    TransferManager();
    virtual ~TransferManager() {};

    bool CommandsParser(int fd, const std::vector<std::string> &context);

    TransferParams* GetTransferParams()
    {
        return transferParams_.get();
    }
    std::string ReloadForRetry() const;
    bool CheckResult(const CommandResult result, const std::string &cmd, const CommandType &type);
    bool NeedSetProgress(const CommandType &type);

private:
    bool RegisterForRetry(const std::string &cmd);
    bool CommandsExecute(int fd, Command &cmd);
    std::unique_ptr<TransferParams> transferParams_;
};
} // namespace Updater
#endif
