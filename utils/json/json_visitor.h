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

#ifndef JSON_VISITOR_H
#define JSON_VISITOR_H

#include <type_traits>

#include "json_node.h"
#include "log/log.h"
#include "updater_ui_traits.h"

namespace Updater {
enum Action { SETVAL, PRINTVAL };
namespace Detail {
template<typename Traits>
constexpr bool CheckTrait()
{
    bool res = true;
    for (int i = 0; i < Traits::COUNT; ++i) {
        res = res && Traits::MEMBER_KEY[i] != nullptr && std::string_view("") != Traits::MEMBER_KEY[i];
    }
    return res;
};

template<Action action>
struct MemberVisitor;

template<typename T, std::size_t idx>
using memberType = std::remove_reference_t<decltype(Traits<T>::template Get<idx>(std::declval<T&>()))>;

template<Action action>
struct StructVisitor {
    template<typename T, std::size_t F, std::size_t... R>
    static bool VisitStruct(const JsonNode &node, const JsonNode &defaultNode, T &t, std::index_sequence<F, R...>)
    {
        static_assert(CheckTrait<Traits<T>>(), "Trait member key invalid, please check");
        constexpr auto key = Traits<T>::MEMBER_KEY[F];
        auto &FthMember = Traits<T>::template Get<F>(t);
        if (!MemberVisitor<action>::template VisitMember<memberType<T, F>>(
            node[key], defaultNode[key], FthMember, key)) {
            return false;
        }

        if constexpr (sizeof...(R) == 0) {
            return true;
        } else { // don't remove else, otherwise can't compile
            return VisitStruct(node, defaultNode, t, std::index_sequence<R...> {});
        }
    }
};

template<>
struct MemberVisitor<SETVAL> {
    // visit string, int, bool
    template<typename T, std::enable_if_t<Detail::G_IS_BASE_TYPE<T>, bool> = true>
    static bool VisitMember(const JsonNode &node, const JsonNode &defaultNode, T &obj, const char *key)
    {
        auto r = node.As<T>();
        auto defaultR = defaultNode.As<T>();
        if (!r.has_value() && !defaultR.has_value()) {
            LOG(ERROR) << key << " has not both default and non-default value!!!";
            return false;
        }
        if (r) {
            obj = *r;
            return true;
        }
        if (defaultR) {
            obj = *defaultR;
            return true;
        }
        return false;
    }
    // visit vector, T is std::vector<T::value_type>
    template<typename T, std::enable_if_t<Detail::G_IS_VECTOR<T>, bool> = true>
    static auto VisitMember(const JsonNode &node, const JsonNode &defaultNode, T &obj, const char *key)
    {
        if ((node.Type() != NodeType::UNKNOWN && node.Type() != NodeType::NUL && node.Type() != NodeType::ARRAY) ||
            (defaultNode.Type() != NodeType::UNKNOWN && defaultNode.Type() != NodeType::NUL &&
            defaultNode.Type() != NodeType::ARRAY)) {
                LOG(ERROR) << "Node type not matched with " << key;
                return false;
        }
        typename T::value_type ele {};
        for (auto &subNode : node) {
            ele = {};
            if (!VisitMember(subNode, {}, ele, "")) {
                return false;
            }
            obj.push_back(std::move(ele));
        }
        for (auto &subNode : defaultNode) {
            ele = {};
            // using subNode to contruct an element of i-th type of T
            if (!VisitMember(subNode, {}, ele, "")) {
                return false;
            }
            obj.push_back(std::move(ele));
        }
        return true;
    }
    // visit struct
    template<typename T, std::enable_if_t<std::is_integral_v<decltype(Traits<T>::COUNT)>, bool> = true>
    static bool VisitMember(const JsonNode &node, const JsonNode &defaultNode, T &obj, const char *key)
    {
        return StructVisitor<SETVAL>::VisitStruct(node, defaultNode, obj,
            std::make_index_sequence<Traits<T>::COUNT> {});
    }
};
}  // namespace Detail

template<Action act, typename T, std::enable_if_t<Detail::G_IS_NUM<decltype(Traits<T>::COUNT)>, bool> = true>
bool Visit(const JsonNode &node, const JsonNode &defaultNode, T &obj)
{
    static_assert(act == SETVAL,
        "Only for setting member of struct with default node!");
    return Detail::StructVisitor<act>::VisitStruct(node, defaultNode, obj,
                                                   std::make_index_sequence<Traits<T>::COUNT> {});
}

template<Action act, typename T, std::enable_if_t<Detail::G_IS_NUM<decltype(Traits<T>::COUNT)>, bool> = true>
bool Visit(const JsonNode &node, T &obj)
{
    static_assert(act == SETVAL,
        "Only for setting member of struct without default node!");
    return Detail::StructVisitor<act>::VisitStruct(node, {}, obj,
                                                   std::make_index_sequence<Traits<T>::COUNT> {});
}
}  // namespace Updater

#endif
