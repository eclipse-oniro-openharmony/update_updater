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
#include "label_btn_adapter.h"
#include "dock/focus_manager.h"
#include "log/log.h"
#include "page/view_proxy.h"
#include "updater_ui_const.h"

namespace Updater {
struct LabelBtnAdapter::LabelBtnOnFocusListener : public OHOS::UIView::OnFocusListener {
    DISALLOW_COPY_MOVE(LabelBtnOnFocusListener);
public:
    LabelBtnOnFocusListener(const OHOS::ColorType &focusedTxtColor, const OHOS::ColorType &unfocusedTxtcolor,
        const OHOS::ColorType &focusedBgColor, const OHOS::ColorType &unfocusedBgcolor)
        : focusedTxtcolor_(focusedTxtColor), unfocusedTxtcolor_(unfocusedTxtcolor), focusedBgcolor_(focusedBgColor),
          unfocusedBgcolor_(unfocusedBgcolor)
    {}

    ~LabelBtnOnFocusListener() {}

    bool OnFocus(OHOS::UIView &view) override
    {
        LabelBtnAdapter *button = nullptr;
        if (view.GetViewType() != OHOS::UI_LABEL_BUTTON) {
            return false;
        }
        button = static_cast<LabelBtnAdapter *>(&view);
        LOG(DEBUG) << "key OnFocus";
        button->SetLabelStyle(OHOS::STYLE_TEXT_COLOR, focusedTxtcolor_.full);
        button->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, focusedBgcolor_.full);
        button->Invalidate();
        return true;
    }

    bool OnBlur(OHOS::UIView &view) override
    {
        LabelBtnAdapter *button = nullptr;
        if (view.GetViewType() != OHOS::UI_LABEL_BUTTON) {
            return false;
        }
        button = static_cast<LabelBtnAdapter *>(&view);
        LOG(DEBUG) << "key OnBlur";
        button->SetLabelStyle(OHOS::STYLE_TEXT_COLOR, unfocusedTxtcolor_.full);
        button->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, unfocusedBgcolor_.full);
        button->Invalidate();
        return true;
    }
private:
    OHOS::ColorType focusedTxtcolor_;
    OHOS::ColorType unfocusedTxtcolor_;
    OHOS::ColorType focusedBgcolor_;
    OHOS::ColorType unfocusedBgcolor_;
};

LabelBtnAdapter::LabelBtnAdapter() = default;

LabelBtnAdapter::~LabelBtnAdapter() = default;

LabelBtnAdapter::LabelBtnAdapter(const UxViewInfo &info)
{
    const UxViewCommonInfo *commonPtr = &info.commonInfo;
    const UxLabelBtnInfo *specPtr = &std::get<UxLabelBtnInfo>(info.specificInfo);
    viewId_ = commonPtr->id;
    this->SetPosition(commonPtr->x, commonPtr->y);
    this->SetWidth(commonPtr->w);
    this->SetHeight(commonPtr->h);
    this->SetVisible(commonPtr->visible);
    this->SetViewId(viewId_.c_str());
    this->SetText(TranslateText(specPtr->text).c_str());
    this->SetFont(DEFAULT_FONT_FILENAME, specPtr->fontSize);
    auto txtColor = StrToColor(specPtr->txtColor);
    this->SetLabelStyle(OHOS::STYLE_TEXT_COLOR, txtColor.full);
    this->SetLabelStyle(OHOS::STYLE_TEXT_OPA, txtColor.alpha);
    auto bgColor = StrToColor(specPtr->bgColor);
    this->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, bgColor.full);
    this->SetStyle(OHOS::STYLE_BACKGROUND_OPA, bgColor.alpha);
    if (specPtr->focusable) {
        LOG(INFO) << "init focus listener for " << viewId_;
        InitFocus(txtColor, bgColor, StrToColor(specPtr->focusedTxtColor),
            StrToColor(specPtr->focusedBgColor));
    }
}

bool LabelBtnAdapter::IsValid(const UxLabelBtnInfo &info)
{
    if (info.fontSize > MAX_FONT_SIZE) {
        LOG(ERROR) << "label viewinfo check failed, fontSize: " << info.fontSize;
        return false;
    }

    if (!CheckColor(info.bgColor) || !CheckColor(info.focusedBgColor) ||
        !CheckColor(info.txtColor) || !CheckColor(info.focusedTxtColor)) {
        LOG(ERROR) << "label viewinfo check failed, bgColor:" << info.bgColor <<
            " focusedBgColor:" << info.focusedBgColor <<
            " txtColor:" << info.txtColor <<
            " focusedTxtColor: " << info.focusedTxtColor;
        return false;
    }

    return true;
}

void LabelBtnAdapter::SetText(const std::string &txt)
{
    /**
     * if argument txt is "*", then won't change the content of this label,
     * ignore this txt and keep label button text as before. "*" is normally
     * used as DEFAULT_STRING, which can ignore unused string
     */
    if (txt == "*") {
        return;
    }
    OHOS::UILabelButton::SetText(txt.c_str());
}

bool LabelBtnAdapter::OnPressEvent(const OHOS::PressEvent &event)
{
    OHOS::FocusManager::GetInstance()->ClearFocus();
    OHOS::FocusManager::GetInstance()->RequestFocus(this);
    return UIButton::OnPressEvent(event);
}

void LabelBtnAdapter::InitFocus(const OHOS::ColorType &txtColor, const OHOS::ColorType &bgColor,
    const OHOS::ColorType &focusedTxtColor, const OHOS::ColorType &focusedBgColor)
{
    focusListener_ = std::make_unique<LabelBtnOnFocusListener>(focusedTxtColor, txtColor, focusedBgColor, bgColor);
    this->SetFocusable(true);
    this->SetOnFocusListener(focusListener_.get());
}
} // namespace Updater
