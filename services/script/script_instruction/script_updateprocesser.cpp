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
#include "script_updateprocesser.h"
#include <sstream>
#include "script_instruction.h"
#include "script_manager.h"
#include "script_utils.h"

using namespace Uscript;

namespace BasicInstruction {
int32_t UScriptInstructionSetProcess::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    float setProcess = 0.0f;
    int32_t ret = context.GetParam(0, setProcess);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    std::string content;
    std::stringstream sstream;
    sstream << setProcess;
    sstream >> content;
    env.PostMessage("set_progress", content);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionShowProcess::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    float startProcess = 0.0f;
    float endProcess = 0.0f;
    int32_t ret = context.GetParam(0, startProcess);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    ret = context.GetParam(1, endProcess);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    std::string content;
    std::stringstream sstream;
    sstream << startProcess;
    sstream << ",";
    sstream << endProcess;
    sstream >> content;
    env.PostMessage("show_progress", content);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionUiPrint::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    std::string message;
    int32_t ret = context.GetParam(0, message);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    env.PostMessage("ui_log", message);
    return USCRIPT_SUCCESS;
}
} // namespace BasicInstruction
