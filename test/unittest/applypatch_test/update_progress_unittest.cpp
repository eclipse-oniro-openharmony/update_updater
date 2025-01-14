/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <pthread.h>
#include <thread>
#include <string>
#include "script_instruction.h"
#include "script/script_unittest.h"
#include "applypatch/update_progress.h"
#include "script_instruction/script_instruction_unittest.h"

using namespace testing::ext;
using namespace Updater;
using namespace Uscript;


namespace OHOS {
class UpdateProgressTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

class UTestPostProgressEnv : public UTestScriptEnv {
public:
    explicit UTestPostProgressEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager) : UTestScriptEnv(pkgManager)
    {}
    ~UTestPostProgressEnv() = default;

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

HWTEST_F(UpdateProgressTest, UpdateProgressTest01, TestSize.Level1)
{
    Hpackage::TestScriptPkgManager pkgManager;
    UTestPostProgressEnv env {&pkgManager};
    pthread_t thread;
    EXPECT_EQ(CreateProgressThread(&env, thread), 0);
    float progress = 1.0f;
    SetUpdateProgress(progress);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::stringstream ss;
    ss << "show_progress " << std::to_string(progress) << "," << std::to_string(0.0f);
    EXPECT_EQ(env.GetPostMessage(), ss.str());
    SetProgressExitFlag(thread);
}
}