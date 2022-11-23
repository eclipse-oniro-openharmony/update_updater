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

#ifndef UI_STRATEGY_H
#define UI_STRATEGY_H

#include <ostream>
#include <string>
#include "logo_strategy.h"
#include "progress_strategy.h"
#include "updater_ui_traits.h"

namespace Updater {
std::ostream &operator<<(std::ostream &os, const UiStrategyCfg &info);
std::ostream &operator<<(std::ostream &os, const ResPage &info);
std::ostream &operator<<(std::ostream &os, const ProgressPage &info);

class UiStrategy {
public:
    static const std::unordered_map<UpdaterMode, UiStrategyCfg> &GetStrategy();
    static bool LoadStrategy(const JsonNode &node);
private:
    static bool LoadStrategy(const JsonNode &node, UpdaterMode mode);
    static constexpr auto DEFAULT_KEY = "default";
    static std::unordered_map<UpdaterMode, UiStrategyCfg> strategies_;
    static std::unordered_map<UpdaterMode, std::string> modeStr_;
};
} // namespace Updater
#endif
