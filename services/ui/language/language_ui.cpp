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
#include "language_ui.h"

#include "json_node.h"
#include "log/log.h"
#include "utils.h"

namespace Updater {
namespace Lang {
constexpr int MIN_LVL = 0; // 0 : min resource level
constexpr int MAX_LVL = 2; // 2 : max resource level
constexpr const char *DEFAULT_KEY = "DEFAULT_STRING";

// map value zh/en/spa is used in string.json to specify language type for each string key
std::unordered_map<Language, std::string> g_languageDataVars = {
    {Language::CHINESE, "zh"},
    {Language::ENGLISH, "en"},
    {Language::SPANISH, "spa"},
};

// map key zh/es/en is used in locale file to specify locale env for updater
const std::unordered_map<std::string, Language> LanguageUI::LOCALES {
    {"zh", Language::CHINESE},
    {"en", Language::ENGLISH},
    {"es", Language::SPANISH}
};

LanguageUI::LanguageUI() : strMap_ {}, res_ {}, langRes_ {}, language_ {Language::ENGLISH}
{
    res_.resize(MAX_LVL + 1);
}

LanguageUI &LanguageUI::GetInstance()
{
    static LanguageUI instance;
    return instance;
}

bool LanguageUI::Init(Language language)
{
    language_ = language;
    if (!Parse()) {
        LOG(ERROR) << "parse language resources failed";
        return false;
    }
    return true;
}

bool LanguageUI::SetRes(const Res &res)
{
    if (!CheckLevel(res.level)) {
        return false;
    }
    res_[res.level] = res.path;
    return true;
}

bool LanguageUI::Parse()
{
    strMap_.clear();
    for (auto &file : res_) {
        if (file.empty()) {
            LOG(WARNING) << "file name empty";
            continue;
        }
        if (!ParseJson(file)) {
            LOG(ERROR) << "parse file " << file << " error";
            return false;
        }
    }
    return true;
}

bool LanguageUI::ParseJson(const std::string &file)
{
    JsonNode root {std::filesystem::path { file }};
    /*
     * an example:
     *	{
     *      "LABEL_REBOOT_DEVICE": {
     *            "zh" : "",
     *            "en" : "",
     *            "spa" : ""
     *      }
     *  }
     *  , this is an object node
     */
    if (root.Type() != NodeType::OBJECT) {
        LOG(ERROR) << file << " is invalid, nodetype is " << static_cast<int>(root.Type());
        return false;
    }
    for (auto &node : root) {
        const JsonNode &strNode = node.get();
        std::string key = strNode.Key().value_or("");
        if (key.empty()) {
            LOG(ERROR) << "key is empty";
            return false;
        }
        if (auto optionalStr = strNode[g_languageDataVars[language_]].As<std::string>();
            optionalStr != std::nullopt) {
            strMap_[key] = *optionalStr;
            continue;
        }
        LOG(ERROR) << "dont have a " << g_languageDataVars[language_] << " string";
        return false;
    }
    return true;
}

bool LanguageUI::CheckLevel(int level)
{
    if (level < MIN_LVL || level > MAX_LVL) {
        LOG(ERROR) << "level invalid : " << level;
        return false;
    }
    return true;
}

const std::string &LanguageUI::Translate(const std::string &key, UpdaterUiMsgID msgId) const
{
    static std::string emptyStr;
    if (auto it = strMap_.find(key); it != strMap_.end() && !it->second.empty()) {
        return it->second;
    }
    if (auto it = strMap_.find(DEFAULT_KEY); it != strMap_.end()) {
        return it->second;
    }
    return emptyStr;
}

bool LanguageUI::LoadLangRes(const JsonNode &node)
{
    langRes_ = {};
    if (!Visit<SETVAL>(node[LANG_RES_KEY], langRes_)) {
        LOG(ERROR) << "parse language res error";
        return false;
    }
    // clear resources
    std::vector<std::string>{3, ""}.swap(res_);
    // load resources
    for (auto &res : langRes_.res) {
        if (!SetRes(res)) {
            return false;
        }
    }
    if (!Init(ParseLanguage())) {
        LOG(ERROR) << "init failed";
        return false;
    }
    LOG(INFO) << "load language resource success";
    return true;
}

Language LanguageUI::ParseLanguage() const
{
    constexpr Language DEFAULT_LOCALE = Language::CHINESE;
    constexpr size_t localeLen = 2; // zh|es|en
    std::string realPath {};
    if (!Utils::PathToRealPath(langRes_.localeFile, realPath)) {
        LOG(ERROR) << "get real path failed";
        return DEFAULT_LOCALE;
    }

    std::ifstream ifs(realPath);
    std::string content {std::istreambuf_iterator<char> {ifs}, {}};
    const std::string &locale = content.substr(0, localeLen);
    if (auto it = LOCALES.find(locale); it != LOCALES.end()) {
        return it->second;
    }
    LOG(ERROR) << "locale " << locale << " not recognized";
    return DEFAULT_LOCALE;
}
}
} // namespace Updater
