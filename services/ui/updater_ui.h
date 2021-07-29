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

#include <string>
#include "text_label.h"

namespace updater {
void DoProgress();

void ShowUpdateFrame(bool isShow);

void UpdaterUiInit();

void ShowText(TextLabel *label, std::string text);

void DeleteView();
} // namespace updater
#endif /* UPDATE_UI_HOS_UPDATER_H */
