/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <unordered_map>
#include "gtest/gtest.h"
#include "json_node.h"
#include "ui_strategy.h"

using namespace testing::ext;
using namespace Updater;

namespace Updater {
bool operator == (const ComInfo &lhs, const ComInfo &rhs)
{
    return lhs.comId == rhs.comId && lhs.pageId == rhs.pageId;
}

bool operator == (const ProgressPage &lhs, const ProgressPage &rhs)
{
    return lhs.logoComId == rhs.logoComId && lhs.logoType == rhs.logoType && lhs.progressComId == rhs.progressComId &&
        lhs.progressPageId == rhs.progressPageId && lhs.progressType == rhs.progressType &&
        lhs.warningComId == rhs.warningComId;
}

bool operator == (const ResPage &lhs, const ResPage &rhs)
{
    return lhs.successPageId == rhs.successPageId && lhs.failPageId == rhs.failPageId;
}

bool operator == (const UiStrategyCfg &lhs, const UiStrategyCfg &rhs)
{
    return lhs.confirmPageId == rhs.confirmPageId && lhs.labelLogId == rhs.labelLogId &&
        lhs.labelLogResId == rhs.labelLogResId && lhs.progressPage == rhs.progressPage &&
        lhs.labelUpdId == rhs.labelUpdId && lhs.resPage == rhs.resPage;
}

std::ostream &operator<<(std::ostream &os, const ComInfo &com)
{
    os << "pageId: " << com.pageId << " comId: " << com.comId;
    return os;
}
}

namespace {
class UpdaterUiStrategyUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(UpdaterUiStrategyUnitTest, test_load_invalid_strategy, TestSize.Level1)
{
    EXPECT_FALSE(UiStrategy::LoadStrategy(JsonNode {Fs::path {"/data/updater/ui/strategy/strategy_invalid.json"}}));
    EXPECT_TRUE(UiStrategy::GetStrategy().empty());
}

HWTEST_F(UpdaterUiStrategyUnitTest, test_load_strategy_for_each_mode, TestSize.Level1)
{
    EXPECT_TRUE(UiStrategy::LoadStrategy(JsonNode {Fs::path {"/data/updater/ui/strategy/strategy_valid.json"}}));
    UiStrategyCfg defaultCfg { "confirm", {"", ""}, {"", ""}, {"", ""}, ProgressPage {"upd:update",
        "ProgressUpdBoxDark_Progress", "bar", "OHOSIconDark_Image", "img", "PowerLongPressWarning_Image"},
        ResPage {"upd:updateSuccess", "upd:normalUpdateFailed"}
    };
    std::unordered_map<Updater::UpdaterMode, Updater::UiStrategyCfg> expected {};
    auto &sdCardCfg = expected[UpdaterMode::SDCARD];
    sdCardCfg = defaultCfg;
    sdCardCfg.progressPage = ProgressPage {"upd:sdUpdate", "UpdBox_Progress", "bar",
        "OHOSIcon_Image", "img", "PowerLongPressWarning_Image"};
    sdCardCfg.labelLogResId = {"upd", "UpdateInfoDark_Label"};
    sdCardCfg.resPage = {"upd:updateSuccess", "upd:updateFailedNoButton"};

    auto &factoryRstCfg = expected[UpdaterMode::FACTORYRST];
    factoryRstCfg = defaultCfg;
    factoryRstCfg.progressPage = ProgressPage {"upd:reset", "UpdBox_Progress", "bar", "OHOSIcon_Image", "img", ""};
    factoryRstCfg.labelLogResId = {"upd", "UpdateInfoDark_Label"};
    factoryRstCfg.labelUpdId = {"upd", "RstInfo_Label"};
    factoryRstCfg.resPage = {"menu:normal", "upd:FactoryRstFailed"};

    auto &rebootFactoryRstCfg = expected[UpdaterMode::REBOOTFACTORYRST];
    rebootFactoryRstCfg = defaultCfg;
    rebootFactoryRstCfg.labelLogResId = {"upd", "RstInfo_Label"};
    rebootFactoryRstCfg.progressPage = {"upd:reset", "UpdBox_Progress", "bar", "OHOSIcon_Image", "img", ""};
    rebootFactoryRstCfg.resPage = {"upd:reset", "upd:FactoryRstFailed"};

    auto &otaCfg = expected[UpdaterMode::OTA];
    otaCfg = defaultCfg;
    EXPECT_EQ(UiStrategy::GetStrategy(), (std::unordered_map<Updater::UpdaterMode, Updater::UiStrategyCfg> {
        {UpdaterMode::SDCARD, sdCardCfg}, {UpdaterMode::FACTORYRST, factoryRstCfg},
        {UpdaterMode::REBOOTFACTORYRST, rebootFactoryRstCfg}, {UpdaterMode::OTA, otaCfg}
    }));
}
} // namespace
