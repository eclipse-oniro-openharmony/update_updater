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

#include "applypatch/command_function.h"
#include "command_process.h"

namespace Updater {
std::unique_ptr<CommandFunction> CommandFunctionFactory::GetCommandFunction(const CommandType type)
{
    switch (type) {
        case CommandType::ABORT:
            return std::make_unique<AbortCommandFn>();
        case CommandType::NEW:
            return std::make_unique<NewCommandFn>();
        case CommandType::BSDIFF:
            return std::make_unique<DiffAndMoveCommandFn>();
        case CommandType::IMGDIFF:
            return std::make_unique<DiffAndMoveCommandFn>();
        case CommandType::ERASE:
            return std::make_unique<ZeroAndEraseCommandFn>();
        case CommandType::ZERO:
            return std::make_unique<ZeroAndEraseCommandFn>();
        case CommandType::FREE:
            return std::make_unique<FreeCommandFn>();
        case CommandType::MOVE:
            return std::make_unique<DiffAndMoveCommandFn>();
        case CommandType::STASH:
            return std::make_unique<StashCommandFn>();
        default:
            break;
    }
    return nullptr;
}

void CommandFunctionFactory::ReleaseCommandFunction(std::unique_ptr<CommandFunction> &instr)
{
    instr.reset();
}
}