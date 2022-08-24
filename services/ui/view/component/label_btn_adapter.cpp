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
    LabelBtnOnFocusListener(UxBRGAPixel focusedTxtColor, UxBRGAPixel unfocusedTxtcolor,
        UxBRGAPixel focusedBgColor, UxBRGAPixel unfocusedBgcolor)
        : focusedTxtcolor_(focusedTxtColor), unfocusedTxtcolor_(unfocusedTxtcolor),
        focusedBgcolor_(focusedBgColor), unfocusedBgcolor_(unfocusedBgcolor)
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
        button->SetLabelStyle(OHOS::STYLE_TEXT_COLOR, GetColor(focusedTxtcolor_));
        button->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, GetColor(focusedBgcolor_));
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
        button->SetLabelStyle(OHOS::STYLE_TEXT_COLOR, GetColor(unfocusedTxtcolor_));
        button->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, GetColor(unfocusedBgcolor_));
        button->Invalidate();
        return true;
    }
private:
    UxBRGAPixel focusedTxtcolor_;
    UxBRGAPixel unfocusedTxtcolor_;
    UxBRGAPixel focusedBgcolor_;
    UxBRGAPixel unfocusedBgcolor_;
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
    this->SetFont(FONT_NAME, specPtr->fontSize);
    this->SetLabelStyle(OHOS::STYLE_TEXT_COLOR, OHOS::Color::GetColorFromRGB(
        specPtr->txtColor.r, specPtr->txtColor.g, specPtr->txtColor.b).full);
    this->SetLabelStyle(OHOS::STYLE_TEXT_OPA, specPtr->txtColor.a);
    this->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, OHOS::Color::GetColorFromRGB(
        specPtr->bgColor.r, specPtr->bgColor.g, specPtr->bgColor.b).full);
    this->SetStyle(OHOS::STYLE_BACKGROUND_OPA, specPtr->bgColor.a);
    if (specPtr->focusable) {
        LOG(INFO) << "init focus listener for " << viewId_;
        InitFocus(specPtr);
    }
}

bool LabelBtnAdapter::IsValid(const UxLabelBtnInfo &info)
{
    if (info.fontSize > MAX_FONT_SIZE) {
        LOG(ERROR) << "label viewinfo check failed, fontSize: " << info.fontSize;
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

void LabelBtnAdapter::InitFocus(const UxLabelBtnInfo *specPtr)
{
    this->SetFocusable(true);
    focusListener_ = std::make_unique<LabelBtnOnFocusListener>(specPtr->focusedTxtColor, specPtr->txtColor,
        specPtr->focusedBgColor, specPtr->bgColor);
    this->SetOnFocusListener(focusListener_.get());
}
} // namespace Updater
