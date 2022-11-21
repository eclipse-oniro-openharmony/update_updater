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

#include "base_page.h"
#include "component/component_factory.h"
#include "dock/focus_manager.h"
#include "log/log.h"

namespace Updater {
using namespace OHOS;
BasePage::BasePage()
    : width_ {}, height_ {}, root_ {std::make_unique<OHOS::UIViewGroup>()},
    coms_ {}, comsMap_ {}, pageId_ {}, color_ {Color::Black()}
{
}

BasePage::BasePage(int16_t width, int16_t height)
    : width_ {width}, height_ {height}, root_ {std::make_unique<OHOS::UIViewGroup>()},
    coms_ {}, comsMap_ {}, pageId_ {}, color_ {Color::Black()}
{
}

bool BasePage::IsPageInfoValid(const UxPageInfo &pageInfo)
{
    if (pageInfo.id.empty()) {
        LOG(ERROR) << "page id is empty";
        return false;
    }
    if (!CheckColor(pageInfo.bgColor)) {
        LOG(ERROR) << "page color not valid, bgcolor: " << pageInfo.bgColor;
        return false;
    }
    return true;
}

void BasePage::Reset()
{
    root_->RemoveAll();
    coms_.clear();
    comsMap_.clear();
    focusedView_ = nullptr;
}

bool BasePage::BuildPage(const UxPageInfo &pageInfo)
{
    Reset();
    pageId_ = pageInfo.id;
    color_ = StrToColor(pageInfo.bgColor);
    root_->SetViewId(pageId_.c_str());
    root_->SetPosition(0, 0, width_, height_);
    root_->SetVisible(true);
    root_->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, color_.full);
    root_->SetStyle(OHOS::STYLE_BACKGROUND_OPA, color_.alpha);
    return BuildComs(pageInfo);
}

bool BasePage::BuildComs(const UxPageInfo &pageInfo)
{
    const auto &infos = pageInfo.viewInfos;
    coms_.reserve(infos.size());
    int minY = INT_MAX;
    for (auto &info : infos) {
        if (!BuildCom(info, minY)) {
            return false;
        }
    }
    return true;
}

bool BasePage::BuildCom(const UxViewInfo &viewInfo, int &minY)
{
    if (!ComponentFactory::CheckUxComponent(viewInfo)) {
        LOG(ERROR) << "component (" << viewInfo.commonInfo.id
            << ") is invalid, please check your page config json";
        return false;
    }
    auto upView = std::make_unique<ViewProxy>(ComponentFactory::CreateUxComponent(viewInfo),
        std::string("component id:") + viewInfo.commonInfo.id + ", ");
    auto &view = *upView;
    if (view->GetViewId() == nullptr) {
        LOG(ERROR) << "id is nullptr";
        return false;
    }
    if (view->IsFocusable() && viewInfo.commonInfo.y < minY) {
        minY = viewInfo.commonInfo.y;
        focusedView_ = view.operator->();
    }
    coms_.push_back(std::move(upView));
    root_->Add(view.operator->());
    // empty id is allowed. id is needed only when get specific view by id.
    if (std::string(view->GetViewId()).empty()) {
        return true; // skip this view. build com success, but not save in map
    }
    // only map non-empty id
    if (!comsMap_.emplace(view->GetViewId(), coms_.back().get()).second) {
        LOG(ERROR) << "view id duplicated:" << view->GetViewId();
    }
    return true;
}

ViewProxy &BasePage::operator[](const std::string &id)
{
    static ViewProxy dummy;
    auto it = comsMap_.find(id);
    if (it != comsMap_.end() && it->second != nullptr) {
        return *it->second;
    }
    return dummy;
}

bool BasePage::IsValidCom(const std::string &id) const
{
    return comsMap_.find(id) != comsMap_.end();
}

bool BasePage::IsValid() const
{
    return root_ != nullptr;
}

std::string BasePage::GetPageId()
{
    return pageId_;
}

void BasePage::SetVisible(bool isVisible)
{
    if (root_ == nullptr) {
        LOG(ERROR) << "root is null";
        return;
    }
    root_->SetVisible(isVisible);
    if (isVisible) {
        // change background
        root_->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, color_.full);
        root_->SetStyle(OHOS::STYLE_BACKGROUND_OPA, color_.alpha);
    }
    UpdateFocus(isVisible);
}

bool BasePage::IsVisible() const
{
    if (root_ == nullptr) {
        LOG(ERROR) << "root is null";
        return false;
    }
    return root_->IsVisible();
}

OHOS::UIViewGroup *BasePage::GetView() const
{
    return root_.get();
}
} // namespace Updater
