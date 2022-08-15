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
#ifndef USCRIPT_BASIC_INSTRUCTION_H
#define USCRIPT_BASIC_INSTRUCTION_H

#include "script_instruction.h"

namespace BasicInstruction {
class UScriptInstructionAbort : public Uscript::UScriptInstruction {
public:
    UScriptInstructionAbort() {}
    virtual ~UScriptInstructionAbort() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};

class UScriptInstructionAssert : public Uscript::UScriptInstruction {
public:
    UScriptInstructionAssert() {}
    virtual ~UScriptInstructionAssert() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};

class UScriptInstructionSleep : public Uscript::UScriptInstruction {
public:
    UScriptInstructionSleep() {}
    virtual ~UScriptInstructionSleep() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};
class UScriptInstructionStdout : public Uscript::UScriptInstruction {
public:
    UScriptInstructionStdout() {}
    virtual ~UScriptInstructionStdout() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};

class UScriptInstructionConcat : public Uscript::UScriptInstruction {
public:
    UScriptInstructionConcat() {}
    virtual ~UScriptInstructionConcat() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};

class UScriptInstructionIsSubString : public Uscript::UScriptInstruction {
public:
    UScriptInstructionIsSubString() {}
    virtual ~UScriptInstructionIsSubString() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
};
} // namespace BasicInstruction
#endif // USCRIPT_BASIC_INSTRUCTION_H
