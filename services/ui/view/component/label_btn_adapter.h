/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef UPDATER_UI_LABEL_BUTTON_H
#define UPDATER_UI_LABEL_BUTTON_H

#include "components/ui_label_button.h"
#include "view_api.h"

namespace Updater {
class LabelBtnAdapter : public OHOS::UILabelButton {
    struct LabelBtnOnFocusListener;
    static constexpr uint32_t MAX_FONT_SIZE = 255;
public:
    using SpecificInfoType = UxLabelBtnInfo;
    LabelBtnAdapter();
    explicit LabelBtnAdapter(const UxViewInfo &info);
    virtual ~LabelBtnAdapter();
    bool OnPressEvent(const OHOS::PressEvent& event) override;
    void SetText(const std::string &txt);
    static bool IsValid(const UxLabelBtnInfo &info);
private:
    void InitFocus(const UxLabelBtnInfo *specPtr);
    std::string viewId_ {};
    std::unique_ptr<LabelBtnOnFocusListener> focusListener_ {};
};
} // namespace Updater
#endif
