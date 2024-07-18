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
#include "pointers_input_device.h"
#include <unistd.h>
#include "log/log.h"
#include "common/input_device_manager.h"
#include "ui_rotation.h"

using namespace testing::ext;
using namespace Updater;

namespace UpdaterUt {
class PointersInputDeviceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(PointersInputDeviceUnitTest, test_pointers_input_read, TestSize.Level0)
{
    OHOS::DeviceData data;
    EXPECT_FALSE(PointersInputDevice::GetInstance().Read(data));
}

HWTEST_F(PointersInputDeviceUnitTest, test_handle_point_event, TestSize.Level0)
{
    uint32_t type = 0;
    input_event ev = {.type = EV_ABS, .code = ABS_MT_POSITION_X};
    HandlePointersEvent(ev, type);
    ev.code = ABS_MT_POSITION_Y;
    EXPECT_EQ(PointersInputDevice::GetInstance().HandlePointEvent(ev, type), 0);
    ev.code = ABS_MT_TRACKING_ID;
    EXPECT_EQ(PointersInputDevice::GetInstance().HandlePointEvent(ev, type), 0);
    ev.code = BTN_TOUCH;
    ev.type = EV_KEY;
    EXPECT_EQ(PointersInputDevice::GetInstance().HandlePointEvent(ev, type), 0);
}
}