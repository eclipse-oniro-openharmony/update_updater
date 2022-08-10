/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "event_manager.h"

#include "log/log.h"

namespace Updater {
EventManager::EventManager() : pgMgr_(PageManager::GetInstance())
{
}

EventManager &EventManager::GetInstance()
{
    static EventManager instance;
    return instance;
}

void EventManager::Add(const ComInfo &viewId, std::unique_ptr<LabelOnTouchListener> listener)
{
    if (!pgMgr_.IsValidCom(viewId)) {
        LOG(ERROR) << "not an valid view " << viewId;
        return;
    }
    auto com = pgMgr_[viewId.pageId][viewId.comId].As();
    labelOnTouchListener_.push_back(std::move(listener));
    com->SetTouchable(true);
    com->SetOnTouchListener(labelOnTouchListener_.back().get());
}

void EventManager::Add(const ComInfo &viewId, std::unique_ptr<BtnOnEventListener> listener)
{
    if (!pgMgr_.IsValidCom(viewId)) {
        LOG(ERROR) << "not an valid view " << viewId;
        return;
    }
    auto com = pgMgr_[viewId.pageId][viewId.comId].As();
    // both click and touch listener
    BtnOnEventListener *btnListener = listener.get();
    // save this listener
    btnOnClickListener_.push_back(std::move(listener));
    com->SetOnClickListener(btnListener);
    // this touch listener used to disable volume key when pressing button
    com->SetOnTouchListener(btnListener);
}

void EventManager::Add(const ComInfo &viewId, std::unique_ptr<BtnOnDragListener> listener)
{
    if (!pgMgr_.IsValidCom(viewId)) {
        LOG(ERROR) << "not an valid view " << viewId;
        return;
    }
    auto com = pgMgr_[viewId.pageId][viewId.comId].As();
    btnOnDragListener.push_back(std::move(listener));
    com->SetDraggable(true);
    com->SetOnDragListener(btnOnDragListener.back().get());
}

// key listener is registered at root view, because key event don't has position info and is globally responded
void EventManager::AddKeyListener()
{
    keyListener_ = std::make_unique<KeyListener>();
    OHOS::RootView::GetInstance()->SetOnKeyActListener(keyListener_.get());
}

void EventManager::Add(const ComInfo &viewId, EventType evt, Callback cb)
{
    switch (evt) {
        case EventType::CLICKEVENT:
            Add(viewId, std::make_unique<BtnOnEventListener>(cb, true));
            break;
        case EventType::TOUCHEVENT:
            Add(viewId, std::make_unique<LabelOnTouchListener>(cb, true));
            break;
        case EventType::DRAGEVENT:
            Add(viewId, std::make_unique<BtnOnDragListener>(cb, true));
            break;
        default:
            LOG(WARNING) << "event type not support";
            break;
    }
}
} // namespace Updater
