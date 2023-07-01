/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "pointers_input_device.h"
#include <unistd.h>
#include "log/log.h"
#include "common/input_device_manager.h"

namespace Updater {
void AddInputDevice()
{
    OHOS::InputDeviceManager::GetInstance()->Add(&PointersInputDevice::GetInstance());
}

void HandlePointersEvent(const input_event &ev, uint32_t type)
{
    PointersInputDevice::GetInstance().HandlePointEvent(ev, type);
}

PointersInputDevice &PointersInputDevice::GetInstance()
{
    static PointersInputDevice instance;
    return instance;
}

bool PointersInputDevice::Read(OHOS::DeviceData& data)
{
    ReadTouch(data);
    return false;
}

bool PointersInputDevice::ReadPoint(OHOS::DeviceData& data, OHOS::Point pos, bool fingerPressDown)
{
    data.point.x = pos.x;
    data.point.y = pos.y;
    data.state = fingerPressDown ? OHOS::InputDevice::STATE_PRESS : OHOS::InputDevice::STATE_RELEASE;
    return false;
}

bool PointersInputDevice::ReadTouch(OHOS::DeviceData &data)
{
    ReadPoint(data, reportAbsMt_, touchFingerDown_);
    return false;
}

void PointersInputDevice::SetFingerDown(bool isPressed)
{
    touchFingerDown_ = isPressed;
}

void PointersInputDevice::HandleEvAbsMt(const input_event &ev)
{
    const static std::unordered_map<int, std::function<void(int)>> evMap {
        {ABS_MT_POSITION_X, [this] (int value) {
            reportAbsMt_.x = value;
            SetFingerDown(true);
        }},
        {ABS_MT_POSITION_Y, [this] (int value) {
            reportAbsMt_.y = value;
            SetFingerDown(true);
        }},
        {ABS_MT_TRACKING_ID, [this] (int value) {
            // Protocol B: -1 marks lifting the contact.
            if (value < 0) {
                SetFingerDown(false);
            }
        }}
    };
    if (auto it = evMap.find(ev.code); it != evMap.end()) {
        it->second(ev.value);
    }
}

int PointersInputDevice::HandlePointEvent(const input_event &ev, uint32_t type)
{
    if (ev.type == EV_ABS) {
        HandleEvAbsMt(ev);
        return 0;
    }
    if (ev.type != EV_KEY || ev.code > KEY_MAX) {
        return 0;
    }
    if (ev.code == BTN_TOUCH) {
        SetFingerDown(ev.value == 1);
    }
    return 0;
}
} // namespace Updater
