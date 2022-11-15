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
#include "script_basicinstruction.h"
#include <unistd.h>
#include "script_utils.h"

using namespace Uscript;
using namespace std;

namespace BasicInstruction {
int32_t UScriptInstructionAbort::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t result = 1;
    int32_t ret = context.GetParam(0, result);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    return ((result == 0) ? USCRIPT_ABOART : USCRIPT_SUCCESS);
}

int32_t UScriptInstructionAssert::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t result = 1;
    int32_t ret = context.GetParam(0, result);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    return ((result == 0) ? USCRIPT_ASSERT : USCRIPT_SUCCESS);
}

int32_t UScriptInstructionSleep::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t seconds = 1;
    int32_t ret = context.GetParam(0, seconds);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    sleep(seconds);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionConcat::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t ret = 0;
    std::string str;
    ret = context.GetParam(0, str);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }

    for (int32_t i = 1; i < context.GetParamCount(); i++) {
        switch (context.GetParamType(i)) {
            case UScriptContext::PARAM_TYPE_INTEGER: {
                int32_t v;
                ret = context.GetParam(i, v);
                if (ret != USCRIPT_SUCCESS) {
                    USCRIPT_LOGE("Failed to get param");
                    return ret;
                }
                str.append(to_string(v));
                break;
            }
            case UScriptContext::PARAM_TYPE_FLOAT: {
                float v;
                ret = context.GetParam(i, v);
                if (ret != USCRIPT_SUCCESS) {
                    USCRIPT_LOGE("Failed to get param");
                    return ret;
                }
                str.append(to_string(v));
                break;
            }
            case UScriptContext::PARAM_TYPE_STRING: {
                std::string v;
                ret = context.GetParam(i, v);
                if (ret != USCRIPT_SUCCESS) {
                    USCRIPT_LOGE("Failed to get param");
                    return ret;
                }
                str.append(v);
                break;
            }
            default:
                break;
        }
    }
    context.PushParam(str);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionIsSubString::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    std::string str;
    std::string subStr;
    int32_t ret = context.GetParam(0, str);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    ret = context.GetParam(1, subStr);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to get param");
        return ret;
    }
    string::size_type last = str.find(subStr);
    if (last == string::npos) {
        context.PushParam(0);
    } else {
        context.PushParam(1);
    }
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionStdout::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    int32_t ret;
    for (int32_t i = 0; i < context.GetParamCount(); i++) {
        if (context.GetParamType(i) == UScriptContext::PARAM_TYPE_INTEGER) {
            int32_t v;
            ret = context.GetParam(i, v);
            if (ret != USCRIPT_SUCCESS) {
                USCRIPT_LOGE("Failed to get param");
                return ret;
            }
            std::cout << v << "  ";
        } else if (context.GetParamType(i) == UScriptContext::PARAM_TYPE_FLOAT) {
            float v;
            ret = context.GetParam(i, v);
            if (ret != USCRIPT_SUCCESS) {
                USCRIPT_LOGE("Failed to get param");
                return ret;
            }
            std::cout << v << "  ";
        } else if (context.GetParamType(i) == UScriptContext::PARAM_TYPE_STRING) {
            std::string v;
            ret = context.GetParam(i, v);
            if (ret != USCRIPT_SUCCESS) {
                USCRIPT_LOGE("Failed to get param");
                return ret;
            }
            std::cout << v << "  ";
        }
    }
    std::cout << std::endl;
    return USCRIPT_SUCCESS;
}
} // namespace BasicInstruction
