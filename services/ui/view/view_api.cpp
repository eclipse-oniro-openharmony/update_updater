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
#include "utils.h"

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

bool CheckColor(const std::string &color)
{
    constexpr char colorBegin = '#';
    constexpr std::size_t len = 9ul; // #rrggbbaa
    if (color.empty() || color[0] != colorBegin || color.size() != len) {
        LOG(ERROR) << "color format error: " << color;
        return false;
    }
    if (std::find_if_not(std::next(begin(color)), end(color), [](unsigned char c) { return isxdigit(c) != 0; }) !=
        end(color)) {
        LOG(ERROR) << "color format error: " << color;
        return false;
    }
    return true;
}

/*
 * should call CheckColor before StrToColor. when directly calling StrToColor,
 * invalid color will be regarded as black color;
 */
OHOS::ColorType StrToColor(const std::string &hexColor)
{
    std::size_t startPos = 1ul;
    auto getNextField = [&startPos, &hexColor] () {
        constexpr std::size_t width = 2ul;
        uint8_t ret = (startPos > hexColor.size()) ? 0 : Utils::String2Int<uint8_t>(hexColor.substr(startPos, width));
        startPos += width;
        return ret;
    };
    auto r = getNextField();
    auto g = getNextField();
    auto b = getNextField();
    auto a = getNextField();
    return OHOS::Color::GetColorFromRGBA(r, g, b, a);
}
}