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

#include "progress_strategy.h"
#include <functional>
#include <string>
#include <unordered_map>
#include "component/box_progress_adapter.h"
#include "component/img_view_adapter.h"
#include "component/text_label_adapter.h"
#include "log/log.h"
#include "updater/updater_const.h"
#include "updater_ui_env.h"
#include "updater_ui.h"

namespace Updater {
std::unique_ptr<ProgressStrategy> ProgressStrategy::Factory(const std::string &type, const ComInfo &id)
{
    using Fun = std::function<std::unique_ptr<ProgressStrategy>(const ComInfo &)>;
    const static std::unordered_map<std::string, Fun> prgrsMap = {
        { "txt", [] (const ComInfo &id) { return std::make_unique<TxtProgress>(id); }},
        { "bar", [] (const ComInfo &id) { return std::make_unique<BarProgress>(id); }}
    };
    if (auto it = prgrsMap.find(type); it != prgrsMap.end()) {
        return it->second(id);
    }
    LOG(ERROR) << "not recognized progress type " << type;
    return std::make_unique<BarProgress>(id);
}

void ProgressStrategy::Show() const
{
    ShowProgress(0);
    pgMgr_[id_]->SetVisible(true);
}

void ProgressStrategy::Hide() const
{
    pgMgr_[id_]->SetVisible(false);
}

void TxtProgress::ShowProgress(float value) const
{
    int intValue = static_cast<int>(value);
    if (intValue < 0 || intValue > FULL_PERCENT_PROGRESS) {
        LOG(ERROR) << "progress value invalid:" << intValue;
        return;
    }
    std::string progress = std::to_string(intValue) + "%";
    pgMgr_[id_].As<TextLabelAdapter>()->SetText(progress);
}

BarProgress::BarProgress(const ComInfo &id) : ProgressStrategy(id)
{
    if (!pgMgr_[id_].As<BoxProgressAdapter>()->InitEp()) {
        LOG(ERROR) << "bar progress's endpoint init failed";
    }
}

void BarProgress::ShowProgress(float value) const
{
    pgMgr_[id_].As<BoxProgressAdapter>()->SetValue(value);
}
} // namespace Updater