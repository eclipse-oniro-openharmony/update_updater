/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef UPDATER_EVENT_H
#define UPDATER_EVENT_H

#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <vector>

#include "event_id.h"

namespace Updater {
class UpdaterEvent {
    using EventHandler = std::function<void()>;
public:
    static void Subscribe(enum EventId id, EventHandler handler)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        eventMap_[id].push_back(handler);
    }
    static void Invoke(enum EventId id)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        for (const auto &handler : eventMap_[id]) {
            if (handler != nullptr) {
                handler();
            }
        }
    }
private:
    static inline std::mutex mutex_ {};
    static inline std::map<enum EventId, std::vector<EventHandler>> eventMap_ {};
};
} // Updater
#endif // UPDATER_EVENT_H