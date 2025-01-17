/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "auto_layout.h"
#include "log/log.h"
namespace Updater {
void AutoLayout::RegisterHelper(std::unique_ptr<LayoutInterface> ptr)
{
    helper_ = std::move(ptr);
}

AutoLayout &AutoLayout::GetInstance()
{
    static AutoLayout layout;
    return layout;
}

void AutoLayout::Init()
{
    if (isInited_ || helper_ == nullptr) {
        LOG(ERROR) << "helper_ null error or already has been inited";
        return;
    }
    helper_->Init();
    isInited_ = true;
}

void AutoLayout::SetJsonLocation(JsonNode &root)
{
    if (helper_ == nullptr) {
        LOG(ERROR) << "helper_ null error";
        return;
    }
    helper_->SetJsonLocation(root);
}

bool AutoLayout::SetCompLocation(JsonNode &node)
{
    if (helper_ == nullptr) {
        LOG(ERROR) << "helper_ null error";
        return false;
    }
    return helper_->SetComLocation(node);
}

bool AutoLayout::IsInited()
{
    return isInited_;
}
}