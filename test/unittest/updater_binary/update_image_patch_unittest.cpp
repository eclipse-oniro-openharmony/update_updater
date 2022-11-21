/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <vector>
#include "update_binary_unittest.h"
#include "script_context.h"
#include "update_image_patch.h"
#include "mount.h"

using namespace Uscript;
using namespace Updater;
using namespace testing::ext;
using namespace Hpackage;

namespace UpdaterUt {
class ImgPatchUnittest : public ::testing::Test {
public:
    ImgPatchUnittest() {}
    ~ImgPatchUnittest() {}

    /* GetParam(): 脚本指令image_patch，参数个数为IMAGE_PATCH_CMD_LEN:6，入参为空 */
    void TestImgPatch01()
    {
        UTestBinaryEnv env {&pkgManager_};
        UScriptInstructionContext context {};
        auto instruction = std::make_unique<USInstrImagePatch>();
        EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 1);
    }

    /* GetParam(): 指令入参参数格式不对 */
    void TestImgPatch02()
    {
        UTestBinaryEnv env {&pkgManager_};
        UScriptInstructionContext context {};
        context.AddInputParam(std::make_shared<IntegerValue>(1));
        context.AddInputParam(std::make_shared<IntegerValue>(1));
        context.AddInputParam(std::make_shared<IntegerValue>(1));
        context.AddInputParam(std::make_shared<IntegerValue>(1));
        context.AddInputParam(std::make_shared<IntegerValue>(1));
        context.AddInputParam(std::make_shared<StringValue>("/data/updater/patchfile"));
        auto instruction = std::make_unique<USInstrImagePatch>();
        EXPECT_EQ(instruction->Execute(env, context), USCRIPT_INVALID_PARAM);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 1);
    }

    /* GetParam(): 根据指令入参解析fstab设备文件失败 */
    void TestImgPatch03()
    {
        UTestBinaryEnv env {&pkgManager_};
        UScriptInstructionContext context {};
        context.AddInputParam(std::make_shared<StringValue>(""));
        context.AddInputParam(std::make_shared<StringValue>("srcSize"));
        context.AddInputParam(std::make_shared<StringValue>("srcHash"));
        context.AddInputParam(std::make_shared<StringValue>("destSize"));
        context.AddInputParam(std::make_shared<StringValue>("destHash"));
        context.AddInputParam(std::make_shared<StringValue>("/data/updater/patchfile"));
        auto instruction = std::make_unique<USInstrImagePatch>();
        EXPECT_EQ(instruction->Execute(env, context), USCRIPT_ERROR_EXECUTE);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 1);
    }

    /* GetParam(): 解析命令行参数成功 */
    void TestImgPatch04()
    {
        UTestBinaryEnv env {&pkgManager_};
        UScriptInstructionContext context {};
        context.AddInputParam(std::make_shared<StringValue>("/vendortest"));
        context.AddInputParam(std::make_shared<StringValue>("srcSize"));
        context.AddInputParam(std::make_shared<StringValue>("srcHash"));
        context.AddInputParam(std::make_shared<StringValue>("destSize"));
        context.AddInputParam(std::make_shared<StringValue>("destHash"));
        context.AddInputParam(std::make_shared<StringValue>("/data/updater/patchfile"));
        auto instruction = std::make_unique<USInstrImagePatch>();
        EXPECT_EQ(instruction->Execute(env, context), USCRIPT_ERROR_EXECUTE);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 1);
    }

    /* GetPatchFile(): retry为true, 获取文件失败, 创建文件流失败 */
    void TestImgPatch05()
    {
        PkgManager::PkgManagerPtr pkgMgr = nullptr;
        UTestBinaryEnv env1 {pkgMgr};
        env1.SetRetry(true);
        UScriptInstructionContext context {};
        context.AddInputParam(std::make_shared<StringValue>("/vendortest"));
        context.AddInputParam(std::make_shared<StringValue>("srcSize"));
        context.AddInputParam(std::make_shared<StringValue>("srcHash"));
        context.AddInputParam(std::make_shared<StringValue>("destSize"));
        context.AddInputParam(std::make_shared<StringValue>("destHash"));
        context.AddInputParam(std::make_shared<StringValue>("/data/updater/patchfile"));
        auto instruction = std::make_unique<USInstrImagePatch>();
        EXPECT_EQ(instruction->Execute(env1, context), USCRIPT_ERROR_EXECUTE);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 1);

        UTestBinaryEnv env2(&pkgManager_);
        EXPECT_EQ(instruction->Execute(env2, context), USCRIPT_ERROR_EXECUTE);

        TestPkgMgrStream1 pkgStream1;
        UTestBinaryEnv env3(&pkgStream1);
        EXPECT_EQ(instruction->Execute(env3, context), USCRIPT_ERROR_EXECUTE);

        TestPkgMgrStream2 pkgStream2;
        UTestBinaryEnv env4(&pkgStream2);
        EXPECT_EQ(instruction->Execute(env4, context), USCRIPT_ERROR_EXECUTE);

        TestPkgMgrExtract1 pkgExtract1;
        UTestBinaryEnv env5(&pkgExtract1);
        EXPECT_EQ(instruction->Execute(env5, context), USCRIPT_ERROR_EXECUTE);
    }

    /* GetPatchFile(): 获取文件成功，patchfile=/data/udpater/binary */
    void TestImgPatch06()
    {
        UTestBinaryEnv env {&pkgManager_};
        UScriptInstructionContext context {};
        context.AddInputParam(std::make_shared<StringValue>("binary"));
        context.AddInputParam(std::make_shared<StringValue>("srcSize"));
        context.AddInputParam(std::make_shared<StringValue>("srcHash"));
        context.AddInputParam(std::make_shared<StringValue>("destSize"));
        context.AddInputParam(std::make_shared<StringValue>("destHash"));
        context.AddInputParam(std::make_shared<StringValue>("/data/updater/patchfile"));
        auto instruction = std::make_unique<USInstrImagePatch>();
        EXPECT_EQ(instruction->Execute(env, context), USCRIPT_ERROR_EXECUTE);
        std::vector<UScriptValuePtr> output = context.GetOutVar();
        EXPECT_EQ(output.size(), 1);
    }

protected:
    static void SetUpTestCase()
    {
        LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
    }
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
private:
    TestPkgMgr pkgManager_ {};
};

HWTEST_F(ImgPatchUnittest, TestImgPatch01, TestSize.Level1)
{
    ImgPatchUnittest {}.TestImgPatch01();
}

HWTEST_F(ImgPatchUnittest, TestImgPatch02, TestSize.Level1)
{
    ImgPatchUnittest {}.TestImgPatch02();
}

HWTEST_F(ImgPatchUnittest, TestImgPatch03, TestSize.Level1)
{
    ImgPatchUnittest {}.TestImgPatch03();
}

HWTEST_F(ImgPatchUnittest, TestImgPatch04, TestSize.Level1)
{
    ImgPatchUnittest {}.TestImgPatch04();
}

HWTEST_F(ImgPatchUnittest, TestImgPatch05, TestSize.Level1)
{
    ImgPatchUnittest {}.TestImgPatch05();
}

HWTEST_F(ImgPatchUnittest, TestImgPatch06, TestSize.Level1)
{
    ImgPatchUnittest {}.TestImgPatch06();
}
}
