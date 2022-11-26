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
#include "common/text.h"
#include "file_ex.h"
#include "gtest/gtest.h"
#include "json_node.h"
#include "language_ui.h"
#include "log.h"
#include "view_api.h"

namespace {
using namespace Updater;
using namespace Lang;
using namespace testing::ext;

class UpdaterUiViewApiTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(UpdaterUiViewApiTest, test_get_color_from_str, TestSize.Level0)
{
    static const std::vector<std::pair<const char *, std::pair<OHOS::ColorType, bool>>> colorDatas {
        {"#00ffffff", {OHOS::Color::GetColorFromRGBA(0, 0xff, 0xff, 0xff), true}},
        {"#aabbccdd", {OHOS::Color::GetColorFromRGBA(0xaa, 0xbb, 0xcc, 0xdd), true}},
        {"#AABBCCDD", {OHOS::Color::GetColorFromRGBA(0xaa, 0xbb, 0xcc, 0xdd), true}},
        {"#FFFFFFGF", {OHOS::Color::GetColorFromRGBA(0, 0, 0, 0xff), false}},
        {"#0011223", {OHOS::Color::GetColorFromRGBA(0, 0, 0, 0xff), false}},
        {"#001122333", {OHOS::Color::GetColorFromRGBA(0, 0, 0, 0xff), false}},
        {"A1B2C3D4", {OHOS::Color::GetColorFromRGBA(0, 0, 0, 0xff), false}},
    };
    bool isValid {};
    for (auto &colorData : colorDatas) {
        isValid = Updater::CheckColor(colorData.first);
        ASSERT_EQ(isValid, colorData.second.second);
        if (isValid) {
            EXPECT_EQ(Updater::StrToColor(colorData.first).full, colorData.second.first.full);
        }
    }
}

HWTEST_F(UpdaterUiViewApiTest, test_translate_text, TestSize.Level0)
{
    constexpr auto localeFile = "/data/updater/locale";
    EXPECT_EQ(OHOS::SaveStringToFile(localeFile, "en"), true);
    string jsonStr = R"({"locale":{
        "res" : [
            {
                "path" : "/data/updater/ui/l0.json",
                "level" : 0
            }
        ],
        "localeFile" : "/data/updater/locale"}})";
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {jsonStr}), true);
    EXPECT_EQ(TranslateText("[]"), "[]");
    EXPECT_EQ(TranslateText("{REBOOT_DEVICE]"), "{REBOOT_DEVICE]");
    EXPECT_EQ(TranslateText("[REBOOT_DEVICE}"), "[REBOOT_DEVICE}");
    EXPECT_EQ(TranslateText("[REBOOT_DEVICE]"), "reboot devices");
    EXPECT_EQ(TranslateText("reboot devices"), "reboot devices");
    unlink(localeFile);
}

HWTEST_F(UpdaterUiViewApiTest, test_get_align, TestSize.Level0)
{
    EXPECT_EQ(GetAlign("left"), OHOS::TEXT_ALIGNMENT_LEFT);
    EXPECT_EQ(GetAlign("right"), OHOS::TEXT_ALIGNMENT_RIGHT);
    EXPECT_EQ(GetAlign("center"), OHOS::TEXT_ALIGNMENT_CENTER);
    EXPECT_EQ(GetAlign("top"), OHOS::TEXT_ALIGNMENT_TOP);
    EXPECT_EQ(GetAlign("bottom"), OHOS::TEXT_ALIGNMENT_BOTTOM);
    EXPECT_EQ(GetAlign("unknow"), OHOS::TEXT_ALIGNMENT_CENTER);
}
}