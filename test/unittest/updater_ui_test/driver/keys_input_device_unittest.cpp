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
#include "keys_input_device.h"

#include <thread>
#include "updater_event.h"

using namespace testing::ext;
using namespace Updater;

namespace UpdaterUt {
enum KeyUpDownEvent {
    EVENT_KEY_UP_VALUE,
    EVENT_KEY_DOWN_VALUE
};

class KeysInputDeviceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(KeysInputDeviceUnitTest, test_keys_input_read, TestSize.Level0)
{
    OHOS::DeviceData data;
    EXPECT_FALSE(KeysInputDevice::GetInstance().Read(data));
}

HWTEST_F(KeysInputDeviceUnitTest, test_handle_key_event, TestSize.Level0)
{
    input_event ev = {.type = EV_KEY + 1};
    uint32_t type = 0;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.type = EV_KEY;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = KEY_MAX +1;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.value = EVENT_KEY_UP_VALUE;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = BTN_TOUCH;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = BTN_TOOL_FINGER;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = BTN_MOUSE;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = BTN_LEFT;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = BTN_MIDDLE;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = BTN_RIGHT;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = KEY_VOLUMEDOWN;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = KEY_VOLUMEUP;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = KEY_POWER;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.value = EVENT_KEY_DOWN_VALUE;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    ev.code = KEY_HOMEPAGE;
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
    KeysInputDevice::GetInstance().SetLongPressType(LONG_PRESS_POWER_ONLY_TYPE);
    EXPECT_EQ(KeysInputDevice::GetInstance().HandleKeyEvent(ev, type), 0);
}
}