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
#ifndef UPDATER_EVENT_ID_H
#define UPDATER_EVENT_ID_H

namespace Updater {
enum EventId {
    UPDATER_EVENT_START = 0,

    // key up down
    UPDATER_POWER_VOLUME_UP_EVENT,
    UPDATER_POWER_VOLUME_DOWN_EVENT,

    UPDATER_EVENT_END
};
} // Updater
#endif // UPDATER_EVENT_ID_H
