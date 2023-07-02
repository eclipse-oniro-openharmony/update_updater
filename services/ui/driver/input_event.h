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
#ifndef UPDATER_UI_INPUT_EVENT_H
#define UPDATER_UI_INPUT_EVENT_H
#include <linux/input.h>
#include "common/input_device_manager.h"
#include "dock/pointer_input_device.h"
#include "input_manager.h"
#include "macros.h"
#include <unordered_set>
#include "pointers_input_device.h"

namespace Updater {
using AddInputDeviceFunc = std::function<void()>;
using HandlePointersEventFunc = std::function<void(const input_event &ev, uint32_t type)>;
class InputEvent {
    DISALLOW_COPY_MOVE(InputEvent);
public:
    void RegisterAddInputDeviceHelper(AddInputDeviceFunc ptr);
    void RegisterHandleEventHelper(HandlePointersEventFunc ptr);
    InputEvent() = default;
    virtual ~InputEvent() = default;
    static InputEvent &GetInstance();
    int HandleInputEvent(const struct input_event *iev, uint32_t type);
    void GetInputDeviceType(uint32_t devIndex, uint32_t &type);
    static void ReportEventPkgCallback(const InputEventPackage **pkgs, const uint32_t count, uint32_t devIndex);
    /**
     * @brief Init input device driver.
     */
    int HdfInit();

private:
    IInputInterface *inputInterface_;
    InputEventCb callback_;
    std::unordered_map<uint32_t, uint32_t> devTypeMap_{};
    AddInputDeviceFunc addInputDeviceHelper_;
    HandlePointersEventFunc handlePointersEventHelper_;
};
} // namespace Updater
#endif // UPDATER_UI_INPUT_EVENT_H