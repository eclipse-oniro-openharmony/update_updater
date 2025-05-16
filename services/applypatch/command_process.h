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
#ifndef UPDATER_COMMAND_PROCESS_H
#define UPDATER_COMMAND_PROCESS_H

#include "applypatch/command.h"
#include "applypatch/command_function.h"

namespace Updater {
class AbortCommandFn : public CommandFunction {
public:
    AbortCommandFn() {}
    ~AbortCommandFn() override {}
    CommandResult Execute(const Command &params) override;
};

class NewCommandFn : public CommandFunction {
public:
    NewCommandFn() {}
    ~NewCommandFn() override {}
    CommandResult Execute(const Command &params) override;
private:
    int32_t StreamExecute(const Command &params);
};

class ZeroAndEraseCommandFn : public CommandFunction {
public:
    ZeroAndEraseCommandFn() {}
    ~ZeroAndEraseCommandFn() override {}
    CommandResult Execute(const Command &params) override;
};

class FreeCommandFn : public CommandFunction {
public:
    FreeCommandFn() {}
    ~FreeCommandFn() override {}
    CommandResult Execute(const Command &params) override;
};

class StashCommandFn : public CommandFunction {
public:
    StashCommandFn() {}
    ~StashCommandFn() override {}
    CommandResult Execute(const Command &params) override;
};

class DiffAndMoveCommandFn : public CommandFunction {
public:
    DiffAndMoveCommandFn() {}
    ~DiffAndMoveCommandFn() override {}
    CommandResult Execute(const Command &params) override;
    virtual int32_t WriteDiffToBlock(const Command &params, std::vector<uint8_t> &srcBuffer,
                                     uint8_t *patchBuffer, size_t patchLength, BlockSet &targetBlock);
    virtual int32_t WriteFileToBlock(const Command &params, std::vector<uint8_t> &srcBuffer,
        size_t offset, size_t patchLength, BlockSet &targetBlock);
};
}
#endif // UPDATER_COMMAND_PROCESS_H
