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

#include "ui_strategy.h"
#include <array>

namespace Updater {
std::ostream &operator<<(std::ostream &os, const UiStrategyCfg &info)
{
    os << "confirmPageId: " << info.confirmPageId << std::endl;
    os << "labelLogId: { " << info.labelLogId << " }" << std::endl;
    os << "labelLogResId: { " << info.labelLogResId << " }" << std::endl;
    os << "labelUpdId: { " << info.labelUpdId << " }" << std::endl;
    os << info.progressPage << std::endl;
    os << info.resPage;
    return os;
}

std::ostream &operator<<(std::ostream &os, const ResPage &info)
{
    os << "resPage: {" << std::endl;
    os << "\tsucessPageId: " << info.successPageId << std::endl;
    os << "\tfailPageId: " << info.failPageId << std::endl;
    os << "}" << std::endl;
    return os;
}

std::ostream &operator<<(std::ostream &os, const ProgressPage &info)
{
    os << "progressPage: {" << std::endl;
    os << "\tprocessPageId: " << info.progressPageId << std::endl;
    os << "\tprgrsComId: " << info.progressComId << std::endl;
    os << "\tprgrsType: " << info.progressType << std::endl;
    os << "\tlogoComId: " << info.logoComId << std::endl;
    os << "\tlogoType: " << info.logoType << std::endl;
    os << "}";
    return os;
}

std::unordered_map<std::string, UiStrategyCfg> UiStrategy::strategies_;
std::vector<std::string> UiStrategy::modeStr_ = {
    {UPDATREMODE_SDCARD},
    {UPDATREMODE_FACTORYRST},
    {UPDATREMODE_REBOOTFACTORYRST},
    {UPDATREMODE_OTA},
    {UPDATREMODE_RECOVER},
};

void UiStrategy::RegisterUiMode(const std::string &mode)
{
    LOG(INFO) << "RegisterUiMode " << mode;
    modeStr_.emplace_back(mode);
}

const std::unordered_map<std::string, UiStrategyCfg> &UiStrategy::GetStrategy()
{
    return strategies_;
}

bool UiStrategy::LoadStrategy(const JsonNode &node, const std::string &mode)
{
    auto it = std::find(modeStr_.begin(), modeStr_.end(), mode);
    if (it == modeStr_.end()) {
        return false;
    }
    const JsonNode &defaultNode = node[Traits<UiStrategyCfg>::STRUCT_KEY][DEFAULT_KEY];
    const JsonNode &specificNode = node[Traits<UiStrategyCfg>::STRUCT_KEY][*it];
    if (!Visit<SETVAL>(specificNode, defaultNode, strategies_[mode])) {
        LOG(ERROR) << "parse strategy config error";
        return false;
    }
    LOG(DEBUG) << "mode " << mode << "\n" << strategies_[mode];
    return true;
}

bool UiStrategy::LoadStrategy(const JsonNode &node)
{
    std::unordered_map<std::string, UiStrategyCfg>().swap(strategies_);
    for (auto mode : modeStr_) {
        if (!LoadStrategy(node, mode)) {
            LOG(ERROR) << "load strategy " << mode << " failed";
            std::unordered_map<std::string, UiStrategyCfg>().swap(strategies_);
            return false;
        }
    }
    return true;
}
} // namespace Updater