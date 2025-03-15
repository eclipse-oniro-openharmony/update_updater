/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "event_listener.h"

#include <linux/input.h>
#include <mutex>
#include <thread>
#include "dock/focus_manager.h"
#include "dock/input_device.h"
#include "page/page_manager.h"
#include "scope_guard.h"
#include "updater_ui_facade.h"

namespace Updater {
std::mutex CallBackDecorator::mtx_;
void CallBackDecorator::operator()(OHOS::UIView &view, bool isAsync) const
{
    auto *page = view.GetParent();
    if (page == nullptr) {
        LOG(ERROR) << "view hasn't a parent";
        return;
    }
    if (view.GetViewId() == nullptr) {
        LOG(ERROR) << "view is invalid, please check your json config";
        return;
    }
    std::string id = view.GetViewId();
    std::string pageId {};
    if (page->GetViewId() != nullptr) {
        pageId = page->GetViewId();
    }
    // page should be visible
    if (!page->IsVisible()) {
        LOG(ERROR) << pageId << " is not visible";
        return;
    }
    // component should be visible
    if (!view.IsVisible()) {
        LOG(ERROR) << id << " is not visible";
        return;
    }
    if (isAsync) {
        LOG(INFO) << "callback by async method";
        // then can trigger callback by async method
        std::thread t {
            [cb = cb_, &view] () {
                CallbackWithGuard(cb, view);
            }
        };
        t.detach();
    } else {
        LOG(INFO) << "callback by sync method";
        // then can trigger callback by sync method
        CallbackWithGuard(cb_, view);
    }
}

void CallBackDecorator::CallbackWithGuard(Callback cb, OHOS::UIView &view)
{
    std::unique_lock<std::mutex> lock(mtx_, std::defer_lock);
    if (!lock.try_lock()) {
        LOG(ERROR) << "try lock failed, only allow running one callback at the same time";
        return;
    }
    if (cb.func != nullptr) {
        cb.func(view);
    }
}

bool LabelOnEventListener::OnClick(OHOS::UIView &view, [[maybe_unused]] const OHOS::ClickEvent &event)
{
    CallBackDecorator{cb_}(view, cb_.isAsync);
    return isConsumed_;
}

bool LabelOnEventListener::OnPress(OHOS::UIView &view, [[maybe_unused]] const OHOS::PressEvent &event)
{
    KeyListener::SetButtonPressed(true);
    return true;
}
 
bool LabelOnEventListener::OnRelease(OHOS::UIView &view, [[maybe_unused]] const OHOS::ReleaseEvent &event)
{
    KeyListener::SetButtonPressed(false);
    CallBackDecorator{cb_}(view, cb_.isAsync);
    return isConsumed_;
}
 
bool LabelOnEventListener::OnCancel(OHOS::UIView &view, [[maybe_unused]] const OHOS::CancelEvent &event)
{
    KeyListener::SetButtonPressed(false);
    return true;
}

bool BtnOnEventListener::OnClick(OHOS::UIView &view, [[maybe_unused]] const OHOS::ClickEvent &event)
{
    CallBackDecorator{cb_}(view, cb_.isAsync);
    return isConsumed_;
}

bool BtnOnEventListener::OnPress(OHOS::UIView &view, [[maybe_unused]] const OHOS::PressEvent &event)
{
    KeyListener::SetButtonPressed(true);
    return true;
}

bool BtnOnEventListener::OnRelease(OHOS::UIView &view, [[maybe_unused]] const OHOS::ReleaseEvent &event)
{
    KeyListener::SetButtonPressed(false);
    return true;
}

bool BtnOnEventListener::OnCancel(OHOS::UIView &view, [[maybe_unused]] const OHOS::CancelEvent &event)
{
    KeyListener::SetButtonPressed(false);
    return true;
}

bool BtnOnDragListener::OnDragStart(OHOS::UIView &view, [[maybe_unused]] const OHOS::DragEvent &event)
{
    CallBackDecorator{cb_}(view, cb_.isAsync);
    return isConsumed_;
}

bool BtnOnDragListener::OnDrag(OHOS::UIView &view, const OHOS::DragEvent &event)
{
    CallBackDecorator{cb_}(view, cb_.isAsync);
    view.SetPosition(view.GetX() + event.GetDeltaX(), view.GetY() + event.GetDeltaY());
    if (view.GetParent() != nullptr) {
        view.GetParent()->Invalidate();
    }
    return isConsumed_;
}

bool BtnOnDragListener::OnDragEnd(OHOS::UIView &view, [[maybe_unused]] const OHOS::DragEvent &event)
{
    CallBackDecorator{cb_}(view, cb_.isAsync);
    return isConsumed_;
}

bool KeyListener::isButtonPressed_ {false};

bool KeyListener::OnKeyAct(OHOS::UIView &view, const OHOS::KeyEvent &event)
{
    bool consumed = false;
    switch (event.GetKeyId()) {
        case KEY_POWER:
            consumed = ProcessPowerKey(view, event);
            break;
        case KEY_VOLUMEUP:
        case KEY_VOLUMEDOWN:
            consumed = ProcessVolumeKey(view, event);
            break;
        default:
            LOG(ERROR) << "unsupported key id";
    }
    return consumed;
}

bool KeyListener::ProcessPowerKey(OHOS::UIView &view, const OHOS::KeyEvent &event)
{
#ifndef UPDATER_UT
    OHOS::UIView *pView = OHOS::FocusManager::GetInstance()->GetFocusedView();
    if (pView == nullptr) {
        LOG(ERROR) << "focused view is nullptr";
        return false;
    }
    // triggering button press event by key supports labelButton and label
    if (!((pView->GetViewType() == OHOS::UI_LABEL_BUTTON) || (pView->GetViewType() == OHOS::UI_LABEL))) {
        LOG(ERROR) << "focused view is not label button or label;
        return false;
    }
    int16_t centerX = pView->GetX() + static_cast<int16_t>(static_cast<uint16_t>(pView->GetWidth()) >> 1u);
    int16_t centerY = pView->GetY() + static_cast<int16_t>(static_cast<uint16_t>(pView->GetHeight()) >> 1u);
    if (event.GetState() == OHOS::InputDevice::STATE_PRESS) {
        LOG(DEBUG) << "OnPress";
        pView->OnClickEvent(OHOS::Point { centerX, centerY });
    }
#endif
    return true;
}

bool KeyListener::ProcessVolumeKey(OHOS::UIView &view, const OHOS::KeyEvent &event)
{
    const static std::unordered_map<uint16_t, uint8_t> dirMap {
        {KEY_VOLUMEUP, OHOS::FOCUS_DIRECTION_UP},
        {KEY_VOLUMEDOWN, OHOS::FOCUS_DIRECTION_DOWN},
        {KEY_UP, OHOS::FOCUS_DIRECTION_UP},
        {KEY_DOWN, OHOS::FOCUS_DIRECTION_DOWN},
    };
    if (isButtonPressed_) {
        return true;
    }
    if (auto it = dirMap.find(event.GetKeyId()); it != dirMap.end() &&
        event.GetState() == OHOS::InputDevice::STATE_RELEASE) {
        if (OHOS::FocusManager::GetInstance()->RequestFocusByDirection(it->second)) {
            return true;
        }
        LOG(WARNING) << "request focus failed";
        OHOS::UIView *candidate = GetFirstFocusableViewByDir(it->second);
        if (candidate != nullptr) {
            OHOS::FocusManager::GetInstance()->RequestFocus(candidate);
        }
    }
    return true;
}

OHOS::UIView *KeyListener::GetFirstFocusableViewByDir(uint8_t dir)
{
    OHOS::UIView *cur = OHOS::FocusManager::GetInstance()->GetFocusedView();
    if (cur == nullptr || (dir != OHOS::FOCUS_DIRECTION_UP && dir != OHOS::FOCUS_DIRECTION_DOWN)) {
        return cur;
    }
    OHOS::UIView *parent = cur->GetParent();
    if (parent == nullptr || !parent->IsViewGroup()) {
        return cur;
    }
    OHOS::UIView *candidate = static_cast<OHOS::UIViewGroup *>(parent)->GetChildrenHead();
    OHOS::UIView *topFocusableView = cur;
    OHOS::UIView *bottomFocusableView = cur;
    while (candidate != nullptr) {
        if (candidate->IsFocusable() && candidate->IsVisible()) {
            if (candidate->GetY() < topFocusableView->GetY()) {
                topFocusableView = candidate;
            } else if (candidate->GetY() > bottomFocusableView->GetY()) {
                bottomFocusableView = candidate;
            }
        }
        candidate = candidate->GetNextSibling();
    }
    return dir == OHOS::FOCUS_DIRECTION_UP ? bottomFocusableView : topFocusableView;
}

void KeyListener::SetButtonPressed(bool isPressed)
{
    isButtonPressed_ = isPressed;
}
}
