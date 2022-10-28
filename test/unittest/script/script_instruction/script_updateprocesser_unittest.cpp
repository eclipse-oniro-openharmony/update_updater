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

#include <gtest/gtest.h>
#include <vector>
#include "script_context.h"
#include "script_instruction.h"
#include "script_instruction_unittest.h"
#include "script_instructionhelper.h"
#include "script_updateprocesser.h"
#include "script_manager_impl.h"
#include "script/script_unittest.h"

using namespace Uscript;
using namespace BasicInstruction;
using namespace Hpackage;
using namespace testing::ext;

namespace {
class UTestProcessorScriptEnv : public UTestScriptEnv {
public:
    explicit UTestProcessorScriptEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager) : UTestScriptEnv(pkgManager)
    {}
    ~UTestProcessorScriptEnv() = default;

    virtual void PostMessage(const std::string &cmd, std::string content)
    {
        message_ = cmd + " " + content;
    }
    std::string GetPostMessage()
    {
        return message_;
    }
private:
    std::string message_ {};
};

class TestPkgManager : public TestScriptPkgManager {
public:
    int32_t ExtractFile(const std::string &fileId, StreamPtr output) override
    {
        return 0;
    }
    const FileInfo *GetFileInfo(const std::string &fileId) override
    {
        static FileInfo fileInfo {};
        if (fileId == "script") {
            return &fileInfo;
        }
        return nullptr;
    }
};

class UpdateProcesserInstructionUnittest : public ::testing::Test {
public:
    UpdateProcesserInstructionUnittest() {}
    ~UpdateProcesserInstructionUnittest() {}
    void TestUpdateProcesserSetProcess() const
    {
        {
            TestPkgManager pkgManager;
            UTestProcessorScriptEnv env {&pkgManager};
            ScriptManagerImpl scriptManager {&env};
            ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            UScriptInstructionContext context {};
            context.AddInputParam(std::make_shared<IntegerValue>(1));
            auto instruction = std::make_unique<UScriptInstructionSetProcess>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            EXPECT_EQ(output.size(), 0);
            ScriptInstructionHelper::ReleaseBasicInstructionHelper();
        }
        {
            TestPkgManager pkgManager;
            UTestProcessorScriptEnv env {&pkgManager};
            ScriptManagerImpl scriptManager {&env};
            ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            UScriptInstructionContext context {};
            constexpr float progress = 1.0f;
            context.AddInputParam(std::make_shared<FloatValue>(progress));
            auto instruction = std::make_unique<UScriptInstructionSetProcess>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            EXPECT_EQ(output.size(), 0);
            std::stringstream ss;
            ss << "set_progress " << progress;
            EXPECT_EQ(env.GetPostMessage(), ss.str());
            ScriptInstructionHelper::ReleaseBasicInstructionHelper();
        }
    }
    void TestUpdateProcesserShowProcess() const
    {
        {
            TestPkgManager pkgManager;
            UTestProcessorScriptEnv env {&pkgManager};
            ScriptManagerImpl scriptManager {&env};
            ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            {
                UScriptInstructionContext context {};
                context.AddInputParam(std::make_shared<IntegerValue>(1));
                auto instruction = std::make_unique<UScriptInstructionShowProcess>();
                EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
                std::vector<UScriptValuePtr> output = context.GetOutVar();
                EXPECT_EQ(output.size(), 0);
            }
            {
                UScriptInstructionContext context {};
                context.AddInputParam(std::make_shared<FloatValue>(1));
                context.AddInputParam(std::make_shared<IntegerValue>(1));
                auto instruction = std::make_unique<UScriptInstructionShowProcess>();
                EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
                std::vector<UScriptValuePtr> output = context.GetOutVar();
                EXPECT_EQ(output.size(), 0);
            }
            {
                UScriptInstructionContext context {};
                constexpr float start = 1.0f;
                constexpr float end = 2.0f;
                context.AddInputParam(std::make_shared<FloatValue>(start));
                context.AddInputParam(std::make_shared<FloatValue>(end));
                auto instruction = std::make_unique<UScriptInstructionShowProcess>();
                EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
                std::vector<UScriptValuePtr> output = context.GetOutVar();
                EXPECT_EQ(output.size(), 0);
                std::stringstream ss;
                ss << "show_progress " << start << "," << end;
                EXPECT_EQ(env.GetPostMessage(), ss.str());
            }
            ScriptInstructionHelper::ReleaseBasicInstructionHelper();
        }
    }
    void TestUpdateProcesserPrint() const
    {
        TestPkgManager pkgManager;
        UTestProcessorScriptEnv env {&pkgManager};
        ScriptManagerImpl scriptManager {&env};
        ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
        {
            UScriptInstructionContext context {};
            context.AddInputParam(std::make_shared<IntegerValue>(1));
            auto instruction = std::make_unique<UScriptInstructionUiPrint>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            EXPECT_EQ(output.size(), 0);
        }
        {
            UScriptInstructionContext context {};
            constexpr auto content = "content";
            context.AddInputParam(std::make_shared<StringValue>(content));
            auto instruction = std::make_unique<UScriptInstructionUiPrint>();
            EXPECT_EQ(instruction->Execute(env, context), USCRIPT_SUCCESS);
            std::vector<UScriptValuePtr> output = context.GetOutVar();
            EXPECT_EQ(output.size(), 0);
            std::stringstream ss;
            ss << "ui_log " << content;
            EXPECT_EQ(env.GetPostMessage(), ss.str());
        }
        ScriptInstructionHelper::ReleaseBasicInstructionHelper();
    }
protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(UpdateProcesserInstructionUnittest, TestUpdateProcesserSetProcess, TestSize.Level1)
{
    UpdateProcesserInstructionUnittest {}.TestUpdateProcesserSetProcess();
}

HWTEST_F(UpdateProcesserInstructionUnittest, TestUpdateProcesserShowProcess, TestSize.Level1)
{
    UpdateProcesserInstructionUnittest {}.TestUpdateProcesserShowProcess();
}

HWTEST_F(UpdateProcesserInstructionUnittest, TestUpdateProcesserPrint, TestSize.Level1)
{
    UpdateProcesserInstructionUnittest {}.TestUpdateProcesserPrint();
}
}