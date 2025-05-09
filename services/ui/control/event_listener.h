/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef UPDATER_UI_EVENT_PRODUCT_H
#define UPDATER_UI_EVENT_PRODUCT_H

#include <functional>
#include <mutex>
#include "components/root_view.h"
#include "components/ui_view.h"

namespace Updater {
struct Callback {
    std::function<void(OHOS::UIView &)> func {nullptr};
    bool isAsync {false};
};
using KeyCallback = std::function<void(bool)>;
enum class EventType {
    TOUCHEVENT,
    CLICKEVENT,
    DRAGEVENT
};

// avoid callback on invisible component
class CallBackDecorator final {
public:
    explicit CallBackDecorator(Callback cb) : cb_(cb) {}
    ~CallBackDecorator() = default;
    void operator()(OHOS::UIView &view, bool isAsync) const;
private:
    static void CallbackWithGuard(Callback cb, OHOS::UIView &view);
    Callback cb_;
    static std::mutex mtx_;
};

class LabelOnTouchListener final : public OHOS::UIView::OnTouchListener {
public:
    LabelOnTouchListener(Callback cb, bool isConsumed)
        : cb_(cb), isConsumed_(isConsumed) {}
    ~LabelOnTouchListener() = default;
    bool OnPress(OHOS::UIView &view, const OHOS::PressEvent &event) override;
    bool OnCancel(OHOS::UIView &view, const OHOS::CancelEvent &event) override;
    bool OnRelease(OHOS::UIView &view, const OHOS::ReleaseEvent &event) override;

private:
    Callback cb_;
    bool isConsumed_;
};

class BtnOnEventListener final : public OHOS::UIView::OnClickListener, public OHOS::UIView::OnTouchListener {
public:
    BtnOnEventListener(Callback cb, bool isConsumed)
        : cb_(cb), isConsumed_(isConsumed) {}
    ~BtnOnEventListener() = default;
    bool OnClick(OHOS::UIView &view, const OHOS::ClickEvent &event) override;
    bool OnPress(OHOS::UIView &view, const OHOS::PressEvent &event) override;
    bool OnRelease(OHOS::UIView &view, const OHOS::ReleaseEvent &event) override;
    bool OnCancel(OHOS::UIView &view, const OHOS::CancelEvent &event) override;
private:
    Callback cb_;
    bool isConsumed_;
};

// note: only used for debug
class BtnOnDragListener final : public OHOS::UIView::OnDragListener {
public:
    BtnOnDragListener(Callback cb, bool isConsumed)
        : cb_(cb), isConsumed_(isConsumed) {
    }
    ~BtnOnDragListener() = default;
    bool OnDragStart(OHOS::UIView &view, const OHOS::DragEvent &event) override;
    bool OnDrag(OHOS::UIView &view, const OHOS::DragEvent &event) override;
    bool OnDragEnd(OHOS::UIView &view, const OHOS::DragEvent &event) override;

private:
    Callback cb_;
    bool isConsumed_;
};

class KeyListener : public OHOS::RootView::OnKeyActListener {
public:
    KeyListener() = default;
    virtual ~KeyListener() = default;
    bool OnKeyAct(OHOS::UIView &view, const OHOS::KeyEvent &event) override;
    static void SetButtonPressed(bool isPressed);
    bool ProcessPowerKey(OHOS::UIView &view, const OHOS::KeyEvent &event);
    bool ProcessVolumeKey(OHOS::UIView &view, const OHOS::KeyEvent &event);
private:
    OHOS::UIView *GetFirstFocusableViewByDir(uint8_t dir);
    static bool isButtonPressed_;
};
}
#endif
