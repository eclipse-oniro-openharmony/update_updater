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
#ifndef UPDATER_MAIN_H
#define UPDATER_MAIN_H

#include <iostream>
#include <string>
#include <vector>
#include "macros.h"
#include "updater/updater.h"

namespace Updater {
enum UpdaterInitEvent {
    // updater
    UPDATER_PRE_INIT_EVENT = 0,
    UPDATER_INIT_EVENT,
    UPDATER_POST_INIT_EVENT,

    // flashd
    FLAHSD_PRE_INIT_EVENT,

    // binary
    UPDATER_BINARY_INIT_EVENT,
    UPDATER_BINARY_INIT_DONE_EVENT,

    UPDATER_INIT_EVENT_BUTT
};

using InitHandler = void (*)(void);

class UpdaterInit {
    DISALLOW_COPY_MOVE(UpdaterInit);
public:
    static UpdaterInit &GetInstance()
    {
        static UpdaterInit instance;
        return instance;
    }
    void InvokeEvent(enum UpdaterInitEvent eventId) const
    {
        if (eventId >= UPDATER_INIT_EVENT_BUTT) {
            return;
        }
        for (const auto &handler : initEvent_[eventId]) {
            if (handler != nullptr) {
                handler();
            }
        }
    }
    void SubscribeEvent(enum UpdaterInitEvent eventId, InitHandler handler)
    {
        if (eventId < UPDATER_INIT_EVENT_BUTT) {
            initEvent_[eventId].push_back(handler);
        }
    }
private:
    UpdaterInit() = default;
    ~UpdaterInit() = default;
    std::vector<InitHandler> initEvent_[UPDATER_INIT_EVENT_BUTT];
};

enum FactoryResetMode {
    USER_WIPE_DATA = 0,
    FACTORY_WIPE_DATA,
};

int UpdaterMain(int argc, char **argv);

int FactoryReset(FactoryResetMode mode, const std::string &path);

UpdaterStatus UpdaterFromSdcard();

bool IsBatteryCapacitySufficient();
} // namespace Updater
#endif // UPDATER_MAIN_H
