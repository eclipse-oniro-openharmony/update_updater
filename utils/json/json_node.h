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
#include "macros.h"
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
private:
    void Parse(const cJSON *root);
    void Init(const cJSON *root, bool needDelete);
    template<typename T>
    void Assign(T rhs) const
    {
        if (innerObj_.valueless_by_exception()) {
            innerObj_ = std::optional<T>(rhs);
        }
        if (auto optPtr = std::get_if<std::optional<T>>(&innerObj_); optPtr) {
            *optPtr = std::optional<T>(rhs);
        }
    }
    int size_ {1};
    NodeType type_ {NodeType::UNKNOWN};         /* json node type */
    std::optional<std::string> key_ {std::nullopt}; /* key for object items */
    optionalVariant<bool, int, std::string, NodeVec, NodeMap> innerObj_ {};
    std::list<std::reference_wrapper<JsonNode>> innerNodesList_ {};
};

inline const JsonNode &GetInvalidNode()
{
    static JsonNode emptyNode;  // used for invalid json node
    return emptyNode;
}
}
#endif // NODE_H
