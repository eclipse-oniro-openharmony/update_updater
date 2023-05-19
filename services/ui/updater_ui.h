/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef UPDATE_UI_UPDATER_UI_H
#define UPDATE_UI_UPDATER_UI_H

#include <ostream>
#include <string>
#include <string_view>
#include "macros.h"
#include "updater_ui_const.h"
#include "updater_ui_traits.h"

namespace Updater {
std::ostream &operator<<(std::ostream &os, const ComInfo &com);
void DoProgress();
void StartLongPressTimer();
void StopLongPressTimer();
} // namespace Updater
#endif /* UPDATE_UI_HOS_UPDATER_H */
