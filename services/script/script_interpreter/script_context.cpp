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
#include "script_context.h"
#include <iostream>
#include <typeinfo>
#include <cmath>
#include "script_expression.h"
#include "script_interpreter.h"
#include "script_utils.h"

using namespace std;
using namespace Updater;

namespace Uscript {
static uint32_t g_contextId = 0;

int32_t UScriptInstructionContext::PushParam(int32_t value)
{
    UScriptValuePtr valuePtr = std::make_shared<IntegerValue>(value);
    USCRIPT_CHECK(valuePtr != nullptr, return USCRIPT_ERROR_CREATE_OBJ, "Failed to create value");
    outParam_.push_back(valuePtr);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionContext::PushParam(float value)
{
    UScriptValuePtr valuePtr = std::make_shared<FloatValue>(value);
    USCRIPT_CHECK(valuePtr != nullptr, return USCRIPT_ERROR_CREATE_OBJ, "Failed to create value");
    outParam_.push_back(valuePtr);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionContext::PushParam(const std::string& value)
{
    UScriptValuePtr valuePtr = std::make_shared<StringValue>(value);
    USCRIPT_CHECK(valuePtr != nullptr, return USCRIPT_ERROR_CREATE_OBJ, "Failed to create value");
    outParam_.push_back(valuePtr);
    return USCRIPT_SUCCESS;
}

int32_t UScriptInstructionContext::GetParamCount()
{
    return innerParam_.size();
}

int32_t UScriptInstructionContext::GetParam(int32_t index, int32_t &value)
{
    return GetParam<int32_t, IntegerValue>(index, value);
}

int32_t UScriptInstructionContext::GetParam(int32_t index, float &value)
{
    return GetParam<float, FloatValue>(index, value);
}

int32_t UScriptInstructionContext::GetParam(int32_t index, std::string &value)
{
    return GetParam<std::string, StringValue>(index, value);
}

template<class T, class TWapper>
int32_t UScriptInstructionContext::GetParam(int32_t index, T &value)
{
    if (static_cast<size_t>(index) >= this->innerParam_.size()) {
        LOG(ERROR) << "Invalid index " << index;
        return UScriptContext::PARAM_TYPE_INVALID;
    }
    // check whether TWapper's type is same with inner param's type
    TWapper v {T {}};
    if (innerParam_[index].get()->GetValueType() != v.GetValueType()) {
        LOG(ERROR) << "Invalid index " << index;
        return USCRIPT_INVALID_PARAM;
    }
    // then perform cast between innerparam and TWapper
    TWapper* inter = static_cast<TWapper*>(innerParam_[index].get());
    value = inter->GetValue();
    return USCRIPT_SUCCESS;
}

UScriptContext::ParamType UScriptInstructionContext::GetParamType(int32_t index)
{
    USCRIPT_CHECK(static_cast<size_t>(index) < this->innerParam_.size(),
        return UScriptContext::PARAM_TYPE_INVALID, "Invalid index %d", index);
    UScriptValue::UScriptValueType type = innerParam_[index]->GetValueType();
    return (UScriptContext::ParamType)type;
}

int32_t UScriptInstructionContext::AddInputParam(UScriptValuePtr value)
{
    innerParam_.push_back(value);
    return USCRIPT_SUCCESS;
}

UScriptValuePtr UScriptInterpretContext::FindVariable(const ScriptInterpreter &inter, std::string id)
{
    INTERPRETER_LOGI(inter, this, "FindVariable varName:%s ", id.c_str());
    if (localVariables_.find(id) != localVariables_.end()) {
        return localVariables_[id];
    }
    return nullptr;
}

UScriptInterpretContext::UScriptInterpretContext(bool top) : top_(top)
{
    contextId_ = ++g_contextId;
}

void UScriptInterpretContext::UpdateVariable(const ScriptInterpreter &inter, std::string id,
    UScriptValuePtr value)
{
    INTERPRETER_LOGI(inter, this, " Update varName:%s value: %s", id.c_str(),
        UScriptValue::ScriptToString(value).c_str());
    localVariables_[id] = value;
}

void UScriptInterpretContext::UpdateVariables(const ScriptInterpreter &inter,
    UScriptValuePtr value,
    std::vector<std::string> ids,
    size_t &startIndex)
{
    if (value->GetValueType() != UScriptValue::VALUE_TYPE_LIST) {
        USCRIPT_CHECK(startIndex < ids.size(), return, "Invalid startIndex %d", startIndex);
        UpdateVariable(inter, ids[startIndex], value);
        startIndex++;
        return;
    }

    ReturnValue* values = static_cast<ReturnValue*>(value.get());
    for (auto out : values->GetValues()) {
        USCRIPT_CHECK(startIndex < ids.size(), return, "Invalid startIndex %d", startIndex);
        UpdateVariable(inter, ids[startIndex], out);
        startIndex++;
    }
}

UScriptValuePtr UScriptValue::Computer(int32_t action, UScriptValuePtr value)
{
    return std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
}

#define INTEGER_INTEGER_COMPUTER(op, rightValue) do {                                     \
    IntegerValue* value = static_cast<IntegerValue *>((rightValue).get());                \
    if (value == nullptr) {                                                               \
        USCRIPT_LOGE("Failed to cast ");                                                  \
    } else {                                                                              \
        retValue = make_shared<IntegerValue>(this->GetValue() op value->GetValue());      \
    }                                                                                     \
} while (0)

#define INTEGER_FLOAT_MATH_COMPUTER(op, rightValue) do {                                  \
    FloatValue* value = static_cast<FloatValue *>((rightValue).get());                    \
    if (value == nullptr) {                                                               \
        USCRIPT_LOGE("Failed to cast ");                                                  \
    } else {                                                                              \
        retValue = make_shared<FloatValue>(this->GetValue() op value->GetValue());        \
    }                                                                                     \
} while (0)

#define INTEGER_MATH_COMPUTER(op, rightValue) do { \
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {                 \
        INTEGER_INTEGER_COMPUTER(op, (rightValue));                                       \
    } else if ((rightValue)->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {          \
        INTEGER_FLOAT_MATH_COMPUTER(op, (rightValue));                                    \
    }                                                                                     \
} while (0)

#define INTEGER_FLOAT_LOGIC_COMPUTER(op, rightValue) do {                                 \
    FloatValue* value = static_cast<FloatValue *>((rightValue).get());                    \
    if (value == nullptr) {                                                               \
        USCRIPT_LOGE("Failed to cast ");                                                  \
    } else {                                                                              \
        retValue = make_shared<IntegerValue>(this->GetValue() op value->GetValue());      \
    }                                                                                     \
} while (0)

#define INTEGER_LOGIC_COMPUTER(op, rightValue) do {                                       \
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {                 \
        INTEGER_INTEGER_COMPUTER(op, (rightValue));                                       \
    } else if ((rightValue)->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {          \
        INTEGER_FLOAT_LOGIC_COMPUTER(op, (rightValue));                                   \
    }                                                                                     \
} while (0)

#define INTEGER_INTEGER_MATH_COMPUTER_DIV(rightValue) do {                                \
    IntegerValue* value = static_cast<IntegerValue *>((rightValue).get());                  \
    if (value == nullptr || value->GetValue() == 0) {                                     \
        USCRIPT_LOGE("Failed to cast ");                                                  \
    } else {                                                                              \
        retValue = make_shared<IntegerValue>(this->GetValue() / value->GetValue());       \
    }                                                                                     \
} while (0)

#define INTEGER_FLOAT_MATH_COMPUTER_DIV(rightValue) do {                                  \
    FloatValue* value = static_cast<FloatValue *>((rightValue).get());                    \
    if (value == nullptr || value->GetValue() == 0) {                                     \
        USCRIPT_LOGE("Failed to cast ");                                                  \
    } else {                                                                              \
        retValue = make_shared<FloatValue>(this->GetValue() / value->GetValue());         \
    }                                                                                     \
} while (0)

#define INTEGER_MATH_COMPUTER_DIV(rightValue) do {                                        \
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {                 \
        INTEGER_INTEGER_MATH_COMPUTER_DIV((rightValue));                                  \
    } else if ((rightValue)->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {          \
        INTEGER_FLOAT_MATH_COMPUTER_DIV((rightValue));                                    \
    }                                                                                     \
} while (0)

UScriptValuePtr IntegerValue::Computer(int32_t action, UScriptValuePtr value)
{
    UScriptValuePtr rightValue = UScriptValue::GetRightCompluteValue(value);
    UScriptValuePtr retValue = std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
    USCRIPT_CHECK(rightValue != nullptr, return retValue, "Check param error");
    switch (action) {
        case UScriptExpression::ADD_OPERATOR: {
            INTEGER_MATH_COMPUTER(+, rightValue);
            break;
        }
        case UScriptExpression::SUB_OPERATOR: {
            INTEGER_MATH_COMPUTER(-, rightValue);
            break;
        }
        case UScriptExpression::MUL_OPERATOR: {
            INTEGER_MATH_COMPUTER(*, rightValue);
            break;
        }
        case UScriptExpression::DIV_OPERATOR: {
            INTEGER_MATH_COMPUTER_DIV(rightValue);
            break;
        }
        case UScriptExpression::GT_OPERATOR:
            INTEGER_LOGIC_COMPUTER(>, rightValue);
            break;
        case UScriptExpression::GE_OPERATOR:
            INTEGER_LOGIC_COMPUTER(>=, rightValue);
            break;
        case UScriptExpression::LT_OPERATOR:
            INTEGER_LOGIC_COMPUTER(<, rightValue);
            break;
        case UScriptExpression::LE_OPERATOR:
            INTEGER_LOGIC_COMPUTER(<=, rightValue);
            break;
        case UScriptExpression::EQ_OPERATOR:
            INTEGER_LOGIC_COMPUTER(==, rightValue);
            break;
        case UScriptExpression::NE_OPERATOR:
            INTEGER_LOGIC_COMPUTER(!=, rightValue);
            break;
        case UScriptExpression::AND_OPERATOR:
            INTEGER_LOGIC_COMPUTER(&&, rightValue);
            break;
        case UScriptExpression::OR_OPERATOR:
            INTEGER_LOGIC_COMPUTER(||, rightValue);
            break;
        default:
            break;
    }
    return retValue;
}

#define FLOAT_INTEGER_COMPUTER(op, rightValue) do {                                          \
    IntegerValue* value = static_cast<IntegerValue *>((rightValue).get());                   \
    if (value == nullptr) {                                                                  \
        USCRIPT_LOGE("Failed to cast ");                                                     \
    } else {                                                                                 \
        retValue = make_shared<FloatValue>(this->GetValue() op value->GetValue());           \
    }                                                                                        \
} while (0)

#define FLOAT_FLOAT_MATH_COMPUTER(op, rightValue) do {                                       \
    FloatValue* value = static_cast<FloatValue *>((rightValue).get());                       \
    if (value == nullptr) {                                                                  \
        USCRIPT_LOGE("Failed to cast ");                                                     \
    } else {                                                                                 \
        retValue = make_shared<FloatValue>(this->GetValue() op value->GetValue());           \
    }                                                                                        \
} while (0)

#define FLOAT_MATH_COMPUTER(op, rightValue) do {                                             \
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {                    \
        FLOAT_INTEGER_COMPUTER(op, (rightValue));                                            \
    } else if ((rightValue)->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {             \
        FLOAT_FLOAT_LOGIC_COMPUTER(op, (rightValue));                                        \
    }                                                                                        \
} while (0)

#define FLOAT_FLOAT_LOGIC_COMPUTER(op, rightValue) do {                                      \
    FloatValue* value = static_cast<FloatValue *>((rightValue).get());                       \
    if (value == nullptr) {                                                                  \
        USCRIPT_LOGE("Failed to cast ");                                                     \
    } else {                                                                                 \
        retValue = make_shared<IntegerValue>(this->GetValue() op value->GetValue());         \
    }                                                                                        \
} while (0)

#define FLOAT_LOGIC_COMPUTER(op, rightValue) do {                                            \
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {                    \
        FLOAT_INTEGER_COMPUTER(op, (rightValue));                                            \
    } else if ((rightValue)->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {             \
        FLOAT_FLOAT_LOGIC_COMPUTER(op, (rightValue));                                        \
    }                                                                                        \
} while (0)

#define FLOAT_INTEGER_MATH_COMPUTER_DIV(rightValue) do {                                     \
    IntegerValue* value = static_cast<IntegerValue *>((rightValue).get());                     \
    if (value == nullptr || value->GetValue() == 0) {                                        \
        USCRIPT_LOGE("Failed to cast ");                                                     \
    } else {                                                                                 \
        retValue = make_shared<FloatValue>(this->GetValue() / value->GetValue());            \
    }                                                                                        \
} while (0)

#define FLOAT_MATH_COMPUTER_DIV(rightValue) do {                                             \
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {                    \
        FLOAT_INTEGER_MATH_COMPUTER_DIV((rightValue));                                       \
    } else if ((rightValue)->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {             \
        INTEGER_FLOAT_MATH_COMPUTER_DIV((rightValue));                                       \
    }                                                                                        \
} while (0)


UScriptValuePtr FloatValue::Computer(int32_t action, UScriptValuePtr value)
{
    UScriptValuePtr rightValue = UScriptValue::GetRightCompluteValue(value);
    UScriptValuePtr retValue = std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
    USCRIPT_CHECK(rightValue != nullptr, return retValue, "Check param error");
    switch (action) {
        case UScriptExpression::ADD_OPERATOR: {
            FLOAT_MATH_COMPUTER(+, rightValue);
            break;
        }
        case UScriptExpression::SUB_OPERATOR: {
            FLOAT_MATH_COMPUTER(-, rightValue);
            break;
        }
        case UScriptExpression::MUL_OPERATOR: {
            FLOAT_MATH_COMPUTER(*, rightValue);
            break;
        }
        case UScriptExpression::DIV_OPERATOR: {
            FLOAT_MATH_COMPUTER_DIV(rightValue);
            break;
        }
        case UScriptExpression::GT_OPERATOR:
            FLOAT_LOGIC_COMPUTER(>, rightValue);
            break;
        case UScriptExpression::GE_OPERATOR:
            FLOAT_LOGIC_COMPUTER(>=, rightValue);
            break;
        case UScriptExpression::LT_OPERATOR:
            FLOAT_LOGIC_COMPUTER(<, rightValue);
            break;
        case UScriptExpression::LE_OPERATOR:
            FLOAT_LOGIC_COMPUTER(<=, rightValue);
            break;
        case UScriptExpression::EQ_OPERATOR:
            return make_shared<IntegerValue>(ComputerEqual(rightValue));
        case UScriptExpression::NE_OPERATOR:
            return make_shared<IntegerValue>(!ComputerEqual(rightValue));
        case UScriptExpression::AND_OPERATOR:
            FLOAT_LOGIC_COMPUTER(&&, rightValue);
            break;
        case UScriptExpression::OR_OPERATOR:
            FLOAT_LOGIC_COMPUTER(||, rightValue);
            break;
        default:
            break;
    }
    return retValue;
}

bool FloatValue::ComputerEqual(const UScriptValuePtr rightValue) const
{
    if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {
        IntegerValue* value = static_cast<IntegerValue*>(rightValue.get());
        USCRIPT_CHECK(value != nullptr, return 0, "Failed to cast ");
        float v2 = value->GetValue();
        USCRIPT_LOGI("ComputerEqual %f   v2: %f", GetValue(), v2);
        float diff = GetValue() - v2;
        diff = abs(diff);
        return diff < 0.0001f;
    } else if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {
        FloatValue* value = static_cast<FloatValue*>(rightValue.get());
        USCRIPT_CHECK(value != nullptr, return 0, "Failed to cast ");
        float diff = GetValue() - value->GetValue();
        diff = abs(diff);
        USCRIPT_LOGI("ComputerEqual %f %f diff: %f", GetValue(), value->GetValue(), diff);
        return diff < 0.0001f;
    }
    return 0;
}

UScriptValuePtr StringValue::ComputerReturn(int32_t action, UScriptValuePtr rightValue,
    UScriptValuePtr defReturn) const
{
    switch (action) {
        case UScriptExpression::GT_OPERATOR: {
            return make_shared<IntegerValue>(ComputerLogic(rightValue) > 0);
        }
        case UScriptExpression::GE_OPERATOR: {
            return make_shared<IntegerValue>(ComputerLogic(rightValue) >= 0);
        }
        case UScriptExpression::LT_OPERATOR: {
            return make_shared<IntegerValue>(ComputerLogic(rightValue) < 0);
        }
        case UScriptExpression::LE_OPERATOR: {
            return make_shared<IntegerValue>(ComputerLogic(rightValue) <= 0);
        }
        case UScriptExpression::EQ_OPERATOR: {
            return make_shared<IntegerValue>(ComputerLogic(rightValue) == 0);
        }
        case UScriptExpression::NE_OPERATOR: {
            return make_shared<IntegerValue>(ComputerLogic(rightValue) != 0);
        }
        case UScriptExpression::AND_OPERATOR:
            return defReturn;
        case UScriptExpression::OR_OPERATOR:
            return defReturn;
        default:
            break;
    }
    return std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
}

UScriptValuePtr StringValue::Computer(int32_t action, UScriptValuePtr value)
{
    UScriptValuePtr rightValue = UScriptValue::GetRightCompluteValue(value);
    UScriptValuePtr defReturn = std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
    USCRIPT_CHECK(rightValue != nullptr, return defReturn, "Check param error");

    std::string str;
    if (action == UScriptExpression::ADD_OPERATOR) {
        if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_INTEGER) {
            IntegerValue* integerValue = static_cast<IntegerValue*>(rightValue.get());
            USCRIPT_CHECK(integerValue != nullptr, return defReturn, "Failed to cast ");
            str.assign(this->GetValue());
            return make_shared<StringValue>(str + to_string(integerValue->GetValue()));
        } else if (rightValue->GetValueType() == UScriptValue::VALUE_TYPE_FLOAT) {
            FloatValue* floatValue = static_cast<FloatValue*>(rightValue.get());
            USCRIPT_CHECK(floatValue != nullptr, return defReturn, "Failed to cast ");
            str.assign(this->GetValue());
            return make_shared<StringValue>(str + to_string(floatValue->GetValue()));
        } else {
            StringValue* stringValue = static_cast<StringValue*>(rightValue.get());
            USCRIPT_CHECK(stringValue != nullptr, return defReturn, "Failed to cast ");
            str.assign(this->GetValue());
            return make_shared<StringValue>(str + stringValue->GetValue());
        }
    }
    if (rightValue->GetValueType() != UScriptValue::VALUE_TYPE_STRING) {
        return defReturn;
    }

    return ComputerReturn(action, rightValue, defReturn);
}

int32_t StringValue::ComputerLogic(UScriptValuePtr rightValue) const
{
    StringValue* value = static_cast<StringValue*>(rightValue.get());
    USCRIPT_CHECK(value != nullptr, return -1, "Failed to cast ");
    std::string str;
    str.assign(this->GetValue());
    return str.compare(value->GetValue());
}

std::string UScriptValue::ToString()
{
    return std::string("null");
}

UScriptValuePtr ReturnValue::Computer(int32_t action, UScriptValuePtr value)
{
    UScriptValuePtr defReturn = std::make_shared<ErrorValue>(USCRIPT_ERROR_INTERPRET);
    // 只支持一个返回值时参与数值计算
    if (values_.size() == 0 || values_.size() > 1) {
        return defReturn;
    }
    return values_[0]->Computer(action, value);
}

void ReturnValue::AddValue(const UScriptValuePtr value)
{
    if (value != nullptr) {
        values_.push_back(value);
    }
}

void ReturnValue::AddValues(const std::vector<UScriptValuePtr> values)
{
    for (auto out : values) {
        values_.push_back(out);
    }
}

std::vector<UScriptValuePtr> ReturnValue::GetValues() const
{
    return values_;
}

std::string IntegerValue::ToString()
{
    return to_string(value_);
}

std::string FloatValue::ToString()
{
    return to_string(value_);
}

std::string StringValue::ToString()
{
    return value_;
}

std::string ErrorValue::ToString()
{
    return to_string(retCode_);
}

std::string ReturnValue::ToString()
{
    std::string str;
    for (size_t index = 0; index < values_.size(); index++) {
        if (values_[index]->GetValueType() != VALUE_TYPE_RETURN) {
            str += " [" + to_string(index) + "] = " + values_[index]->ToString();
        } else {
            str += "error type";
        }
    }
    return str;
}

std::string UScriptValue::ScriptToString(UScriptValuePtr value)
{
    if (value == nullptr) {
        std::string str("null");
        return str;
    }
    static std::map<int8_t, std::string> typsMaps = {
        {VALUE_TYPE_INTEGER, "type: Integer "},
        {VALUE_TYPE_FLOAT, "type: Float "},
        {VALUE_TYPE_STRING, "type: String "},
        {VALUE_TYPE_ERROR, "type: Error "},
        {VALUE_TYPE_LIST, "type: List "},
        {VALUE_TYPE_RETURN, "type: Return "}
    };
    std::string str = typsMaps[value->GetValueType()];
    return str + value->ToString();
}

UScriptValuePtr UScriptValue::GetRightCompluteValue(UScriptValuePtr rightValue)
{
    if (rightValue->GetValueType() == VALUE_TYPE_LIST) {
        ReturnValue* value = static_cast<ReturnValue*>(rightValue.get());
        std::vector<UScriptValuePtr> retValues = value->GetValues();
        if (retValues.size() == 0 || retValues.size() > 1) {
            return nullptr;
        }
        return retValues[0];
    }
    return rightValue;
}
} // namespace Uscript
