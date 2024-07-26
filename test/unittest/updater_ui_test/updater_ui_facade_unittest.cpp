/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "gtest/gtest.h"
#include <thread>
#include "component/text_label_adapter.h"
#include "updater_event.h"
#include "updater_ui_config.h"
#include "updater_ui_env.h"
#include "updater_ui_tools.h"

using namespace testing::ext;
using namespace Updater;

#define UPDATER_UI_INSTANCE UpdaterUiFacade::GetInstance()

namespace UpdaterUt {
class UpdaterUiFacadeUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(UpdaterUiFacadeUnitTest, test_updater_ui_facade_set_mode, Test.Level0)
{
    std::string mode = "abc";
    EXPECT_TRUE(UPDATER_UI_INSTANCE.SetMode(""));
    EXPECT_TRUE(UPDATER_UI_INSTANCE.SetMode(mode));
    EXPECT_EQ(UPDATER_UI_INSTANCE.GetMode(), mode);
}

HWTEST_F(UpdaterUiFacadeUnitTest, test_updater_ui_facade_show_funcs, Test.Level0)
{
    std::string log = "log";
    UPDATER_UI_INSTANCE.ShowLog(log);
    UPDATER_UI_INSTANCE.ShowLogRes(log);
    UPDATER_UI_INSTANCE.ShowUpdInfo(log);
    UPDATER_UI_INSTANCE.ClearText();
    UPDATER_UI_INSTANCE.ClearLog();
    UPDATER_UI_INSTANCE.ShowProgress(0.0);
    UPDATER_UI_INSTANCE.ShowProgressPage();
    UPDATER_UI_INSTANCE.ShowSuccessPage();
    UPDATER_UI_INSTANCE.ShowFailedPage();
    UPDATER_UI_INSTANCE.ShowFactoryConfirmPage();
    UPDATER_UI_INSTANCE.ShowMainpage();
    UPDATER_UI_INSTANCE.ShowProgressWarning(false);
    EXPECT_FALSE(UPDATER_UI_INSTANCE.IsInProgress());
    EXPECT_TRUE(UPDATER_UI_INSTANCE.SetMode("ota"));
    UPDATER_UI_INSTANCE.ShowLog(log);
    UPDATER_UI_INSTANCE.ShowLog(log, true);
    UPDATER_UI_INSTANCE.ShowLogRes(log);
    UPDATER_UI_INSTANCE.ShowUpdInfo(log);
    UPDATER_UI_INSTANCE.ClearText();
    UPDATER_UI_INSTANCE.ClearLog();
    UPDATER_UI_INSTANCE.ShowProgress(0.0);
    UPDATER_UI_INSTANCE.ShowProgressPage();
    UPDATER_UI_INSTANCE.ShowSuccessPage();
    UPDATER_UI_INSTANCE.ShowFailedPage();
    UPDATER_UI_INSTANCE.ShowFactoryConfirmPage();
    UPDATER_UI_INSTANCE.ShowMainpage();
    UPDATER_UI_INSTANCE.ShowProgressWarning(false);
    EXPECT_TRUE(UPDATER_UI_INSTANCE.IsInProgress());
}
}