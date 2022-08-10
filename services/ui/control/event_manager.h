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

#ifndef UPDATER_UI_EVENT_MANAGER_H
#define UPDATER_UI_EVENT_MANAGER_H

#include <memory>
#include <vector>

#include "component/component_factory.h"
#include "event_listener.h"
#include "macros.h"
#include "page/page_manager.h"

namespace Updater {
class EventManager final {
    DISALLOW_COPY_MOVE(EventManager);
public:
    EventManager();
    ~EventManager() = default;
    static EventManager &GetInstance();
    void Add(const ComInfo &viewId, std::unique_ptr<LabelOnTouchListener> listener);
    void Add(const ComInfo &viewId, std::unique_ptr<BtnOnEventListener> listener);
    void Add(const ComInfo &viewId, std::unique_ptr<BtnOnDragListener> listener);
    void Add(const ComInfo &viewId, EventType evt, Callback cb);
    void AddKeyListener();
private:
    PageManager &pgMgr_;
    std::unique_ptr<KeyListener> keyListener_;
    std::vector<std::unique_ptr<OHOS::UIView::OnTouchListener>> labelOnTouchListener_;
    std::vector<std::unique_ptr<OHOS::UIView::OnClickListener>> btnOnClickListener_;
    std::vector<std::unique_ptr<OHOS::UIView::OnDragListener>> btnOnDragListener;
};
} // namespace Updater
#endif
