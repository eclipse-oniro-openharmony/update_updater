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

#include <thread>
#include "gtest/gtest.h"
#include "ui_test_graphic_engine.h"
#include "view_api.h"
#include "graphic_engine.h"
#include "common/graphic_startup.h"
#include "common/image_decode_ability.h"
#include "common/screen.h"
#include "font/ui_font_header.h"
#include "log/log.h"
#include "updater_ui_const.h"
#include "ui_rotation.h"
#include "utils.h"

using namespace testing::ext;
using namespace Updater;

namespace {
constexpr uint32_t WHITE_BGCOLOR = 0x000000ff;
constexpr const char* FONT_PATH = "/system/etc/charger/resources/";
class UpdaterUiGraphicEngineUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        TestGraphicEngine::GetInstance();
    }
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(UpdaterUiGraphicEngineUnitTest, test_ui_graphic_engine_test01, TestSize.Level0)
{
    UiRotation::GetInstance().SetDegree(UI_ROTATION_DEGREE::UI_ROTATION_0);
    GraphicEngine::GetInstance().Init(WHITE_BGCOLOR, OHOS::ColorMode::ARGB8888, FONT_PATH);
    uint16_t height = OHOS::Screen::GetInstance().GetHeight();
    uint16_t width = OHOS::Screen::GetInstance().GetWidth();
    EXPECT_EQ(height, GraphicEngine::GetInstance().GetScreenHeight());
    EXPECT_EQ(width, GraphicEngine::GetInstance().GetScreenWidth());
    OHOS::BufferInfo *bufferInfo = GraphicEngine::GetInstance().GetFBBufferInfo();
    EXPECT_EQ(bufferInfo->color, WHITE_BGCOLOR);
    EXPECT_EQ(bufferInfo->mode, OHOS::ColorMode::ARGB8888);
    EXPECT_EQ(height, bufferInfo->height);
    EXPECT_EQ(width, bufferInfo->width);
}
}
