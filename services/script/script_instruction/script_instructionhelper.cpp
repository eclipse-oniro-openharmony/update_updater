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
#include "script_instructionhelper.h"
#include <dlfcn.h>
#include <set>
#include "scope_guard.h"
#include "script_basicinstruction.h"
#include "script_loadscript.h"
#include "script_manager_impl.h"
#include "script_registercmd.h"
#include "script_updateprocesser.h"
#include "script_utils.h"

using namespace BasicInstruction;
using namespace Updater;

namespace Uscript {
static std::set<std::string> g_reservedInstructions = {
    "LoadScript", "RegisterCmd", "abort", "assert", "concat",
    "is_substring", "stdout", "sleep", "set_progress", "ui_print",
    "show_progress"
    };

static ScriptInstructionHelper* g_instructionHelper = nullptr;

ScriptInstructionHelper* ScriptInstructionHelper::GetBasicInstructionHelper(ScriptManagerImpl *impl)
{
    if (g_instructionHelper == nullptr) {
        if (impl == nullptr) {
            return nullptr;
        }
        g_instructionHelper = new ScriptInstructionHelper(impl);
    }
    return g_instructionHelper;
}

void ScriptInstructionHelper::ReleaseBasicInstructionHelper()
{
    if (g_instructionHelper != nullptr) {
        delete g_instructionHelper;
    }
    g_instructionHelper = nullptr;
}

ScriptInstructionHelper::~ScriptInstructionHelper()
{
    if (instrLib_ != nullptr) {
        dlclose(instrLib_);
    }
    instrLib_ = nullptr;
}

int32_t ScriptInstructionHelper::RegisterInstructions() const
{
    scriptManager_->AddInstruction("RegisterCmder", new ScriptRegisterCmd());
    scriptManager_->AddInstruction("LoadScript", new ScriptLoadScript());
    scriptManager_->AddInstruction("Stdout", new UScriptInstructionStdout());
    scriptManager_->AddInstruction("Abort", new UScriptInstructionAbort());
    scriptManager_->AddInstruction("Assert", new UScriptInstructionAssert());
    scriptManager_->AddInstruction("Sleep", new UScriptInstructionSleep());
    scriptManager_->AddInstruction("Concat", new UScriptInstructionConcat());
    scriptManager_->AddInstruction("IsSubString", new UScriptInstructionIsSubString());
    scriptManager_->AddInstruction("set_progress", new UScriptInstructionSetProcess());
    scriptManager_->AddInstruction("show_progress", new UScriptInstructionShowProcess());
    scriptManager_->AddInstruction("ui_print", new UScriptInstructionUiPrint());
    return USCRIPT_SUCCESS;
}

bool ScriptInstructionHelper::IsReservedInstruction(const std::string &scriptName) const
{
    if (g_reservedInstructions.find(scriptName) != g_reservedInstructions.end()) {
        return true;
    }
    return false;
}

int32_t ScriptInstructionHelper::AddScript(const std::string &scriptName, int32_t priority) const
{
    return scriptManager_->AddScript(scriptName, priority);
}

int32_t ScriptInstructionHelper::AddInstruction(const std::string &instrName, const UScriptInstructionPtr instr)
{
    if (IsReservedInstruction(instrName)) {
        USCRIPT_LOGE(" %s reserved", instrName.c_str());
        return USCRIPT_ERROR_REVERED;
    }
    return scriptManager_->AddInstruction(instrName, instr);
}

int32_t ScriptInstructionHelper::RegisterUserInstruction(const std::string& libName,
    const std::string &instrName)
{
    // first get realpath of libName, then compare with realLibName
    char *realPath = realpath(libName.c_str(), nullptr);
    USCRIPT_CHECK(realPath != nullptr, return USCRIPT_INVALID_PARAM, "realPath is NULL %s", libName.c_str());
    std::string realLibName = realPath;
    free(realPath);
    if (!userInstrLibName_.empty() && userInstrLibName_.compare(realLibName) != 0) {
        USCRIPT_LOGE("Lib name must be equal %s ", realLibName.c_str());
        return USCRIPT_INVALID_PARAM;
    }

    userInstrLibName_.assign(realLibName);
    Uscript::UScriptInstructionFactoryPtr factory = nullptr;
    if (instrLib_ == nullptr) {
        instrLib_ = dlopen(realLibName.c_str(), RTLD_LAZY | RTLD_LOCAL);
    }
    USCRIPT_CHECK(instrLib_ != nullptr, return USCRIPT_INVALID_PARAM,
        "Fail to dlopen %s ", libName.c_str());
    auto pGetInstructionFactory = (Uscript::UScriptInstructionFactoryPtr(*)())dlsym(instrLib_, "GetInstructionFactory");
    auto pReleaseInstructionFactory =
        (void(*)(Uscript::UScriptInstructionFactoryPtr))dlsym(instrLib_, "ReleaseInstructionFactory");
    if (pReleaseInstructionFactory == nullptr || pGetInstructionFactory == nullptr) {
        USCRIPT_LOGE("Fail to get sym %s", libName.c_str());
        return USCRIPT_INVALID_PARAM;
    }
    factory = pGetInstructionFactory();
    USCRIPT_CHECK(factory != nullptr, return USCRIPT_INVALID_PARAM,
        "Fail to create instruction factory for %s", instrName.c_str());
    ON_SCOPE_EXIT(freeFactory) {
        pReleaseInstructionFactory(factory);
    };
    // Create instruction and register it
    UScriptInstructionPtr instr = nullptr;
    int32_t ret = factory->CreateInstructionInstance(instr, instrName);
    if (ret != USCRIPT_SUCCESS || instr == nullptr) {
        USCRIPT_LOGE("Fail to create instruction for %s", instrName.c_str());
        return ret == USCRIPT_SUCCESS ? USCRIPT_ERROR_CREATE_OBJ : USCRIPT_NOTEXIST_INSTRUCTION;
    }

    ret = AddInstruction(instrName, instr);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Fail to add instruction for %s", instrName.c_str());
        // ret is USCRIPT_ERROR_REVERED, instr register failed, can be deleted
        delete instr;
        instr = nullptr;
    }
    // ScriptManagerImpl::AddInstruction has saved instr, don't delete it here!!!
    return ret;
}

int32_t ScriptInstructionHelper::RegisterUserInstruction(const std::string &instrName,
    Uscript::UScriptInstructionFactory *factory)
{
    if (factory == nullptr) {
        USCRIPT_LOGE("%s factory is null", instrName.c_str());
        return USCRIPT_INVALID_PARAM;
    }

    // Create instruction and register it
    UScriptInstructionPtr instr = nullptr;
    int32_t ret = factory->CreateInstructionInstance(instr, instrName);
    if (ret != USCRIPT_SUCCESS || instr == nullptr) {
        USCRIPT_LOGE("Fail to create instruction for %s", instrName.c_str());
        // when instr == nullptr && ret == USCRIPT_SUCCESS, shouldn't return USCRIPT_SUCCESS
        return ret == USCRIPT_SUCCESS ? USCRIPT_ERROR_CREATE_OBJ : USCRIPT_NOTEXIST_INSTRUCTION;
    }

    ret = AddInstruction(instrName, instr);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Fail to add instruction for %s", instrName.c_str());
        delete instr;
        instr = nullptr;
        return ret;
    }

    USCRIPT_LOGI("RegisterUserInstruction %s successfull", instrName.c_str());
    return ret;
}
} // namespace Uscript
