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
#ifndef UPDATER_UI_POINTERS_INPUT_DEVICE_H
#define UPDATER_UI_POINTERS_INPUT_DEVICE_H
#include <linux/input.h>
#include "macros_updater.h"
#include "dock/pointer_input_device.h"
#include "dock/input_device.h"

namespace Updater {
void AddInputDevice();
void HandlePointersEvent(const input_event &ev, uint32_t type);
class PointersInputDevice : public OHOS::PointerInputDevice {
    DISALLOW_COPY_MOVE(PointersInputDevice);
public:
    PointersInputDevice() = default;
    virtual ~PointersInputDevice() = default;
    static PointersInputDevice &GetInstance();
    bool Read(OHOS::DeviceData &data) override;
    bool ReadPoint(OHOS::DeviceData& data, OHOS::Point pos, bool fingerPressDown);
    bool ReadTouch(OHOS::DeviceData &data);
    void SetFingerDown(bool isPressed);
    int HandlePointEvent(const input_event &ev, uint32_t type);
    void HandleEvAbsMt(const input_event &ev);

private:
    OHOS::Point reportAbsMt_ {};
    bool touchFingerDown_ { false };
};
} // namespace Updater
#endif // UPDATER_UI_POINTERS_INPUT_DEVICE_H