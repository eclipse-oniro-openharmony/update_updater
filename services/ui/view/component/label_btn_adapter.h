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
#ifndef UPDATER_UI_LABEL_BTN_ADAPTER_H
#define UPDATER_UI_LABEL_BTN_ADAPTER_H

#include <memory>
#include "component_common.h"
#include "components/ui_label_button.h"
#include "macros_updater.h"

namespace Updater {
struct UxLabelBtnInfo {
    uint8_t fontSize;
    std::string text;
    std::string txtColor;
    std::string bgColor;
    std::string focusedTxtColor;
    std::string focusedBgColor;
    bool focusable;
};
struct UxViewInfo;
class LabelBtnAdapter : public OHOS::UILabelButton, public ComponentCommon<LabelBtnAdapter> {
    DISALLOW_COPY_MOVE(LabelBtnAdapter);
    struct LabelBtnOnFocusListener;
    static constexpr uint32_t MAX_FONT_SIZE = 200;
public:
    using SpecificInfoType = UxLabelBtnInfo;
    static constexpr auto COMPONENT_TYPE = "UILabelButton";
    LabelBtnAdapter();
    explicit LabelBtnAdapter(const UxViewInfo &info);
    virtual ~LabelBtnAdapter();
    bool OnPressEvent(const OHOS::PressEvent& event) override;
    void SetText(const std::string &txt);
    static bool IsValid(const UxLabelBtnInfo &info);
private:
    void InitFocus(const OHOS::ColorType &txtColor, const OHOS::ColorType &bgColor,
        const OHOS::ColorType &focusedTxtColor, const OHOS::ColorType &focusedBgColor);
    std::unique_ptr<LabelBtnOnFocusListener> focusListener_ {};
};
} // namespace Updater
#endif
