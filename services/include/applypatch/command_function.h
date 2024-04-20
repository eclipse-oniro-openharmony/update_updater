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
#ifndef UPDATER_COMMAND_FUNCTION_H
#define UPDATER_COMMAND_FUNCTION_H

#include "command.h"
#include <unordered_map>

namespace Updater {
class CommandFunction {
public:
    virtual ~CommandFunction() = default;
    virtual CommandResult Execute(const Command &params) = 0;
};

class CommandFunctionFactory {
    DISALLOW_COPY_MOVE(CommandFunctionFactory);
public:
    static CommandFunctionFactory &GetInstance();
    CommandFunction* GetCommandFunction(std::string command);
    void RegistCommandFunction(std::string command, std::unique_ptr<CommandFunction> commandFunction);
    std::unordered_map<std::string, std::unique_ptr<CommandFunction>> commandFunctionMap_;
private:

    CommandFunctionFactory() = default;
    ~CommandFunctionFactory() = default;
};
}
#endif // UPDATER_COMMAND_FUNCTION_H