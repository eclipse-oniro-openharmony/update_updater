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

#include "view_api.h"
#include "language/language_ui.h"

namespace Updater {
using namespace OHOS;
UITextLanguageAlignment GetAlign(const std::string &align)
{
    static const std::unordered_map<std::string, OHOS::UITextLanguageAlignment> alignMap {
        {"left", TEXT_ALIGNMENT_LEFT},
        {"right", TEXT_ALIGNMENT_RIGHT},
        {"center", TEXT_ALIGNMENT_CENTER},
        {"top", TEXT_ALIGNMENT_TOP},
        {"bottom", TEXT_ALIGNMENT_BOTTOM}
    };
    if (auto it = alignMap.find(align); it != alignMap.end()) {
        return it->second;
    }
    LOG(ERROR) << "not recognized align, must be one of left,right,center,top,bottom, use center as default align";
    return TEXT_ALIGNMENT_CENTER;
}

std::string TranslateText(const std::string &id)
{
    constexpr std::string_view emptyContent = "[]";
    constexpr size_t idStartPos = 1;
    if (id.size() > emptyContent.size() && *id.begin() == '[' && *id.rbegin() == ']') {
        // format is [tag], then find by tag
        return Lang::LanguageUI::GetInstance().Translate(id.substr(idStartPos, id.size() - emptyContent.size()));
    }
    // format is not [tag], then directly return id
    return id;
}

uint32_t GetColor(const UxBRGAPixel &color)
{
    return OHOS::Color::GetColorFromRGB(color.r, color.g, color.b).full;
}
}