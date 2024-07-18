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
#include "ui_rotation.h"
#include "log/log.h"
#include "securec.h"

using namespace testing::ext;
using namespace Updater;

namespace UpdaterUt {
class UiRotationUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(UiRotationUnitTest, test_init_rotation, TestSize.Level0)
{
    int realWidth = 100;
    int realHeight = 500;
    uint8_t pixelBytes = 0;
    std::vector<UI_ROTATION_DEGREE> degrees = {
        UI_ROTATION_DEGREE::UI_ROTATION_0,
        UI_ROTATION_DEGREE::UI_ROTATION_90,
        UI_ROTATION_DEGREE::UI_ROTATION_180,
        UI_ROTATION_DEGREE::UI_ROTATION_270 };
    for (auto degree : degrees) {
        UiRotation::GetInstance().SetDegree(degree);
        UiRotation::GetInstance().InitRotation(realWidth, realHeight, pixelBytes);
        if (int(degree) % 2 == 1) {
            EXPECT_EQ(realWidth, UiRotation::GetInstance().GetHeight());
            EXPECT_EQ(realHeight, UiRotation::GetInstance().GetWidth());
        } else {
            EXPECT_EQ(realWidth, UiRotation::GetInstance().GetWidth());
            EXPECT_EQ(realHeight, UiRotation::GetInstance().GetHeight());
        }
    }
}
}