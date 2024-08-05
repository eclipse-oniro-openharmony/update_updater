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
#include "event_listener.h"
#include <linux/input.h>
#include "dock/input_device.h"

using namespace testing::ext;
using namespace Updater;

namespace {
class UpdaterUiControlUnittest : public testing::Test {
public:
    KeyListener *keyListener;
    OHOS::UIView *view;

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override
    {
        keyListener = new KeyListener();
        view = new OHOS::UIView();
    }
    void TearDown() override
    {
        delete keyListener;
        keyListener = nullptr;
        delete view;
        view = nullptr;
    }
};

HWTEST_F(UpdaterUiControlUnittest, OnKeyAct01, TestSize.Level0)
{
    OHOS::KeyEvent *event = new OHOS::KeyEvent(KEY_POWER, OHOS::InputDevice::STATE_PRESS);
    bool ret = keyListener->OnKeyAct(*view, *event);
    EXPECT_EQ(ret, true);
    if (event != nullptr) {
        delete event;
        event = nullptr;
    }
}

HWTEST_F(UpdaterUiControlUnittest, OnKeyAct02, TestSize.Level0)
{
    OHOS::KeyEvent *event = new OHOS::KeyEvent(KEY_VOLUMEUP, OHOS::InputDevice::STATE_PRESS);
    bool ret = keyListener->OnKeyAct(*view, *event);
    EXPECT_EQ(ret, true);
    if (event != nullptr) {
        delete event;
        event = nullptr;
    }
}

HWTEST_F(UpdaterUiControlUnittest, OnKeyAct03, TestSize.Level0)
{
    OHOS::KeyEvent *event = new OHOS::KeyEvent(KEY_VOLUMEDOWN, OHOS::InputDevice::STATE_PRESS);
    bool ret = keyListener->OnKeyAct(*view, *event);
    EXPECT_EQ(ret, true);
    if (event != nullptr) {
        delete event;
        event = nullptr;
    }
}

HWTEST_F(UpdaterUiControlUnittest, OnKeyAct04, TestSize.Level0)
{
    OHOS::KeyEvent *event = new OHOS::KeyEvent(100, OHOS::InputDevice::STATE_PRESS);
    bool ret = keyListener->OnKeyAct(*view, *event);
    EXPECT_EQ(ret, true);
    if (event != nullptr) {
        delete event;
        event = nullptr;
    }
}
}