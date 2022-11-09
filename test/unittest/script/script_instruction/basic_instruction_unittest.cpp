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
#include <gmock/gmock.h>
#include <iostream>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>
#include "log.h"
#include "script_context.h"
#include "script_basicinstruction.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "script/script_unittest.h"
#include "script_utils.h"
#include "thread_pool.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;
using namespace BasicInstruction;
using namespace Updater;
using namespace testing::ext;

namespace {
class MockUScriptEnv : public UScriptEnv {
public:
    explicit MockUScriptEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager) : UScriptEnv(pkgManager) {}
    MOCK_CONST_METHOD0(IsRetry, bool());
    MOCK_METHOD2(PostMessage, void(const std::string &cmd, std::string content));
    MOCK_METHOD0(GetInstructionFactory, UScriptInstructionFactoryPtr());
    MOCK_CONST_METHOD0(GetInstructionNames, const std::vector<std::string>());
};

class BasicInstructionUnittest : public ::testing::Test {
public:
    BasicInstructionUnittest() {}
    ~BasicInstructionUnittest() {}
    template<typename ... Args>
    static void AddInputParam(UScriptInstructionContext &ctx, Args && ... args)
    {
        [[maybe_unused]] auto li = { AddInputParamImpl(ctx, args)...};
    }
    static int32_t AddInputParamImpl(UScriptInstructionContext &ctx, const std::string &v)
    {
        return ctx.AddInputParam(std::make_shared<StringValue>(v));
    }
    template<typename T, std::enable_if_t<std::is_floating_point_v<T>, void *> = nullptr>
    static int32_t AddInputParamImpl(UScriptInstructionContext &ctx, T v)
    {
        return ctx.AddInputParam(std::make_shared<FloatValue>(v));
    }
    static int32_t AddInputParamImpl(UScriptInstructionContext &ctx, int32_t v)
    {
        return ctx.AddInputParam(std::make_shared<IntegerValue>(v));
    }
    void TestBasicInstructionIsSubString01() const
    {
        {
            MockUScriptEnv env {nullptr};
            UScriptInstructionContext context {};
            AddInputParam(context, "this is a test", "this is a test for is sub string");
            auto instruction = std::make_unique<UScriptInstructionIsSubString>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            ASSERT_EQ(output.size(), 1);
            EXPECT_EQ(output[0]->GetValueType(), UScriptValue::VALUE_TYPE_INTEGER);
            EXPECT_EQ(output[0]->ToString(), "0");
        }
        {
            MockUScriptEnv env {nullptr};
            UScriptInstructionContext context {};
            AddInputParam(context, "this is a test for is sub string", "this is a test");
            auto instruction = std::make_unique<UScriptInstructionIsSubString>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            ASSERT_EQ(output.size(), 1);
            EXPECT_EQ(output[0]->GetValueType(), UScriptValue::VALUE_TYPE_INTEGER);
            EXPECT_EQ(output[0]->ToString(), "1");
        }
    }
    void TestBasicInstructionIsSubString02() const
    {
        {
            MockUScriptEnv env {nullptr};
            UScriptInstructionContext context {};
            context.AddInputParam(std::make_shared<StringValue>("this is a test"));
            auto instruction = std::make_unique<UScriptInstructionIsSubString>();
            EXPECT_EQ(instruction->Execute(env, context), UScriptContext::PARAM_TYPE_INVALID);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            EXPECT_EQ(output.size(), 0);
        }
        {
            MockUScriptEnv env {nullptr};
            UScriptInstructionContext context {};
            AddInputParam(context, "this is a test", 1);
            auto instruction = std::make_unique<UScriptInstructionIsSubString>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            EXPECT_EQ(output.size(), 0);
        }
    }
    void TestBasicInstructionStdout() const
    {
        MockUScriptEnv env {nullptr};
        UScriptInstructionContext context {};
        auto input = std::make_tuple("string1", 1, 1.0, 1.0001, 1.00000001);
        auto instruction = std::make_unique<UScriptInstructionStdout>();
        std::stringstream buffer;
        std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
        std::stringstream tgt;
        std::apply([&tgt] (auto && ... args) {
            ((tgt << args << "  "), ...);
            tgt << std::endl;
            }, input);
        std::apply([&context] (auto && ... args) {
            AddInputParam(context, args...);
            }, input);
        EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 0);
        EXPECT_EQ(buffer.str(), tgt.str());
        std::cout.rdbuf(old);
    }
    void TestBasicInstructionConcat() const
    {
        MockUScriptEnv env {nullptr};
        {
            UScriptInstructionContext context {};
            auto input = std::make_tuple("this is a test",  "test2", 1, 1.0, 1.0001, 1.00000001);
            auto instruction = std::make_unique<UScriptInstructionConcat>();
            std::stringstream tgt;
            auto toString = [] (auto &&arg) -> std::string {
                using T = std::remove_cv_t<std::remove_reference_t<decltype(arg)>>;
                if constexpr (std::is_same_v<std::string, T> ||
                    std::is_same_v<const char *, T> ||
                    std::is_same_v<char *, T>) {
                    return arg;
                } else {
                    return std::to_string(arg);
                }
            };
            std::apply([&toString, &tgt] (auto && ... args) {
                ((tgt << toString(args)), ...);
                }, input);
            std::apply([&context] (auto && ... args) {
                AddInputParam(context, args...);
                }, input);
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            ASSERT_EQ(output.size(), 1);
            EXPECT_EQ(output[0]->GetValueType(), UScriptValue::VALUE_TYPE_STRING);
            EXPECT_EQ(output[0]->ToString(), tgt.str());
        }
        {
            UScriptInstructionContext context {};
            auto instruction = std::make_unique<UScriptInstructionConcat>();
            EXPECT_EQ(instruction->Execute(env, context), UScriptContext::PARAM_TYPE_INVALID);
        }
        {
            UScriptInstructionContext context1 {};
            AddInputParam(context1, 1);
            auto instruction1 = std::make_unique<UScriptInstructionConcat>();
            EXPECT_EQ(instruction1->Execute(env, context1), USCRIPT_INVALID_PARAM);
            UScriptInstructionContext context2 {};
            AddInputParam(context2, "test");
            auto instruction2 = std::make_unique<UScriptInstructionConcat>();
            EXPECT_EQ(instruction2->Execute(env, context2), USCRIPT_SUCCESS);
            std::vector<UScriptValuePtr> output = context2.GetOutVar();
            ASSERT_EQ(output.size(), 1);
            EXPECT_EQ(output[0]->GetValueType(), UScriptValue::VALUE_TYPE_STRING);
            EXPECT_EQ(output[0]->ToString(), "test");
        }
    }
    void TestBasicInstructionAbort() const
    {
        MockUScriptEnv env {nullptr};
        {
            UScriptInstructionContext context {};
            auto instruction = std::make_unique<UScriptInstructionAbort>();
            EXPECT_EQ(instruction->Execute(env, context), UScriptContext::PARAM_TYPE_INVALID);
        }
        {
            UScriptInstructionContext context {};
            AddInputParam(context, 1.0);
            auto instruction = std::make_unique<UScriptInstructionSleep>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
        }
        {
            UScriptInstructionContext context {};
            AddInputParam(context, 1);
            auto instruction = std::make_unique<UScriptInstructionAbort>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
        }
        {
            UScriptInstructionContext context {};
            AddInputParam(context, 0);
            auto instruction = std::make_unique<UScriptInstructionAbort>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_ABOART);
        }
    }
    void TestBasicInstructionAssert() const
    {
        MockUScriptEnv env {nullptr};
        {
            UScriptInstructionContext context {};
            auto instruction = std::make_unique<UScriptInstructionAssert>();
            EXPECT_EQ(instruction->Execute(env, context), UScriptContext::PARAM_TYPE_INVALID);
        }
        {
            UScriptInstructionContext context {};
            AddInputParam(context, 1);
            auto instruction = std::make_unique<UScriptInstructionAssert>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
        }
        {
            UScriptInstructionContext context {};
            AddInputParam(context, 0);
            auto instruction = std::make_unique<UScriptInstructionAssert>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_ASSERT);
        }
    }
    void TestBasicInstructionSleep() const
    {
        MockUScriptEnv env {nullptr};
        {
            UScriptInstructionContext context {};
            auto instruction = std::make_unique<UScriptInstructionSleep>();
            EXPECT_EQ(instruction->Execute(env, context), UScriptContext::PARAM_TYPE_INVALID);
        }
        {
            UScriptInstructionContext context {};
            AddInputParam(context, 0);
            auto instruction = std::make_unique<UScriptInstructionSleep>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
        }
    }
protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionIsSubString01, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionIsSubString01();
}

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionIsSubString02, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionIsSubString02();
}

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionStdout, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionStdout();
}

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionConcat, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionConcat();
}

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionAbort, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionAbort();
}

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionAssert, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionAssert();
}

HWTEST_F(BasicInstructionUnittest, TestBasicInstructionSleep, TestSize.Level1)
{
    BasicInstructionUnittest test;
    test.TestBasicInstructionSleep();
}
}
