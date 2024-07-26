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
#include "updater_ui_env.h"
#include
#include
#include
#include
#include <unistd.h>
#include "callback_manager.h"
#include "common/graphic_startup.h"
#include "common/screen.h"
#include "components/root_view.h"
#include "graphic_engine.h"
#include "input_event.h"
#include "language/language_ui.h"
#include "log/log.h"
#include "page/page_manager.h"
#include "updater_ui_config.h"

using namespace testing::ext;
using namespace Updater;

namespace UpdaterUt {
class UpdaterUiEnvUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

constexpr std::pair MY_BRIGHTNESS_FILE =
    std::pair { "/data/updater/ui/brightness", "/data/updater/ui/max_brightness" };

HWTEST_F(UpdaterUiEnvUnitTest, test_updater_ui_env_bright_init, TestSize.Level0)
{
    EXPECT_FALSE(UpdaterUiEnv::InitBrightness("/fakebrightness", "/fakemaxbrightness"));
    EXPECT_FALSE(UpdaterUiEnv::InitBrightness(MY_BRIGHTNESS_FILE.first, "/fakemaxbrightness"));
    EXPECT_TRUE(UpdaterUiEnv::InitBrightness(MY_BRIGHTNESS_FILE.first, MY_BRIGHTNESS_FILE.second));
}
}