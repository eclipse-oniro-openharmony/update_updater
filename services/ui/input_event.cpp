/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "log/log.h"
#include "updater_ui_const.h"

namespace updater {
constexpr int TOUCH_LOW_TH = 50;
constexpr int TOUCH_HIGH_TH = 90;
constexpr int INIT_DEFAULT_VALUE = 255;
constexpr int LABEL_HEIGHT = 80;
constexpr int LABEL_OFFSET_2 = 2;
constexpr int LABEL_OFFSET_3 = 3;
constexpr int DIALOG_START_Y = 550;
IInputInterface *g_inputInterface;
InputEventCb g_callback;

bool g_touchSwiping = false;
bool g_touchFingerDown = false;

int g_touchX;
int g_touchY;
int g_touchStartX;
int g_touchStartY;
extern Frame *g_menuFrame;

enum SwipeDirection {
    UP,
    DOWN,
    RIGHT,
    LEFT
};

void TouchToClickEvent(const int dx, const int dy, int event)
{
    if (!g_menuFrame->IsVisiable()) {
        LOG(INFO) << "menu page is not top";
        return;
    }
    if (abs(dy) >= 0 && abs(dy) <= LABEL_HEIGHT) {
        g_menuFrame->DispatchKeyEvent(LABEL_ID_0, event);
    } else if (abs(dy) > LABEL_HEIGHT && abs(dy) <= LABEL_HEIGHT * LABEL_OFFSET_2) {
        g_menuFrame->DispatchKeyEvent(LABEL_ID_1, event);
    } else if (abs(dy) > LABEL_HEIGHT * LABEL_OFFSET_2 && abs(dy) <= LABEL_HEIGHT * LABEL_OFFSET_3) {
        g_menuFrame->DispatchKeyEvent(LABEL_ID_2, event);
    } else if (abs(dy) > WIDTH1 && abs(dy) <= DIALOG_START_Y) {
        if (abs(dx) > 0 && abs(dx) <= DIALOG_OK_WIDTH) {
            g_menuFrame->DispatchKeyEvent(DIALOG_OK_ID, event);
        } else if (abs(dx) > DIALOG_CANCEL_X && abs(dx) <= WIDTH1) {
            g_menuFrame->DispatchKeyEvent(DIALOG_CANCEL_ID, event);
        }
    }
    return;
}

void TouchToKey(const int dx, const int dy)
{
    enum SwipeDirection direction;
    if (abs(dy) < TOUCH_LOW_TH && abs(dx) > TOUCH_HIGH_TH) {
        direction = (dx < 0) ? SwipeDirection::LEFT : SwipeDirection::RIGHT;
    } else if (abs(dx) < TOUCH_LOW_TH && abs(dy) > TOUCH_HIGH_TH) {
        direction = (dy < 0) ? SwipeDirection::UP : SwipeDirection::DOWN;
    } else {
        return;
    }
    switch (direction) {
        case SwipeDirection::UP:
            g_menuFrame->DispatchKeyEvent(KEY_UP);
            break;
        case SwipeDirection::DOWN:
            g_menuFrame->DispatchKeyEvent(KEY_DOWN);
            break;
        case SwipeDirection::LEFT:
        case SwipeDirection::RIGHT:
            g_menuFrame->DispatchKeyEvent(KEY_POWER);
            break;
        default:
            break;
    }
    return;
}

void SwipEvent()
{
    if (g_touchFingerDown && !g_touchSwiping) {
        g_touchStartX = g_touchX;
        g_touchStartY = g_touchY;
        g_touchSwiping = true;
    } else if (!g_touchFingerDown && g_touchSwiping) {
        g_touchSwiping = false;
        TouchToKey(g_touchX - g_touchStartX, g_touchY - g_touchStartY);
    }
}

void ClickEvent()
{
    if (!g_menuFrame->IsVisiable()) {
        LOG(INFO) << "click event";
        TouchToClickEvent(g_touchX, g_touchY, -1);
        return;
    }
    if (g_touchFingerDown) {
        TouchToClickEvent(g_touchX, g_touchY, PRESS_EVENT);
    } else if (!g_touchFingerDown) {
        TouchToClickEvent(g_touchX, g_touchY, RELEASE_EVENT);
    }
}

int HandleInputEvent(const struct input_event *iev)
{
    struct input_event ev {};
    ev.type = iev->type;
    ev.code = iev->code;
    ev.value = iev->value;
    if (ev.type == EV_SYN) {
        if (ev.code == SYN_REPORT) {
            // There might be multiple SYN_REPORT events. We should only detect
            // a swipe after lifting the contact.
#ifdef CONVERT_RL_SLIDE_TO_CLICK
            SwipEvent();
#else
            ClickEvent();
#endif
        }
        return 0;
    }
    if (ev.type == EV_ABS) {
        switch (ev.code) {
            case ABS_MT_POSITION_X:
                g_touchX = ev.value;
                g_touchFingerDown = true;
                break;
            case ABS_MT_POSITION_Y:
                g_touchY = ev.value;
                g_touchFingerDown = true;
                break;
            case ABS_MT_TRACKING_ID:
                // Protocol B: -1 marks lifting the contact.
                if (ev.value < 0) {
                    g_touchFingerDown = false;
                }
                break;
            default:
                break;
        }
        return 0;
    }
    if (ev.type == EV_KEY && ev.code <= KEY_MAX) {
        if (ev.code == BTN_TOUCH) {
            g_touchFingerDown = (ev.value == 1);
        }
        if (ev.code == BTN_TOUCH || ev.code == BTN_TOOL_FINGER) {
            return 0;
        }
    }
    return 0;
}

void ReportEventPkgCallback(const EventPackage **pkgs, const uint32_t count, uint32_t devIndex)
{
    if (pkgs == nullptr || *pkgs == nullptr || !g_menuFrame->IsVisiable()) {
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
    ret = g_inputInterface->iInputManager->OpenInputDevice(1);
    if (ret) {
        LOG(ERROR) << "open device1 failed";
        return ret;
    }
    uint32_t devType = 0;
    ret = g_inputInterface->iInputController->GetDeviceType(1, &devType);
    if (ret) {
        LOG(ERROR) << "get device1's type failed";
        return ret;
    }
    g_callback.EventPkgCallback = ReportEventPkgCallback;
    ret  = g_inputInterface->iInputReporter->RegisterReportCallback(1, &g_callback);
    if (ret) {
        LOG(ERROR) << "register callback failed for device 1";
        return ret;
    }
    devType = INIT_DEFAULT_VALUE;
    ret = g_inputInterface->iInputController->GetDeviceType(1, &devType);
    return 0;
}
} // namespace updater
