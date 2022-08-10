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

#include "callback_manager.h"

#include <algorithm>
#include "event_listener.h"
#include "event_manager.h"
#include "page/page_manager.h"
#include "updater_ui.h"

namespace Updater {
constexpr auto CB_FIELD = "callbacks";

std::vector<CallbackCfg> CallbackManager::callbackCfgs_;

std::unordered_map<std::string, EventType> CallbackManager::evtTypes_ = {
    {"TOUCHEVENT", EventType::TOUCHEVENT},
    {"CLICKEVENT", EventType::CLICKEVENT}
};

std::unordered_map<std::string, Callback> CallbackManager::funcs_ = {
    {"OnLabelCancelEvt", &OnLabelCancelEvt},
    {"OnLabelOkEvt", &OnLabelOkEvt},
    {"OnRebootEvt", &OnRebootEvt},
    {"OnLabelResetEvt", &OnLabelResetEvt},
    {"OnMenuShutdownEvt", &OnMenuShutdownEvt},
    {"OnLabelSDCardEvt", &OnLabelSDCardEvt},
    {"OnLabelSDCardNoDelayEvt", &OnLabelSDCardNoDelayEvt},
    {"OnConfirmRstEvt", &OnConfirmRstEvt},
    {"OnMenuClearCacheEvt", &OnMenuClearCacheEvt}
};

bool CallbackManager::LoadCallbacks(const JsonNode &node)
{
    const JsonNode &cbNodes = node[CB_FIELD];
    if (cbNodes.Type() != NodeType::ARRAY) {
        LOG(ERROR) << "callback config node type should be array";
        return false;
    }
    CallbackCfg cbCfg;
    for (const auto &cbNode : cbNodes) {
        cbCfg = {};
        if (!Visit<SETVAL>(cbNode.get(), cbCfg)) {
            LOG(ERROR) << "callback config parse failed";
            return false;
        }
        callbackCfgs_.push_back(std::move(cbCfg));
    }
    return true;
}

void CallbackManager::Register(const CallbackCfg &cbCfg)
{
    auto it = evtTypes_.find(cbCfg.type);
    if (it == evtTypes_.end()) {
        LOG(ERROR) << "not recognized event type: " << cbCfg.type;
        return;
    }
    auto itFunc = funcs_.find(cbCfg.func);
    if (itFunc == funcs_.end()) {
        LOG(ERROR) << "not recognized event type: " << cbCfg.func;
        return;
    }
    EventManager::GetInstance().Add(ComInfo {cbCfg.pageId, cbCfg.comId}, it->second, itFunc->second);
    LOG(DEBUG) << "register " << cbCfg.func << " for " << ComInfo {cbCfg.pageId, cbCfg.comId} << " succeed";
}

void CallbackManager::Init(bool hasFocus)
{
    [[maybe_unused]] static bool isInited = [hasFocus] () {
        for (auto &cb : callbackCfgs_) {
            Register(cb);
        }

        if (hasFocus) {
            // for focus manager
            EventManager::GetInstance().AddKeyListener();
        }

        // for long press warning
        LOG(DEBUG) << "register key action listerner succeed";
        return true;
    } ();
}
}