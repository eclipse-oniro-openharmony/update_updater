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

#include "json_node.h"

#include "log/log.h"
#include "utils.h"

namespace Updater {
namespace {
inline const std::unordered_map<uint16_t, NodeType> &GetJsonTypeMap()
{
    static const std::unordered_map<uint16_t, NodeType> jsonTypeMap {
        { cJSON_Object, NodeType::OBJECT }, { cJSON_Array, NodeType::ARRAY }, { cJSON_Number, NodeType::INT },
        { cJSON_String, NodeType::STRING }, { cJSON_False, NodeType::BOOL },  { cJSON_True, NodeType::BOOL },
        { cJSON_NULL, NodeType::NUL },
    };
    return jsonTypeMap;
}
}  // namespace

JsonNode::JsonNode() : type_ {NodeType::UNKNOWN}
{
}

JsonNode::JsonNode(const std::string &str, bool needDelete) : JsonNode(cJSON_Parse(str.c_str()), needDelete)
{
}

JsonNode::JsonNode(const Fs::path &path)
{
    std::string realPath {};
    if (!Utils::PathToRealPath(path, realPath)) {
        return;
    }

    std::ifstream ifs(realPath);
    if (!ifs.is_open()) {
        LOG(ERROR) << realPath << " not exist";
        return;
    }

    // get root node
    std::string content {std::istreambuf_iterator<char> {ifs}, {}};
    cJSON *root = cJSON_Parse(content.c_str());
    if (root == nullptr || cJSON_IsInvalid(root)) {
        LOG(ERROR) << path << " contained json invalid";
        return;
    }

    Init(root, true);
}

JsonNode::JsonNode(const cJSON *root, bool needDelete)
{
    if (root != nullptr && !cJSON_IsInvalid(root)) {
        Init(root, needDelete);
        return;
    }
    LOG(ERROR) << "root is not valid";
    type_ = NodeType::UNKNOWN;
}


void JsonNode::Init(const cJSON *root, bool needDelete)
{
    if (auto it = GetJsonTypeMap().find(root->type); it != GetJsonTypeMap().end()) {
        type_ = it->second;
    }
    Parse(root);
    if (needDelete) {
        cJSON_Delete(const_cast<cJSON *>(root));
    }
}

void JsonNode::Parse(const cJSON *root)
{
    cJSON *element {};
    size_ = 1;
    switch (type_) {
        case NodeType::OBJECT: {
            innerObj_ = std::make_optional<NodeMap>();
            auto &optNodeMap = std::get<std::optional<NodeMap>>(innerObj_);
            cJSON_ArrayForEach(element, root)
            {
                std::unique_ptr<JsonNode> uPtr = std::make_unique<JsonNode>(element, false);
                innerNodesList_.push_back(*uPtr);
                (*optNodeMap).emplace(element->string, std::move(uPtr));
            }
            size_ = static_cast<int>(innerNodesList_.size());
            break;
        }
        case NodeType::ARRAY: {
            innerObj_ = std::make_optional<NodeVec>();
            auto &optNodeVec = std::get<std::optional<NodeVec>>(innerObj_);
            cJSON_ArrayForEach(element, root)
            {
                std::unique_ptr<JsonNode> uPtr = std::make_unique<JsonNode>(element, false);
                innerNodesList_.push_back(*uPtr);
                (*optNodeVec).push_back(std::move(uPtr));
            }
            size_ = static_cast<int>(innerNodesList_.size());
            break;
        }
        case NodeType::INT:
            innerObj_ = std::make_optional<int>(root->valueint);
            innerNodesList_.push_back(*this);
            break;
        case NodeType::STRING:
            innerObj_ = std::make_optional<std::string>(root->valuestring);
            innerNodesList_.push_back(*this);
            break;
        case NodeType::BOOL:
            innerObj_ = std::make_optional<bool>(root->type == cJSON_True);
            innerNodesList_.push_back(*this);
            break;
        case NodeType::NUL:
            break;
        default:
            LOG(ERROR) << "unknown node type";
            break;
    }
    if (root->string) {
        key_ = root->string;
    }
}

JsonNode::~JsonNode() = default;

const JsonNode &JsonNode::operator[](int idx) const
{
    auto optVec = std::get_if<std::optional<NodeVec>>(&innerObj_);
    if (optVec == nullptr || *optVec == std::nullopt) {
        return GetInvalidNode(); // type not matched
    }
    const NodeVec &nodeVec = **optVec;
    if (idx < 0 || idx >= size_) {
        return GetInvalidNode();
    }
    return *nodeVec[idx];
}

const JsonNode &JsonNode::operator[](const std::string &key) const
{
    auto optMap = std::get_if<std::optional<NodeMap>>(&innerObj_);
    if (optMap == nullptr || *optMap == std::nullopt) {
        return GetInvalidNode(); // type not matched
    }
    const NodeMap &nodeMap = **optMap;
    if (auto it = nodeMap.find(key); it != nodeMap.end()) {
        return *(it->second);
    }
    return GetInvalidNode();
}

std::list<std::reference_wrapper<JsonNode>>::const_iterator JsonNode::begin() const
{
    return innerNodesList_.cbegin();
}

std::list<std::reference_wrapper<JsonNode>>::const_iterator JsonNode::end() const
{
    return innerNodesList_.cend();
}
}  // namespace Updater