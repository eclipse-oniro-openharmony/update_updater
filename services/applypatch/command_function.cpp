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
extern "C" __attribute__((constructor)) void RegistBlockUpdateCommandFunction(void)
{
    const std::unordered_map<std::string, std::function<std::unique_ptr<CommandFunction>()>> COMMANDFUNC = {
        { "abort", []() { return std::make_unique<AbortCommandFn>(); } },
        { "bsdiff", []() { return std::make_unique<DiffAndMoveCommandFn>(); } },
        { "new", []() { return std::make_unique<NewCommandFn>(); } },
        { "pkgdiff", []() { return std::make_unique<DiffAndMoveCommandFn>(); } },
        { "zero", []() { return std::make_unique<ZeroAndEraseCommandFn>(); } },
        { "erase", []() { return std::make_unique<ZeroAndEraseCommandFn>(); } },
        { "free", []() { return std::make_unique<FreeCommandFn>(); } },
        { "move", []() { return std::make_unique<DiffAndMoveCommandFn>(); } },
        { "stash", []() { return std::make_unique<StashCommandFn>(); } },
        { "copy", []() { return std::make_unique<DiffAndMoveCommandFn>(); } }
    };
    for (auto &iter : COMMANDFUNC) {
        CommandFunctionFactory::GetInstance().RegistCommandFunction(iter.first, iter.second());
    }
}

CommandFunctionFactory &CommandFunctionFactory::GetInstance()
{
    static CommandFunctionFactory instance;
    return instance;
}

CommandFunction* CommandFunctionFactory::GetCommandFunction(std::string command)
{
    auto iter = commandFunctionMap_.find(command);
    if (iter != commandFunctionMap_.end()) {
        return iter->second.get();
    }
    return nullptr;
}

void CommandFunctionFactory::RegistCommandFunction(std::string command,
                                                   std::unique_ptr<CommandFunction> commandFunction)
{
    commandFunctionMap_.emplace(command, std::move(commandFunction));
}
}