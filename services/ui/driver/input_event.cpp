/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include <unistd.h>
#include "log/log.h"
#include "keys_input_device.h"

namespace Updater {
constexpr const int MAX_INPUT_DEVICES = 32;
extern "C" __attribute__((constructor)) void RegisterAddInputDeviceHelper(void)
{
    InputEvent::GetInstance().RegisterAddInputDeviceHelper(AddInputDevice);
}

void InputEvent::RegisterAddInputDeviceHelper(AddInputDeviceFunc ptr)
{
    addInputDeviceHelper_ = std::move(ptr);
}

extern "C" __attribute__((constructor)) void RegisterHandleEventHelper(void)
{
    InputEvent::GetInstance().RegisterHandleEventHelper(HandlePointersEvent);
}

void InputEvent::RegisterHandleEventHelper(HandlePointersEventFunc ptr)
{
    handlePointersEventHelper_ = std::move(ptr);
}

InputEvent &InputEvent::GetInstance()
{
    static InputEvent instance;
    return instance;
}

int InputEvent::HandleInputEvent(const struct input_event *iev, uint32_t type)
{
    struct input_event ev {};
    ev.type = iev->type;
    ev.code = iev->code;
    ev.value = iev->value;

    KeysInputDevice::GetInstance().HandleKeyEvent(ev, type);
    handlePointersEventHelper_(ev, type);
    return 0;
}

void InputEvent::GetInputDeviceType(uint32_t devIndex, uint32_t &type)
{
    auto it = devTypeMap_.find(devIndex);
    if (it == devTypeMap_.end()) {
        LOG(ERROR) << "devTypeMap_ devIndex: " << devIndex << "not valid";
        return;
    }
    type = it->second;
}

void InputEvent::ReportEventPkgCallback(const InputEventPackage **pkgs, const uint32_t count, uint32_t devIndex)
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
        uint32_t type = 0;
        InputEvent::GetInstance().GetInputDeviceType(devIndex, type);
        InputEvent::GetInstance().HandleInputEvent(&ev, type);
    }
    return;
}

int InputEvent::HdfInit()
{
    int ret = GetInputInterface(&inputInterface_);
    if (ret != INPUT_SUCCESS) {
        LOG(ERROR) << "get input driver interface failed";
        return ret;
    }

    sleep(1); // need wait thread running

    InputDevDesc sta[MAX_INPUT_DEVICES] = {{0}};
    ret = inputInterface_->iInputManager->ScanInputDevice(sta, MAX_INPUT_DEVICES);
    if (ret != INPUT_SUCCESS) {
        LOG(ERROR) << "scan device failed";
        return ret;
    }

    for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
        uint32_t idx = sta[i].devIndex;
        uint32_t dev = sta[i].devType;
        if ((idx == 0) || (inputInterface_->iInputManager->OpenInputDevice(idx) == INPUT_FAILURE)) {
            continue;
        }
        devTypeMap_.insert(std::pair<uint32_t, uint32_t>(idx, dev));

        LOG(INFO) << "hdf devType:" << dev << ", devIndex:" << idx;
    }

    /* first param not necessary, pass default 1 */
    callback_.EventPkgCallback = ReportEventPkgCallback;
    ret = inputInterface_->iInputReporter->RegisterReportCallback(1, &callback_);
    if (ret != INPUT_SUCCESS) {
        LOG(ERROR) << "register callback failed for device 1";
        return ret;
    }

    OHOS::InputDeviceManager::GetInstance()->Add(&KeysInputDevice::GetInstance());
    OHOS::InputDeviceManager::GetInstance()->SetPeriod(0);
    addInputDeviceHelper_();
    LOG(INFO) << "add InputDevice done";

    return 0;
}
} // namespace Updater
