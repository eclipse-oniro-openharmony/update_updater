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
#include "text_label_adapter.h"
#include "dock/focus_manager.h"
#include "language/language_ui.h"
#include "log/log.h"
#include "page/view_proxy.h"
#include "updater_ui_const.h"
#include "view_api.h"
namespace Updater {
struct TextLabelAdapter::TextLabelOnFocusListener : public OHOS::UIView::OnFocusListener {
    DISALLOW_COPY_MOVE(TextLabelOnFocusListener);
public:
    TextLabelOnFocusListener(const OHOS::ColorType &focusedFontColor, const OHOS::ColorType &unfocusedFontcolor,
        const OHOS::ColorType &focusedBgColor, const OHOS::ColorType &unfocusedBgcolor)
        : focusedFontcolor_(focusedFontColor), unfocusedFontcolor_(unfocusedFontcolor), focusedBgcolor_(focusedBgColor),
          unfocusedBgcolor_(unfocusedBgcolor)
    {}

    ~TextLabelOnFocusListener() {}

    bool OnFocus(OHOS::UIView &view) override
    {
        TextLabelAdapter *label = nullptr;
        if (view.GetViewType() != OHOS::UI_LABEL) {
            return false;
        }
        label = static_cast<TextLabelAdapter *>(&view);
        LOG(DEBUG) << "key OnFocus";
        label->SetStyle(OHOS::STYLE_TEXT_COLOR, focusedFontcolor_.full);
        label->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, focusedBgcolor_.full);
        label->Invalidate();
        return true;
    }

    bool OnBlur(OHOS::UIView &view) override
    {
        TextLabelAdapter *label = nullptr;
        if (view.GetViewType() != OHOS::UI_LABEL) {
            return false;
        }
        label = static_cast<TextLabelAdapter *>(&view);
        LOG(DEBUG) << "key OnBlur";
        label->SetStyle(OHOS::STYLE_TEXT_COLOR, unfocusedFontcolor_.full);
        label->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, unfocusedBgcolor_.full);
        label->Invalidate();
        return true;
    }
private:
    OHOS::ColorType focusedFontcolor_;
    OHOS::ColorType unfocusedFontcolor_;
    OHOS::ColorType focusedBgcolor_;
    OHOS::ColorType unfocusedBgcolor_;
};

TextLabelAdapter::TextLabelAdapter() = default;

TextLabelAdapter::~TextLabelAdapter() = default;

TextLabelAdapter::TextLabelAdapter(const UxViewInfo &info)
{
    const UxLabelInfo &spec = std::get<UxLabelInfo>(info.specificInfo);
    SetViewCommonInfo(info.commonInfo);
    this->SetAlign(GetAlign(spec.align), OHOS::TEXT_ALIGNMENT_CENTER);
    this->SetText(TranslateText(spec.text).c_str());
    this->SetFont((spec.style == "bold") ? BOLD_FONT_FILENAME : DEFAULT_FONT_FILENAME, spec.fontSize);
    this->SetStyle(OHOS::STYLE_LINE_HEIGHT, UPDATER_UI_FONT_HEIGHT_RATIO * spec.fontSize);
    auto fontColor = StrToColor(spec.fontColor);
    this->SetStyle(OHOS::STYLE_TEXT_COLOR, fontColor.full);
    this->SetStyle(OHOS::STYLE_TEXT_OPA, fontColor.alpha);
    auto bgColor = StrToColor(spec.bgColor);
    this->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, bgColor.full);
    this->SetStyle(OHOS::STYLE_BACKGROUND_OPA, bgColor.alpha);
    if (spec.focusable) {
        LOG(DEBUG) << "init focus listener for " << viewId_;
        InitFocus(fontColor, bgColor, StrToColor(spec.focusedFontColor),
            StrToColor(spec.focusedBgColor));
    } else {
        this->SetFocusable(false);
        this->SetTouchable(false);
    }

    if (spec.lineBreakMode == "marquee") {
        this->SetLineBreakMode(OHOS::UILabel::LINE_BREAK_MARQUEE);
        this->SetRollSpeed(80); // 80: label roll speed
    }
}

bool TextLabelAdapter::IsValid(const UxLabelInfo &info)
{
    if (info.fontSize > MAX_FONT_SIZE) {
        LOG(ERROR) << "label viewinfo check failed, fontSize: " << info.fontSize;
        return false;
    }

    if (!CheckColor(info.bgColor) || !CheckColor(info.fontColor)) {
        LOG(ERROR) << "label viewinfo check failed, bgColor:" << info.bgColor <<
            " fontColor:" << info.fontColor;
        return false;
    }
    return true;
}

void TextLabelAdapter::SetText(const std::string &txt)
{
    // usage of "*" is same as label button's SetText
    if (txt == "*") {
        return;
    }
    OHOS::UILabel::SetText(txt.c_str());
}

bool TextLabelAdapter::OnPressEvent(const OHOS::PressEvent &event)
{
    OHOS::FocusManager::GetInstance()->ClearFocus();
    OHOS::FocusManager::GetInstance()->RequestFocus(this);
    return UIView::OnPressEvent(event);
}

void TextLabelAdapter::InitFocus(const OHOS::ColorType &fontColor, const OHOS::ColorType &bgColor,
    const OHOS::ColorType &focusedFontColor, const OHOS::ColorType &focusedBgColor)
{
    focusListener_ = std::make_unique<TextLabelOnFocusListener>(focusedFontColor, fontColor, focusedBgColor, bgColor);
    this->SetFocusable(true);
    this->SetOnFocusListener(focusListener_.get());
}
} // namespace Updater
