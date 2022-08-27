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

#include "layout_parser.h"
#include "components/ui_view.h"
#include "json_visitor.h"
#include "log/log.h"

namespace Updater {
namespace {
constexpr auto DEFAULT_MODULE = "default";
constexpr auto COMPONENT_MODULE = "coms";
constexpr auto COMMON_LABEL = "Common";
constexpr auto COMMON_TYPE = "type";

inline const auto &GetSpecificInfoMap()
{
    using namespace OHOS;
    const static std::unordered_map<std::string, std::function<SpecificInfo()>> specificInfoMap {
        {VIEW_TYPE_STRING[UI_IMAGE_VIEW], [] () { return UxImageInfo {}; }},
        {VIEW_TYPE_STRING[UI_BOX_PROGRESS], [] () { return UxBoxProgressInfo {}; }},
        {VIEW_TYPE_STRING[UI_LABEL], [] () { return UxLabelInfo {}; }},
        {VIEW_TYPE_STRING[UI_LABEL_BUTTON], [] () { return UxLabelBtnInfo {}; }}
    };
    return specificInfoMap;
}
}

class LayoutParser::Impl {
    using cJSONPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
public:
    bool LoadLayout(const std::vector<std::string> &layoutFiles, std::vector<UxPageInfo> &vec) const
    {
        std::vector<UxPageInfo>().swap(vec);
        UxPageInfo pageInfo = {};
        for (const auto &file : layoutFiles) {
            if (!LoadLayout(file, pageInfo)) {
                LOG(ERROR) << file << " load failed";
                return false;
            }
            vec.push_back(std::move(pageInfo));
            pageInfo = {};
        }
        return true;
    }
    bool LoadLayout(const std::string &filename, UxPageInfo &pageInfo) const
    {
        JsonNode node {std::filesystem::path {filename}};

        // parse color, id, subpages
        if (!Visit<SETVAL>(node, pageInfo)) {
            LOG(ERROR) << "get page info (id, color, subpages) failed";
            return false;
        }

        // parse view info
        if (!ParseViewInfo(node, pageInfo.viewInfos)) {
            LOG(ERROR) << "component Node parse failed";
            return false;
        }

        return true;
    }
private:
    bool ParseViewInfo(const JsonNode &root, std::vector<UxViewInfo> &vec) const
    {
        UxViewInfo info {};
        std::vector<UxViewInfo>().swap(vec);
        const JsonNode &defaultNode = root[DEFAULT_MODULE];
        const JsonNode &componentNodes = root[COMPONENT_MODULE];
        if (componentNodes.Type() != NodeType::ARRAY) {
            LOG(ERROR) << "Please check whether json file has a coms field";
            return false;
        }
        for (const auto &componentNode : componentNodes) {
            const JsonNode &comNode = componentNode.get();
            auto viewType = comNode[COMMON_TYPE].As<std::string>();
            if (viewType == std::nullopt) {
                LOG(ERROR) << "Component don't have a type field";
                return false;
            }
            const JsonNode &commonDefault = defaultNode[COMMON_LABEL];
            if (!Visit<SETVAL>(componentNode, commonDefault, info.commonInfo)) {
                LOG(ERROR) << "set common info failed";
                return false;
            }

            auto it = GetSpecificInfoMap().find(*viewType);
            if (it == GetSpecificInfoMap().end()) {
                LOG(ERROR) << "Can't recognize this type " << *viewType;
                return false;
            }
            info.specificInfo = it->second();
            auto visitor = [&comNode, &defaultNode] (auto &args) {
                const JsonNode &defaultComNode = defaultNode[Traits<std::decay_t<decltype(args)>>::STRUCT_KEY];
                return Visit<SETVAL>(comNode, defaultComNode, args);
            };
            if (!std::visit(visitor, info.specificInfo)) {
                return false;
            }
            vec.push_back(std::move(info));
            info = {};
        }
        return true;
    }
};

LayoutParser::~LayoutParser() = default;

LayoutParser::LayoutParser() : pImpl_(std::make_unique<Impl>()) { }

LayoutParser &LayoutParser::GetInstance()
{
    static LayoutParser layoutParser;
    return layoutParser;
}

bool LayoutParser::LoadLayout(const std::string &layoutFile, UxPageInfo &pageInfo) const
{
    return pImpl_->LoadLayout(layoutFile, pageInfo);
}

bool LayoutParser::LoadLayout(const std::vector<std::string> &layoutFiles, std::vector<UxPageInfo> &vec) const
{
    return pImpl_->LoadLayout(layoutFiles, vec);
}
}  // namespace Updater
