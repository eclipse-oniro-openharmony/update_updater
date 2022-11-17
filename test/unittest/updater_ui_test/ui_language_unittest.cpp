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

#include <sstream>
#include <string>
#include "file_ex.h"
#include "gtest/gtest.h"
#include "language/language_ui.h"

using namespace testing::ext;
using namespace Updater;
using namespace Lang;
using namespace std::string_literals;

namespace UpdaterUt {
constexpr auto LOCALE_FILE = "/data/updater/ui/locale";
constexpr std::array STR_FILES {"/data/updater/ui/l0.json", "/data/updater/ui/l1.json", "/data/updater/ui/l2.json"};
constexpr auto KEY_EMPTY_JSON = "/data/updater/ui/keyEmpty.json";
constexpr auto STR_NO_EXIST_JSON = "/data/updater/ui/strNotExist.json";
constexpr auto ALL_CORRECT_JSON = "/data/updater/ui/correct_all.json";
// only correct for one language
constexpr std::array CORRECT_JSONS {
    std::make_tuple(Language::CHINESE, "zh", "/data/updater/ui/correct_zh.json"),
    std::make_tuple(Language::SPANISH, "es", "/data/updater/ui/correct_es.json"),
    std::make_tuple(Language::ENGLISH, "en", "/data/updater/ui/correct_en.json")
};
constexpr std::array LANG_ARRAY { Language::CHINESE, Language::ENGLISH, Language::SPANISH };
constexpr std::array LANG_MAP {
    std::pair {Language::CHINESE, "zh"},
    std::pair {Language::ENGLISH, "en"},
    std::pair {Language::SPANISH, "es"}
};
constexpr auto GetStrForLang(Language language)
{
    for (auto [lang, localeStr] : LANG_MAP) {
        if (lang == language) {
            return localeStr;
        }
    }
    return "";
}
static_assert(GetStrForLang(Language::CHINESE), "zh");
static_assert(GetStrForLang(Language::ENGLISH), "en");
static_assert(GetStrForLang(Language::SPANISH), "es");

class UpdaterUiLangUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override
    {
        unlink(LOCALE_FILE);
    }
};

std::string GetTestJson(LangResource &langRes)
{
    std::stringstream ss;
    ss << R"({"locale":{"localeFile" : ")"s;
    ss << langRes.localeFile << R"(", "res" : [)";
    for (auto &res : langRes.res) {
        ss << R"({"path" : ")";
        ss << res.path;
        ss << R"(", "level" :)";
        ss << res.level;
        ss << R"(},)";
    }
    if (!langRes.res.empty()) {
        ss.seekp(-1, std::ios_base::end);
    }
    ss << "]}}";
    LOG(INFO) << ss.str();
    return ss.str();
}

HWTEST_F(UpdaterUiLangUnitTest, testLoadLangRes, TestSize.Level0)
{
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {}), false);
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{"invalidPath", -1}};

    // level invalid, too small
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);

    langRes.res = std::vector<Res> {{"invalidPath", 3}};
    // level invalid, too big
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);

    langRes.res = std::vector<Res> {{"invalidPath", 0}};
    // locale file not existed
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);
}

HWTEST_F(UpdaterUiLangUnitTest, testParseLanguage, TestSize.Level0)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{"invalidPath", 0}};
    // set langRes.localeFile
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);
    EXPECT_EQ(LanguageUI::GetInstance().ParseLanguage(), Language::CHINESE);

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "en"), true);
    EXPECT_EQ(LanguageUI::GetInstance().ParseLanguage(), Language::ENGLISH);

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "zh"), true);
    EXPECT_EQ(LanguageUI::GetInstance().ParseLanguage(), Language::CHINESE);

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "es"), true);
    EXPECT_EQ(LanguageUI::GetInstance().ParseLanguage(), Language::SPANISH);

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "invalid"), true);
    EXPECT_EQ(LanguageUI::GetInstance().ParseLanguage(), Language::CHINESE);
}

HWTEST_F(UpdaterUiLangUnitTest, testInit01, TestSize.Level0)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{"invalidPath", 0}};
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::CHINESE), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::ENGLISH), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::SPANISH), false);
}

HWTEST_F(UpdaterUiLangUnitTest, testInit02, TestSize.Level0)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{KEY_EMPTY_JSON, 0}};
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);
    EXPECT_EQ(OHOS::FileExists(KEY_EMPTY_JSON), true) << KEY_EMPTY_JSON << " not exist";
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::CHINESE), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::ENGLISH), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::SPANISH), false);
}

HWTEST_F(UpdaterUiLangUnitTest, testInit03, TestSize.Level0)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{STR_NO_EXIST_JSON, 0}};
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), false);
    EXPECT_EQ(OHOS::FileExists(STR_NO_EXIST_JSON), true) << STR_NO_EXIST_JSON << " not exist";
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::CHINESE), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::ENGLISH), false);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::SPANISH), false);
}

HWTEST_F(UpdaterUiLangUnitTest, testInit04, TestSize.Level0)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{ALL_CORRECT_JSON, 0}};
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(OHOS::FileExists(ALL_CORRECT_JSON), true) << ALL_CORRECT_JSON << " not exist";
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::CHINESE), true);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::ENGLISH), true);
    EXPECT_EQ(LanguageUI::GetInstance().Init(Language::SPANISH), true);

    for (auto [langType, locale, filePath] : CORRECT_JSONS) {
        langRes.res = std::vector<Res> {{filePath, 0}};
        EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, GetStrForLang(langType)), true);
        EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
        EXPECT_EQ(OHOS::FileExists(filePath), true) << filePath << " not exist";
        for (auto lang : LANG_ARRAY) {
            EXPECT_EQ(LanguageUI::GetInstance().Init(lang), lang == langType);
        }
    }
}

HWTEST_F(UpdaterUiLangUnitTest, testTranslate01, TestSize.Level0)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{STR_FILES[0], 0}};

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "en"), true);
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(LanguageUI::GetInstance().Translate("REBOOT_DEVICE"), "reboot devices");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SHUTDOWN_DEVICE"), "shutdown device");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("FACTORY_RESET"), "factory reset");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("NOT_EXIST_KEY"), "");

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "zh"), true);
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(LanguageUI::GetInstance().Translate("REBOOT_DEVICE"), "重启设备");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SHUTDOWN_DEVICE"), "关闭设备");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("FACTORY_RESET"), "清除用户数据");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("NOT_EXIST_KEY"), "");
}

HWTEST_F(UpdaterUiLangUnitTest, testTranslate02, TestSize.Level1)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{STR_FILES[0], 0}, {STR_FILES[1], 1}};

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "en"), true);
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(LanguageUI::GetInstance().Translate("REBOOT_DEVICE"), "reboot devices");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SHUTDOWN_DEVICE"), "shutdown device");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("FACTORY_RESET"), "factory reset");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SDCARD_UPDATE"), "update from sdcard");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("NOT_EXIST_KEY"), "default text");

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "zh"), true);
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(LanguageUI::GetInstance().Translate("REBOOT_DEVICE"), "重启设备");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SHUTDOWN_DEVICE"), "关闭设备");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("FACTORY_RESET"), "清除用户数据");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SDCARD_UPDATE"), "SD卡升级");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("NOT_EXIST_KEY"), "默认文字");
}

HWTEST_F(UpdaterUiLangUnitTest, testTranslate03, TestSize.Level1)
{
    LangResource langRes {};
    langRes.localeFile = LOCALE_FILE;
    langRes.res = std::vector<Res> {{STR_FILES[0], 0}, {STR_FILES[1], 1}, {STR_FILES[2], 2}};

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "en"), true);
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(LanguageUI::GetInstance().Translate("REBOOT_DEVICE"), "reboot devices");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SHUTDOWN_DEVICE"), "shutdown");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("FACTORY_RESET"), "factory reset");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SDCARD_UPDATE"), "sdcard update");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("NOT_EXIST_KEY"), "default");

    EXPECT_EQ(OHOS::SaveStringToFile(LOCALE_FILE, "zh"), true);
    EXPECT_EQ(LanguageUI::GetInstance().LoadLangRes(JsonNode {GetTestJson(langRes)}), true);
    EXPECT_EQ(LanguageUI::GetInstance().Translate("REBOOT_DEVICE"), "重启设备");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SHUTDOWN_DEVICE"), "关机");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("FACTORY_RESET"), "恢复出厂设置");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("SDCARD_UPDATE"), "SD卡升级");
    EXPECT_EQ(LanguageUI::GetInstance().Translate("NOT_EXIST_KEY"), "默认");
}
}
