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

#ifndef UPDATER_UI_CONFIG_H
#define UPDATER_UI_CONFIG_H

#include "strategy/ui_strategy.h"
#include "updater_ui.h"
#include "view_api.h"

#include <filesystem>
#include <unordered_map>

namespace Updater {
class UpdaterUiConfig {
public:
    static bool Init();
    static std::string_view GetMainPage();
    static std::unordered_map<UpdaterMode, UiStrategyCfg> &GetStrategy();
    static std::vector<UxPageInfo> &GetPageInfos();
    static bool GetFocusCfg();
private:
    static bool Init(const JsonNode &node);
    static bool LoadStrategy(const JsonNode &node);
    static bool LoadLayout(const JsonNode &node);
    static bool LoadLangRes(const JsonNode &node);
    static bool LoadCallbacks(const JsonNode &node);
    static bool LoadFocusCfg(const JsonNode &node);
    static PagePath pagePath_;
    static bool isFocusEnable_;
    static std::vector<UxPageInfo> pageInfos_;
};
}
#endif
