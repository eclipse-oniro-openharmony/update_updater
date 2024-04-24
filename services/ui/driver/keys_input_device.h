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
#ifndef UPDATER_UI_KEYS_INPUT_DEVICE_H
#define UPDATER_UI_KEYS_INPUT_DEVICE_H

#include <atomic>
#include <linux/input.h>
#include <string>
#include "input_manager.h"
#include "macros.h"
#include "dock/key_input_device.h"
#include "events/key_event.h"

namespace Updater {
static constexpr auto LONG_PRESS_POWER_ONLY_TYPE = "power";
class KeysInputDevice : public OHOS::KeyInputDevice {
    DISALLOW_COPY_MOVE(KeysInputDevice);
public:
    KeysInputDevice() = default;
    virtual ~KeysInputDevice() = default;
    static KeysInputDevice &GetInstance();
    bool Read(OHOS::DeviceData &data) override;
    int HandleKeyEvent(const input_event &ev, uint32_t type);
    void SetLongPressType(const std::string &type);
private:
    void PowerVolumeDownPress(const input_event &ev);
    void OnLongKeyPressUp();
    void OnLongKeyPressDown();
    /* for KeysInputDevice */
    uint16_t lastKeyId_ { 0 };
    uint16_t keyState_ = OHOS::INVALID_KEY_STATE;
    std::atomic<bool> timerStop_ {false};
    std::string type_;
};
} // namespace Updater
#endif // UPDATER_UI_KEYS_INPUT_DEVICE_H
