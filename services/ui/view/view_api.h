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

#ifndef UPDATER_UX_VIEW_API_H
#define UPDATER_UX_VIEW_API_H

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "common/text.h"
#include "updater_ui_traits.h"

namespace Updater {
OHOS::UITextLanguageAlignment GetAlign(const std::string &align);
std::string TranslateText(const std::string &id);
bool CheckColor(const std::string &color);
OHOS::ColorType StrToColor(const std::string &hexColor);
}  // namespace Updater
#endif
