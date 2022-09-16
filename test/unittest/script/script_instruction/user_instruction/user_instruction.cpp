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

#include <cstring>
#include <iostream>
#include "script_manager.h"
#include "script/script_instruction.h"

using namespace std;
using namespace Uscript;

class UserInstruction1 : public UScriptInstruction {
public:
    int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        context.PushParam("user instruction1");
        return USCRIPT_SUCCESS;
    }
};

class UserInstruction2 : public UScriptInstruction {
public:
    int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        context.PushParam("user instruction2");
        return USCRIPT_SUCCESS;
    }
};

class UserInstructionAbort : public UScriptInstruction {
public:
    int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        context.PushParam("user instruction2");
        return USCRIPT_SUCCESS;
    }
};

class UserInstructionFactory : public UScriptInstructionFactory {
public:
    int32_t CreateInstructionInstance(UScriptInstructionPtr& instr, const std::string& name) override
    {
        if (name == "uInstruction1") {
            instr = new (std::nothrow) UserInstruction1(); // 为何不加nothrow？？？
        } else if (name == "uInstruction2") {
            instr = new (std::nothrow) UserInstruction2();
        } else if (name == "uInstruction3") {
            instr = nullptr; // mock new failed scene
        } else if (name == "abort") {
            instr = new (std::nothrow) UserInstructionAbort(); // mock reserved error
        } else {
            return USCRIPT_NOTEXIST_INSTRUCTION;
        }
        return USCRIPT_SUCCESS;
    }
    UserInstructionFactory()
    {
    }
    ~UserInstructionFactory()
    {
    }
};

extern "C" {
__attribute__((visibility("default"))) Uscript::UScriptInstructionFactoryPtr GetInstructionFactory()
{
    return new (std::nothrow) UserInstructionFactory;
}

__attribute__((visibility("default"))) void ReleaseInstructionFactory(Uscript::UScriptInstructionFactoryPtr p)
{
    delete p;
}
}