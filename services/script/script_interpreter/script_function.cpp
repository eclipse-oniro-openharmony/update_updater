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
#include "script_function.h"
#include "script_context.h"
#include "script_interpreter.h"
#include "script_manager.h"
#include "script_param.h"
#include "script_utils.h"

using namespace std;

namespace Uscript {
ScriptFunction::~ScriptFunction()
{
    delete params_;
    delete statements_;
}

UScriptValuePtr ScriptFunction::Execute(ScriptInterpreter &inter,
    UScriptContextPtr context, ScriptParams *inputParams)
{
    INTERPRETER_LOGI(inter, context, "ScriptFunction execute %s", functionName_.c_str());
    UScriptContextPtr funcContext = std::make_shared<UScriptInterpretContext>();
    if (funcContext == nullptr) {
        USCRIPT_LOGE("[interpreter-%d] Fail to create context for function %s",
            inter.GetInstanceId(), functionName_.c_str());
        return std::make_shared<ErrorValue>(USCRIPT_ERROR_CREATE_OBJ);
    }

    if (inputParams == nullptr || params_ == nullptr) {
        if (params_ != nullptr || inputParams != nullptr) {
            USCRIPT_LOGE("[interpreter-%d] ScriptFunction::Execute param not match %s",
                inter.GetInstanceId(), functionName_.c_str());
            return std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
        }
    } else {
        if (params_->GetParams().size() != inputParams->GetParams().size()) {
            USCRIPT_LOGE("[interpreter-%d] ScriptFunction::Execute param not match %s",
                inter.GetInstanceId(), functionName_.c_str());
            return std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
        }

        size_t index = 0;
        std::vector<std::string> paramNames = GetParamNames(inter, context);
        for (auto expression : inputParams->GetParams()) {
            UScriptValuePtr var = expression->Execute(inter, context);
            if (var == nullptr || var->GetValueType() == UScriptValue::VALUE_TYPE_ERROR) {
                USCRIPT_LOGE("[interpreter-%d] ScriptFunction::Execute fail to computer param %s",
                    inter.GetInstanceId(), functionName_.c_str());
                return std::make_shared<ErrorValue>(USCRIPT_NOTEXIST_INSTRUCTION);
            }
            if (index >= paramNames.size()) {
                USCRIPT_LOGE("[interpreter-%d] ScriptFunction::Execute invalid index %zu param %s",
                    inter.GetInstanceId(), index, functionName_.c_str());
                return std::make_shared<ErrorValue>(USCRIPT_NOTEXIST_INSTRUCTION);
            }
            funcContext->UpdateVariables(inter, var, paramNames, index);
        }
    }
    UScriptStatementResult result = statements_->Execute(inter, funcContext);
    INTERPRETER_LOGI(inter, context, "ScriptFunction execute %s result %s", functionName_.c_str(),
        UScriptStatementResult::ScriptToString(&result).c_str());
    return result.GetResultValue();
}

std::vector<std::string> ScriptFunction::GetParamNames(ScriptInterpreter &inter,
    UScriptContextPtr context) const
{
    int32_t ret;
    std::vector<std::string> names;
    for (auto expression : params_->GetParams()) {
        std::string varName;
        ret = IdentifierExpression::GetIdentifierName(expression, varName);
        if (ret != USCRIPT_SUCCESS) {
            INTERPRETER_LOGE(inter, context, "Fail to get param name %s", functionName_.c_str());
            return names;
        }
        names.push_back(varName);
    }
    return names;
}
} // namespace Uscript
