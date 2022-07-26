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

#include "sub_page.h"
#include "dock/focus_manager.h"
#include "log/log.h"

namespace Updater {
SubPage::SubPage(UxSubPageInfo &subpageInfo, BasePage &basePage, const std::string &pageId)
    : basePage_(basePage), pageId_(pageId), comsId_(std::move(subpageInfo.coms)), isVisible_(false)
{
    color_ = subpageInfo.bgColor;
    int minY = INT16_MAX;
    std::string focusedId {};
    for (auto &id : comsId_) {
        if (basePage.IsValidCom(id) && basePage[id]->IsFocusable() && basePage[id]->GetY() < minY) {
            minY = basePage[id]->GetY();
            focusedId = id;
        }
    }
    if (!focusedId.empty()) {
        focusedView_ = basePage[focusedId].As();
    }
}

std::string &SubPage::GetPageId()
{
    return pageId_;
}

void SubPage::SetVisible(bool isVisible)
{
    isVisible_ = isVisible;
    auto view = basePage_.GetView();
    if (!view) {
        LOG(ERROR) << "basepage's view is nullptr";
        return;
    }

    for (const auto &id : comsId_) {
        basePage_[id]->SetVisible(isVisible);
    }
    view->SetVisible(isVisible);
    if (isVisible) {
        // change background
        view->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, GetColor(color_));
        view->SetStyle(OHOS::STYLE_BACKGROUND_OPA, color_.a);
    }
    UpdateFocus(isVisible);
}
bool SubPage::IsVisible() const
{
    return isVisible_;
}

OHOS::UIViewGroup *SubPage::GetView() const
{
    return basePage_.GetView();
}

bool SubPage::IsValid() const
{
    return basePage_.IsValid();
}

bool SubPage::IsValidCom(const std::string &id) const
{
    return basePage_.IsValidCom(id);
}

ViewProxy SubPage::operator[](const std::string &id)
{
    return basePage_[id];
}
}