/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "input_event.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "common/input_device_manager.h"
#include "events/key_event.h"
#include "log/log.h"
#include "updater_ui.h"
#include "updater_ui_const.h"

namespace Updater {
namespace {
constexpr int MAX_INPUT_DEVICES = 32;

IInputInterface *g_inputInterface;
InputEventCb g_callback;

bool g_touchFingerDown = false;

// for TouchInputDevice
int g_touchX;
int g_touchY;

// for KeysInputDevice
uint16_t g_lastKeyId;
uint16_t g_keyState = OHOS::INVALID_KEY_STATE;

void HandleEvAbs(const input_event &ev)
{
    const static std::unordered_map<int, std::function<void(int)>> evMap {
        {ABS_MT_POSITION_Y, [] (int value) {
            g_touchY = value;
            g_touchFingerDown = true;
        }},
        {ABS_MT_POSITION_X, [] (int value) {
            g_touchX = value;
            g_touchFingerDown = true;
        }},
        {ABS_MT_TRACKING_ID, [] (int value) {
            // Protocol B: -1 marks lifting the contact.
            if (value < 0) {
                g_touchFingerDown = false;
            }
        }}
    };
    if (auto it = evMap.find(ev.code); it != evMap.end()) {
        it->second(ev.value);
    }
}
}

int HandleInputEvent(const struct input_event *iev)
{
    struct input_event ev {};
    ev.type = iev->type;
    ev.code = iev->code;
    ev.value = iev->value;
    if (ev.type == EV_ABS) {
        HandleEvAbs(ev);
        return 0;
    }
    if (ev.type != EV_KEY || ev.code > KEY_MAX) {
        return 0;
    }
    if (ev.code == BTN_TOUCH) {
        g_touchFingerDown = (ev.value == 1);
    }
    if (ev.code == BTN_TOUCH || ev.code == BTN_TOOL_FINGER) {
        return 0;
    }
    // KEY_VOLUMEDOWN = 114, KEY_VOLUMEUP = 115, KEY_POWER = 116
    if (!(ev.code == KEY_VOLUMEDOWN || ev.code == KEY_VOLUMEUP || ev.code == KEY_POWER)) {
        return 0;
    }
    g_keyState = (ev.value == 1) ? OHOS::InputDevice::STATE_PRESS : OHOS::InputDevice::STATE_RELEASE;
    g_lastKeyId = ev.code;
    return 0;
}

void ReportEventPkgCallback(const InputEventPackage **pkgs, const uint32_t count, uint32_t devIndex)
{
    if (pkgs == nullptr || *pkgs == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        struct input_event ev = {
            .type = static_cast<__u16>(pkgs[i]->type),
            .code = static_cast<__u16>(pkgs[i]->code),
            .value = pkgs[i]->value,
        };
        HandleInputEvent(&ev);
    }
    return;
}

int HdfInit()
{
    int ret = GetInputInterface(&g_inputInterface);
    if (ret != INPUT_SUCCESS) {
        LOG(ERROR) << "get input driver interface failed";
        return ret;
    }

    sleep(1); // need wait thread running

    InputDevDesc sta[MAX_INPUT_DEVICES] = {{0}};
    ret = g_inputInterface->iInputManager->ScanInputDevice(sta, MAX_INPUT_DEVICES);
    if (ret != INPUT_SUCCESS) {
        LOG(ERROR) << "scan device failed";
        return ret;
    }

    for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
        uint32_t idx = sta[i].devIndex;
        if ((idx == 0) || (g_inputInterface->iInputManager->OpenInputDevice(idx) == INPUT_FAILURE)) {
            continue;
        }

        LOG(INFO) << "hdf devType:" << sta[i].devType << ", devIndex:" << idx;
    }

    /* first param not necessary, pass default 1 */
    g_callback.EventPkgCallback = ReportEventPkgCallback;
    ret = g_inputInterface->iInputReporter->RegisterReportCallback(1, &g_callback);
    if (ret != INPUT_SUCCESS) {
        LOG(ERROR) << "register callback failed for device 1";
        return ret;
    }

    OHOS::InputDeviceManager::GetInstance()->Add(&TouchInputDevice::GetInstance());
    OHOS::InputDeviceManager::GetInstance()->Add(&KeysInputDevice::GetInstance());
    LOG(INFO) << "add InputDevice done";

    return 0;
}

TouchInputDevice &TouchInputDevice::GetInstance()
{
    static TouchInputDevice instance;
    return instance;
}

bool TouchInputDevice::Read(OHOS::DeviceData& data)
{
    data.point.x = g_touchX;
    data.point.y = g_touchY;
    data.state = g_touchFingerDown ? OHOS::InputDevice::STATE_PRESS : OHOS::InputDevice::STATE_RELEASE;
    return false;
}

KeysInputDevice &KeysInputDevice::GetInstance()
{
    static KeysInputDevice instance;
    return instance;
}

bool KeysInputDevice::Read(OHOS::DeviceData& data)
{
    data.keyId = g_lastKeyId;
    data.state = g_keyState;
    g_keyState =  OHOS::INVALID_KEY_STATE;
    return false;
}
} // namespace Updater
