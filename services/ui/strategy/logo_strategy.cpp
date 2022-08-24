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

#include "logo_strategy.h"
#include <functional>
#include "component/box_progress_adapter.h"
#include "component/img_view_adapter.h"
#include "component/text_label_adapter.h"
#include "log/log.h"
#include "updater_ui_env.h"

namespace Updater {
std::unique_ptr<LogoStrategy> LogoStrategy::Factory(const std::string &type, const ComInfo &id)
{
    using Fun = std::function<std::unique_ptr<LogoStrategy>(const ComInfo &)>;
    const static std::unordered_map<std::string, Fun> logoMap {
        { "img", [] (const ComInfo &id) { return std::make_unique<ImageLogo>(id); }},
        { "anim", [] (const ComInfo &id) { return std::make_unique<AnimatorLogo>(id); }},
    };
    if (auto it = logoMap.find(type); it != logoMap.end()) {
        return it->second(id);
    }
    LOG(ERROR) << "not recognized logo type " << type;
    return std::make_unique<ImageLogo>(id);
}

void AnimatorLogo::Show() const
{
    pgMgr_[id_]->SetVisible(true);
    pgMgr_[id_].As<ImgViewAdapter>()->Start();
}

void AnimatorLogo::Hide() const
{
    pgMgr_[id_].As<ImgViewAdapter>()->Stop();
    pgMgr_[id_]->SetVisible(false);
}

void ImageLogo::Show() const
{
    pgMgr_[id_]->SetVisible(true);
}

void ImageLogo::Hide() const
{
    pgMgr_[id_]->SetVisible(false);
}
} // namespace Updater