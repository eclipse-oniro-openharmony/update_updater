/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "commander_factory.h"

#include "erase_commander.h"
#include "flash_commander.h"
#include "flashd_define.h"
#include "format_commander.h"
#include "update_commander.h"

namespace Flashd {
const std::unordered_map<std::string, std::function<std::unique_ptr<Commander>(callbackFun callback)>> COMMANDERS = {
    { Hdc::CMDSTR_FLASH_PARTITION, [](callbackFun callback) { return std::make_unique<FlashCommander>(callback); } },
    { Hdc::CMDSTR_ERASE_PARTITION, [](callbackFun callback) { return std::make_unique<EraseCommander>(callback); } },
    { Hdc::CMDSTR_UPDATE_SYSTEM, [](callbackFun callback) { return std::make_unique<UpdateCommander>(callback); } },
    { Hdc::CMDSTR_FORMAT_PARTITION, [](callbackFun callback) { return std::make_unique<FormatCommander>(callback); } }
};

CommanderFactory &CommanderFactory::GetInstance()
{
    static CommanderFactory instance;
    return instance;
}

std::unique_ptr<Commander> CommanderFactory::CreateCommander(const std::string &cmd, callbackFun callback) const
{
    auto iter = COMMANDERS.find(cmd);
    if (iter != COMMANDERS.end()) {
        return iter->second(callback);
    }
    return nullptr;
}
} // namespace Flashd