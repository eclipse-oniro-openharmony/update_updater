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

#ifndef JSON_NODE_H
#define JSON_NODE_H

#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "cJSON.h"
#include "log/log.h"
#include "macros_updater.h"
#include "traits_util.h"

namespace Updater {
class JsonNode;

enum class NodeType { OBJECT, INT, STRING, ARRAY, BOOL, NUL, UNKNOWN };

using NodeMap = std::unordered_map<std::string, std::unique_ptr<JsonNode>>;
using NodeVec = std::vector<std::unique_ptr<JsonNode>>;
using cJSONPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
template<typename...T>
using optionalVariant = std::variant<std::optional<T> ...>;

namespace Fs = std::filesystem;
class JsonNode {
    DISALLOW_COPY_MOVE(JsonNode);
public:
    JsonNode();
    explicit JsonNode(const Fs::path &path);
    explicit JsonNode(const std::string &str, bool needDelete = true);
    explicit JsonNode(const cJSON *root, bool needDelete = true);
    ~JsonNode();

    const JsonNode &operator[](int idx) const;
    const JsonNode &operator[](const std::string &key) const;
    JsonNode &operator[](int idx);
    JsonNode &operator[](const std::string &key);

    template<typename T>
    std::optional<T> As() const
    {
        if (auto optPtr = std::get_if<std::optional<Detail::StandardType<T>>>(&innerObj_); optPtr) {
            return *optPtr;
        }
        return std::nullopt;
    }

    template<typename T>
    bool operator==(T rhs) const
    {
        if (auto optPtr = std::get_if<std::optional<Detail::StandardType<T>>>(&innerObj_); optPtr) {
            return *optPtr == rhs;
        }
        return false;
    }

    int Size() const
    {
        return size_;
    }
    NodeType Type() const
    {
        return type_;
    }
    std::optional<std::string> Key() const
    {
        return key_;
    }
    std::list<std::reference_wrapper<JsonNode>>::const_iterator begin() const;
    std::list<std::reference_wrapper<JsonNode>>::const_iterator end() const;
    template<typename T>
    void operator=(T &&rhs)
    {
        static_assert(Detail::G_IS_BASE_TYPE<Detail::RemoveCvRef<T>>, "only allow change int, string, bool value");
        if (innerObj_.valueless_by_exception()) {
            innerObj_ = Detail::OptStandardType<T>(rhs);
        }
        if (auto optPtr = std::get_if<Detail::OptStandardType<T>>(&innerObj_); optPtr) {
            *optPtr = Detail::OptStandardType<T>(rhs);
        } else {
            LOG(ERROR) << "assign json node failed, key is " << key_.value_or("null") << ", type is "
                << static_cast<int>(type_) << ", rhs is " << rhs;
        }
    }
private:
    void Parse(const cJSON *root);
    void Init(const cJSON *root, bool needDelete);
    int size_ {1};
    NodeType type_ {NodeType::UNKNOWN};         /* json node type */
    std::optional<std::string> key_ {std::nullopt}; /* key for object items */
    optionalVariant<bool, int, std::string, NodeVec, NodeMap> innerObj_ {};
    std::list<std::reference_wrapper<JsonNode>> innerNodesList_ {};
};

inline JsonNode &GetInvalidNode()
{
    static JsonNode emptyNode;  // used for invalid json node
    return emptyNode;
}

template<typename T>
inline JsonNode &GetNodeByIdx(T &innerObj, int size, int idx)
{
    auto optVec = std::get_if<std::optional<NodeVec>>(&innerObj);
    if (optVec == nullptr || *optVec == std::nullopt) {
        return GetInvalidNode(); // type not matched
    }
    auto &nodeVec = **optVec;
    if (idx < 0 || idx >= size) {
        return GetInvalidNode();
    }
    return *nodeVec[idx];
}

template<typename T>
inline JsonNode &GetNodeByKey(T &innerObj, const std::string &key)
{
    auto optMap = std::get_if<std::optional<NodeMap>>(&innerObj);
    if (optMap == nullptr || *optMap == std::nullopt) {
        return GetInvalidNode(); // type not matched
    }
    auto &nodeMap = **optMap;
    if (auto it = nodeMap.find(key); it != nodeMap.end()) {
        return *(it->second);
    }
    return GetInvalidNode();
}
}
#endif // NODE_H
