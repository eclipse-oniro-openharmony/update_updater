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
#ifndef UPDATER_UI_TEXT_LABEL_ADAPTER_H
#define UPDATER_UI_TEXT_LABEL_ADAPTER_H

#include "component_common.h"
#include "components/ui_label.h"
#include "json_visitor.h"
#include "macros_updater.h"

namespace Updater {
DEFINE_STRUCT_TRAIT(LabelFocusInfo, "",
    (std::string, focusedFontColor),
    (std::string, focusedBgColor),
    (bool, focusable)
);

struct UxLabelInfo {
    uint8_t fontSize;
    std::string text;
    std::string align;
    std::string fontColor;
    std::string bgColor;
    std::string style;
    LabelFocusInfo focusInfo;
    bool touchable;
    std::string lineBreakMode;
};
struct UxViewInfo;
class TextLabelAdapter : public OHOS::UILabel, public ComponentCommon<TextLabelAdapter> {
    DISALLOW_COPY_MOVE(TextLabelAdapter);
    struct TextLabelOnFocusListener;
    static constexpr uint32_t MAX_FONT_SIZE = 200;
public:
    using SpecificInfoType = UxLabelInfo;
    static constexpr auto COMPONENT_TYPE = "UILabel";
    TextLabelAdapter();
    explicit TextLabelAdapter(const UxViewInfo &info);
    virtual ~TextLabelAdapter();
    bool OnPressEvent(const OHOS::PressEvent& event) override;
    void SetText(const std::string &txt);
    static bool IsValid(const UxLabelInfo &info);
private:
    void InitFocus(const OHOS::ColorType &fontColor, const OHOS::ColorType &bgColor,
        const OHOS::ColorType &focusedFontColor, const OHOS::ColorType &focusedBgColor);
    std::unique_ptr<TextLabelOnFocusListener> focusListener_ {};
};
} // namespace Updater
#endif
