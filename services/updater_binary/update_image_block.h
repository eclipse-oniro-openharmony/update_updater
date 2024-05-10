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

#ifndef UPDATER_UPDATE_IMAGE_BLOCK_H
#define UPDATER_UPDATE_IMAGE_BLOCK_H
#include "pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"

namespace Updater {
class UScriptInstructionBlockUpdate : public Uscript::UScriptInstruction {
public:
    UScriptInstructionBlockUpdate() {}
    virtual ~UScriptInstructionBlockUpdate() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};

class UScriptInstructionBlockCheck : public Uscript::UScriptInstruction {
public:
    UScriptInstructionBlockCheck() {}
    virtual ~UScriptInstructionBlockCheck() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
private:
    bool ExecReadBlockInfo(const std::string &devPath, Uscript::UScriptContext &context,
        time_t &mountTime, uint16_t &mountCount);
};

class UScriptInstructionShaCheck : public Uscript::UScriptInstruction {
public:
    UScriptInstructionShaCheck() {}
    virtual ~UScriptInstructionShaCheck() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
private:
    int ExecReadShaInfo(Uscript::UScriptEnv &env, const std::string &devPath, const std::string &blockPairs,
        const std::string &contrastSha, const std::string &targetSha);
    void PrintAbnormalBlockHash(const std::string &devPath, const std::string &blockPairs);
    std::string CalculateBlocksSha(const std::string &devPath, const std::string &blockPairs);
};
}

#endif // UPDATER_UPDATE_IMAGE_BLOCK_H
