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
    : width_ {}, height_ {}, root_ { std::make_unique<OHOS::UIViewGroup>() },
    coms_ {}, comsMap_ {}, pageId_ {}, extraMsg_ {}, color_ { 0, 0, 0, 255 }
{
}

BasePage::BasePage(uint16_t width, uint16_t height)
    : width_ { width }, height_ { height }, root_ { std::make_unique<OHOS::UIViewGroup>() },
    coms_ {}, comsMap_ {}, pageId_ {}, extraMsg_ {}, color_ { 0, 0, 0, 255 }
{
}

void BasePage::BuildPage(const UxPageInfo &pageInfo)
{
    pageId_ = pageInfo.id;
    color_ = pageInfo.bgColor;
    root_->SetViewId(pageId_.c_str());
    root_->SetPosition(0, 0, width_, height_);
    root_->SetVisible(true);
    root_->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, GetColor(color_));
    root_->SetStyle(OHOS::STYLE_BACKGROUND_OPA, color_.a);
    extraMsg_.elementPtr = this;
    BuildComs(pageInfo);
}

void BasePage::BuildComs(const UxPageInfo &pageInfo)
{
    const auto &infos = pageInfo.viewInfos;
    coms_.resize(infos.size());
    int16_t minY = INT16_MAX;
    for (size_t i = 0; i < infos.size(); ++i) {
        if (!ComponentFactory::CheckUxComponent(infos[i])) {
            LOG(ERROR) << "component (" << infos[i].commonInfo.id << ") is invalid, please check your page config json";
            continue;
        }
        coms_[i] = ComponentFactory::CreateUxComponent(infos[i]);
        // set parent page into component's extraMsg(set by BuildPage), used by CallbackDecorator
        coms_[i]->SetExtraMsg(&extraMsg_);
        if (coms_[i]->GetViewId() == nullptr) {
            LOG(ERROR) << "component is invalid, please check your page config json";
            continue;
        }
        if (coms_[i]->IsFocusable() && infos[i].commonInfo.y < minY) {
            minY = infos[i].commonInfo.y;
            focusedView_ = coms_[i].get();
        }
        comsMap_[coms_[i]->GetViewId()] = coms_[i].get();
        root_->Add(coms_[i].get());
    }
}

ViewProxy BasePage::operator[](const std::string &id)
{
    OHOS::UIView *ret = nullptr;
    auto it = comsMap_.find(id);
    if (it != comsMap_.end()) {
        ret = it->second;
    }
    return ViewProxy(ret, pageId_ + "[" + id + "]");
}

bool BasePage::IsValidCom(const std::string &id) const
{
    return comsMap_.find(id) != comsMap_.end();
}

bool BasePage::IsValid() const
{
    return root_ != nullptr;
}

std::string &BasePage::GetPageId()
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
        root_->SetStyle(OHOS::STYLE_BACKGROUND_COLOR, GetColor(color_));
        root_->SetStyle(OHOS::STYLE_BACKGROUND_OPA, color_.a);
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
    if (root_ == nullptr) {
        LOG(ERROR) << "root is null";
        return nullptr;
    }
    return root_.get();
}
}
