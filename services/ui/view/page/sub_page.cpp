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
SubPage::SubPage() : basePage_ {}, pageId_ {}, comsId_ {}, isVisible_ {false}, color_ {0, 0, 0, 255}
{
}

SubPage::SubPage(UxSubPageInfo &subpageInfo, const std::shared_ptr<Page> &basePage, const std::string &pageId)
    : basePage_ {basePage}, pageId_ {pageId}, comsId_ {std::move(subpageInfo.coms)}, isVisible_ {false},
    color_ {subpageInfo.bgColor}
{
}

bool SubPage::BuildSubPage()
{
    if (!IsValid()) {
        return false;
    }
    int minY = INT16_MAX;
    std::string focusedId {};
    for (auto &id : comsId_) {
        if (basePage_->IsValidCom(id) && (*basePage_)[id]->IsFocusable() && (*basePage_)[id]->GetY() < minY) {
            minY = (*basePage_)[id]->GetY();
            focusedId = id;
        }
    }
    if (!focusedId.empty()) {
        focusedView_ = (*basePage_)[focusedId].As();
    }
    return true;
}

bool SubPage::IsPageInfoValid(const UxSubPageInfo &info)
{
    if (info.id.empty()) {
        LOG(ERROR) << "sub page id is empty";
        return false;
    }
    return true;
}

std::string SubPage::GetPageId()
{
    return pageId_;
}

void SubPage::SetVisible(bool isVisible)
{
    if (!IsValid()) {
        return;
    }
    isVisible_ = isVisible;
    const auto &view = basePage_->GetView();
    if (view == nullptr) {
        LOG(ERROR) << "basepage's view is nullptr";
        return;
    }

    for (const auto &id : comsId_) {
        (*basePage_)[id]->SetVisible(isVisible);
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
    if (!IsValid()) {
        return nullptr;
    }
    return basePage_->GetView();
}

bool SubPage::IsValid() const
{
    if (basePage_ == nullptr || !basePage_->IsValid()) {
        LOG(ERROR) << "basepage of subpage is null";
        return false;
    }
    return true;
}

bool SubPage::IsValidCom(const std::string &id) const
{
    if (!IsValid()) {
        return false;
    }
    return basePage_->IsValidCom(id);
}

ViewProxy &SubPage::operator[](const std::string &id)
{
    static ViewProxy dummy;
    if (!IsValid()) {
        return dummy;
    }
    return (*basePage_)[id];
}
} // namespace Updater