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

#include <array>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "script_basicinstruction.h"
#include "script_instruction_unittest.h"
#include "script_instructionhelper.h"
#include "script_manager.h"
#include "script/script_unittest.h"
#include "script_utils.h"
#include "thread_pool.h"
#include "unittest_comm.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;
using namespace Updater;
using namespace testing::ext;

namespace {

class TestInstruction1 : public UScriptInstruction {
public:
    int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        return USCRIPT_SUCCESS;
    }
};

class TestInstruction2 : public UScriptInstruction {
public:
    int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        return USCRIPT_SUCCESS;
    }
};

class TestInstruction3 : public UScriptInstruction {
public:
    int32_t Execute(UScriptEnv &env, UScriptContext &context) override
    {
        return USCRIPT_SUCCESS;
    }
};

class TestInstructionFactory : public UScriptInstructionFactory {
public:
    int32_t CreateInstructionInstance(UScriptInstructionPtr& instr, const std::string& name) override
    {
        if (name == "test1") {
            instr = new (std::nothrow) TestInstruction1();
        } else if (name == "test2") {
            instr = new (std::nothrow) TestInstruction2();
        } else if (name == "test3") {
            instr = nullptr; // mock new memory for Instruction failed scene
        } else if (name == "abort") {
            instr = new (std::nothrow) BasicInstruction::UScriptInstructionAbort(); // mock reserved error
        } else {
            return USCRIPT_NOTEXIST_INSTRUCTION;
        }
        return USCRIPT_SUCCESS;
    }
    TestInstructionFactory() {}
    virtual ~TestInstructionFactory() {}
};

class TestPkgManager : public TestScriptPkgManager {
public:
    int32_t ExtractFile(const std::string &fileId, StreamPtr output) override
    {
        return 0;
    }
    const FileInfo *GetFileInfo(const std::string &fileId) override
    {
        const static std::unordered_map<std::string, FileInfo> fileMap {
            {"script1", FileInfo {}},
            {"script2", FileInfo {}},
        };
        if (auto it = fileMap.find(fileId); it != fileMap.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

class ScriptInstructionHelperUnitTest : public ::testing::Test {
public:
    ScriptInstructionHelperUnitTest()
    {
        factory_ = std::make_unique<TestInstructionFactory>();
    }
    ~ScriptInstructionHelperUnitTest() {}
    void TestGetBasicInstructionHelper() const
    {
        EXPECT_EQ(ScriptInstructionHelper::GetBasicInstructionHelper(nullptr), nullptr);
        UTestScriptEnv env {nullptr};
        ScriptInstructionHelper *instructionHelper = nullptr;
        auto impl = std::make_unique<ScriptManagerImpl>(&env);
        {
            instructionHelper = ScriptInstructionHelper::GetBasicInstructionHelper(impl.get());
            EXPECT_NE(instructionHelper, nullptr);
            EXPECT_EQ(ScriptInstructionHelper::GetBasicInstructionHelper(impl.get()), instructionHelper);
            EXPECT_EQ(ScriptInstructionHelper::GetBasicInstructionHelper(nullptr), instructionHelper);
        }
        ScriptInstructionHelper::ReleaseBasicInstructionHelper();
        EXPECT_EQ(ScriptInstructionHelper::GetBasicInstructionHelper(nullptr), nullptr);
    }
    void TestRegisterInstructions() const
    {
        UTestScriptEnv env {nullptr};
        auto impl = std::make_unique<ScriptManagerImpl>(&env);
        ScriptInstructionHelper helper(impl.get());
        EXPECT_EQ(helper.RegisterInstructions(), USCRIPT_SUCCESS);
    }
    void TestIsReservedInstruction() const
    {
        UTestScriptEnv env {nullptr};
        auto impl = std::make_unique<ScriptManagerImpl>(&env);
        auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(impl.get());
        std::array reservedInstructions {"LoadScript", "RegisterCmd", "abort", "assert", "concat",
        "is_substring", "stdout", "sleep", "set_progress", "ui_print",
        "show_progress"};
        for (auto instruction : reservedInstructions) {
            EXPECT_TRUE(helper->IsReservedInstruction(instruction));
        }
        EXPECT_FALSE(helper->IsReservedInstruction("non reserved"));
    }
    void TestAddInstruction() const
    {
        UTestScriptEnv env {nullptr};
        auto impl = std::make_unique<ScriptManagerImpl>(&env);
        auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(impl.get());
        std::array reservedInstructions {"LoadScript", "RegisterCmd", "abort", "assert", "concat",
        "is_substring", "stdout", "sleep", "set_progress", "ui_print",
        "show_progress"};
        for (auto instruction : reservedInstructions) {
            EXPECT_EQ(helper->AddInstruction(instruction, nullptr), USCRIPT_ERROR_REVERED);
        }
        UScriptInstructionPtr instr1 = nullptr;
        EXPECT_EQ(factory_->CreateInstructionInstance(instr1, "test1"), USCRIPT_SUCCESS);
        EXPECT_EQ(helper->AddInstruction("instruction1", instr1), USCRIPT_SUCCESS);
        UScriptInstructionPtr instr2 = nullptr;
        EXPECT_EQ(factory_->CreateInstructionInstance(instr2, "test2"), USCRIPT_SUCCESS);
        EXPECT_EQ(helper->AddInstruction("instruction1", instr2), USCRIPT_SUCCESS);
    }
    void TestAddScript() const
    {
        // no pkg manager in env
        {
            UTestScriptEnv env {nullptr};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->AddScript("", 0), USCRIPT_INVALID_PARAM);
        }
        // priority invalid
        TestPkgManager pkgManager;
        {
            UTestScriptEnv env {&pkgManager};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->AddScript("", -1), USCRIPT_INVALID_PRIORITY);
            EXPECT_EQ(helper->AddScript("", 4), USCRIPT_INVALID_PRIORITY);
        }
        // successfully add script
        {
            UTestScriptEnv env {&pkgManager};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->AddScript("script0", 0), USCRIPT_INVALID_SCRIPT);
            EXPECT_EQ(helper->AddScript("script1", 1), USCRIPT_SUCCESS);
            EXPECT_EQ(helper->AddScript("", 1), USCRIPT_INVALID_SCRIPT);
        }
    }
    void TestRegisterUserInstruction01() const
    {
        // empty factory
        {
            UTestScriptEnv env {nullptr};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction("", nullptr), USCRIPT_INVALID_PARAM);
        }
        // invalid instruction ""
        {
            UTestScriptEnv env {nullptr};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction("", factory_.get()), USCRIPT_NOTEXIST_INSTRUCTION);
        }
        // test new failed, register reserved instr, register non-exist instr scene
        {
            UTestScriptEnv env {nullptr};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction("test3", factory_.get()), USCRIPT_ERROR_CREATE_OBJ);
            EXPECT_EQ(helper->RegisterUserInstruction("test4", factory_.get()), USCRIPT_NOTEXIST_INSTRUCTION);
            EXPECT_EQ(helper->RegisterUserInstruction("abort", factory_.get()), USCRIPT_ERROR_REVERED);
            EXPECT_EQ(helper->RegisterUserInstruction("test2", factory_.get()), USCRIPT_SUCCESS);
            ScriptInstructionHelper::ReleaseBasicInstructionHelper();
        }
    }
    void TestRegisterUserInstruction02() const
    {
        {
            UTestScriptEnv env {nullptr};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction("", ""), USCRIPT_INVALID_PARAM);
        }
        {
            UTestScriptEnv env {nullptr};
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction("noexist", ""), USCRIPT_INVALID_PARAM);
        }
        {
            UTestScriptEnv env {nullptr};{
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_INVALID_LIB_PATH, ""), USCRIPT_ERROR_CREATE_OBJ);}
        }
        {
            UTestScriptEnv env {nullptr};{
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_VALID_LIB_PATH, ""), USCRIPT_NOTEXIST_INSTRUCTION);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_NONEXIST_LIB_PATH, ""), USCRIPT_INVALID_PARAM);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_INVALID_LIB_PATH, ""), USCRIPT_INVALID_PARAM);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_VALID_LIB_PATH, "uInstruction1"), USCRIPT_SUCCESS);}
        }
        {
            UTestScriptEnv env {nullptr};{
            ScriptManagerImpl scriptManager(&env);
            auto *helper = ScriptInstructionHelper::GetBasicInstructionHelper(&scriptManager);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_VALID_LIB_PATH, ""), USCRIPT_NOTEXIST_INSTRUCTION);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_NONEXIST_LIB_PATH, ""), USCRIPT_INVALID_PARAM);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_VALID_LIB_PATH, "uInstruction1"), USCRIPT_SUCCESS);
            EXPECT_EQ(helper->RegisterUserInstruction(TEST_VALID_LIB_PATH, "abort"), USCRIPT_ERROR_REVERED);}
        }
    }
protected:
    std::unique_ptr<TestInstructionFactory> factory_;
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(ScriptInstructionHelperUnitTest, TestGetBasicInstructionHelper, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestGetBasicInstructionHelper();
}

HWTEST_F(ScriptInstructionHelperUnitTest, TestRegisterInstructions, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestRegisterInstructions();
}

HWTEST_F(ScriptInstructionHelperUnitTest, TestIsReservedInstruction, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestIsReservedInstruction();
}

HWTEST_F(ScriptInstructionHelperUnitTest, TestAddInstruction, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestAddInstruction();
}

HWTEST_F(ScriptInstructionHelperUnitTest, TestAddScript, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestAddScript();
}

HWTEST_F(ScriptInstructionHelperUnitTest, TestRegisterUserInstruction01, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestRegisterUserInstruction01();
}

HWTEST_F(ScriptInstructionHelperUnitTest, TestRegisterUserInstruction02, TestSize.Level1)
{
    ScriptInstructionHelperUnitTest test;
    test.TestRegisterUserInstruction02();
}
}
