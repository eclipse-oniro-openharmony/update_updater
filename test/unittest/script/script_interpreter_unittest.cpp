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
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "script_context.h"
#include "script_expression.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "unittest_comm.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;
using namespace Updater;
using namespace testing::ext;

namespace {
class ScriptInterpreterUnitTest : public ::testing::Test {
public:
    ScriptInterpreterUnitTest() {}
    ~ScriptInterpreterUnitTest() {}

    int TestScriptInterpreterScriptValue() const
    {
        int32_t action = 0;
        UScriptValuePtr rightValue = std::make_shared<UScriptValue>(UScriptValue::VALUE_TYPE_RETURN);
        std::unique_ptr<UScriptValue> value = std::make_unique<UScriptValue>(UScriptValue::VALUE_TYPE_RETURN);
        if (value == nullptr) {
            return -1;
        }
        value->Computer(action, rightValue);
        value->IsTrue();
        std::string tmpStr = value->ToString();
        EXPECT_STREQ("null", tmpStr.c_str());
        value.reset();

        int32_t intValue = 10;
        std::unique_ptr<IntegerValue> iv = std::make_unique<IntegerValue>(intValue);
        if (iv == nullptr) {
            return -1;
        }
        iv->IsTrue();
        iv.reset();

        float floatValue = 10.0;
        std::unique_ptr<FloatValue> fv = std::make_unique<FloatValue>(floatValue);
        if (fv == nullptr) {
            return -1;
        }
        fv->IsTrue();
        fv.reset();

        std::unique_ptr<StringValue> sv = std::make_unique<StringValue>("2222222222222");
        if (sv == nullptr) {
            return -1;
        }
        sv->IsTrue();
        sv.reset();

        std::unique_ptr<ReturnValue> rv = std::make_unique<ReturnValue>();
        if (rv == nullptr) {
            return -1;
        }
        rv->IsTrue();
        rv->Computer(action, rightValue);
        rv->ToString();
        rv.reset();

        constexpr int32_t retCode = 100;
        std::unique_ptr<ErrorValue> ev = std::make_unique<ErrorValue>(retCode);
        if (ev == nullptr) {
            return -1;
        }
        ev->IsTrue();
        ev->ToString();
        ev->Computer(action, rightValue);
        ev.reset();
        return 0;
    }

    int TestScriptInstructionContext() const
    {
        std::unique_ptr<UScriptInstructionContext> funcContext = std::make_unique<UScriptInstructionContext>();
        int intValue = 100;
        int ret1 = funcContext->PushParam(intValue);
        float floatValue = 100.0;
        int ret2 = funcContext->PushParam(floatValue);
        std::string str = std::string("333333333");
        int ret3 = funcContext->PushParam(str);
        EXPECT_EQ(0, ret1 || ret2 || ret3);

        int32_t outOfIndex = 3;
        int ret = funcContext->GetParamType(outOfIndex);
        EXPECT_EQ(UScriptContext::PARAM_TYPE_INVALID, ret);
        return 0;
    }

    int TestIntegerValueComputer() const
    {
        int intValue = 100;
        UScriptValuePtr rightValue = std::make_shared<IntegerValue>(0);
        UScriptValuePtr value = std::make_shared<IntegerValue>(intValue);
        if (value == nullptr || rightValue == nullptr) {
            return -1;
        }
        value->Computer(UScriptExpression::DIV_OPERATOR, rightValue);
        // 除浮点数
        float floatValue = 100.0;
        UScriptValuePtr rightValue2 = std::make_shared<FloatValue>(floatValue);
        value->Computer(UScriptExpression::DIV_OPERATOR, rightValue2);

        // 除浮点数
        UScriptValuePtr rightValue3 = std::make_shared<FloatValue>(0);
        value->Computer(UScriptExpression::DIV_OPERATOR, rightValue3);

        int32_t action = 100;
        value->Computer(action, rightValue3);
        return 0;
    }

    int TestFloatValueComputer() const
    {
        float floatValue = 100;
        UScriptValuePtr rightValue = std::make_shared<IntegerValue>(0);
        UScriptValuePtr value = std::make_shared<FloatValue>(floatValue);
        if (value == nullptr || rightValue == nullptr) {
            return -1;
        }
        value->Computer(UScriptExpression::DIV_OPERATOR, rightValue);
        // 除浮点数
        UScriptValuePtr rightValue2 = std::make_shared<FloatValue>(floatValue);
        value->Computer(UScriptExpression::DIV_OPERATOR, rightValue2);

        // 除浮点数
        UScriptValuePtr rightValue3 = std::make_shared<FloatValue>(0);
        value->Computer(UScriptExpression::DIV_OPERATOR, rightValue3);

        value->Computer(UScriptExpression::EQ_OPERATOR, rightValue3);
        return 0;
    }

    int TestStringValueComputer() const
    {
        UScriptValuePtr rightValue = std::make_shared<IntegerValue>(0);
        UScriptValuePtr value = std::make_shared<StringValue>("100");
        if (value == nullptr || rightValue == nullptr) {
            return -1;
        }
        value->Computer(UScriptExpression::ADD_OPERATOR, rightValue);

        float floatValue = 100.0;
        UScriptValuePtr rightValue2 = std::make_shared<FloatValue>(floatValue);
        value->Computer(UScriptExpression::ADD_OPERATOR, rightValue2);

        UScriptValuePtr rightValue3 = std::make_shared<StringValue>("666666");
        value->Computer(UScriptExpression::ADD_OPERATOR, rightValue3);

        // string 比较
        value->Computer(UScriptExpression::EQ_OPERATOR, rightValue2);

        value->Computer(UScriptExpression::GT_OPERATOR, rightValue3);
        value->Computer(UScriptExpression::GE_OPERATOR, rightValue3);
        value->Computer(UScriptExpression::LT_OPERATOR, rightValue3);
        value->Computer(UScriptExpression::LE_OPERATOR, rightValue3);
        value->Computer(UScriptExpression::EQ_OPERATOR, rightValue3);
        value->Computer(UScriptExpression::NE_OPERATOR, rightValue3);
        return 0;
    }

protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(ScriptInterpreterUnitTest, TestScriptInterpreterScriptValue, TestSize.Level0)
{
    ScriptInterpreterUnitTest test;
    EXPECT_EQ(0, test.TestScriptInterpreterScriptValue());
}

HWTEST_F(ScriptInterpreterUnitTest, TestScriptInstructionContext, TestSize.Level0)
{
    ScriptInterpreterUnitTest test;
    EXPECT_EQ(0, test.TestScriptInstructionContext());
}

HWTEST_F(ScriptInterpreterUnitTest, TestIntegerValueComputer, TestSize.Level0)
{
    ScriptInterpreterUnitTest test;
    EXPECT_EQ(0, test.TestIntegerValueComputer());
}

HWTEST_F(ScriptInterpreterUnitTest, TestFloatValueComputer, TestSize.Level0)
{
    ScriptInterpreterUnitTest test;
    EXPECT_EQ(0, test.TestFloatValueComputer());
}

HWTEST_F(ScriptInterpreterUnitTest, TestStringValueComputer, TestSize.Level0)
{
    ScriptInterpreterUnitTest test;
    EXPECT_EQ(0, test.TestStringValueComputer());
}

HWTEST_F(ScriptInterpreterUnitTest, SomeDestructor, TestSize.Level0)
{
    IntegerValue a1(0);
    FloatValue a2(0.0);
    ErrorValue a3(0);
    char a5[1000] = {1};
    UScriptContextPtr local;
    UScriptExpression a4(UScriptExpression::EXPRESSION_TYPE_INTERGER);
    a4.Execute(*(ScriptInterpreter*)a5, local);
    // just for executing these destructor
    IntegerValue *a6 = new IntegerValue(1);
    bool b1 = a6->IsTrue();
    delete a6;
    FloatValue *a7 = new FloatValue(0.0);
    delete a7;
    ErrorValue *a8 = new ErrorValue(0);
    delete a8;
    EXPECT_TRUE(b1);
}
}
