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

namespace Updater {
enum Action { SETVAL, PRINTVAL };

template<typename T>
struct Traits;

namespace Detail {
template<typename T, std::size_t idx>
using memberType = std::remove_reference_t<decltype(Traits<T>::template Get<idx>(std::declval<T&>()))>;

template<Action action>
struct MemberVisitor;

template<Action action>
struct StructVisitor {
    template<typename T, std::size_t F, std::size_t... R>
    static bool VisitStruct(const JsonNode &node, const JsonNode &defaultNode, T &t, std::index_sequence<F, R...>)
    {
        constexpr auto key = Traits<T>::MEMBER_KEY[F];
        if (!MemberVisitor<action>::template VisitMember<T, F>(node[key], defaultNode[key], t)) {
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
    template<typename T, std::size_t i>
    static auto VisitMember(const JsonNode &node, const JsonNode &defaultNode, T &obj)
        -> Detail::isMatch<Detail::G_IS_BASE_TYPE<memberType<T, i>>, bool>
    {
        auto r = node.As<memberType<T, i>>();
        auto defaultR = defaultNode.As<memberType<T, i>>();
        if (!r.has_value() && !defaultR.has_value()) {
            LOG(ERROR) << Traits<T>::MEMBER_KEY[i] << " has not both default and non-default value!!!";
            return false;
        }
        if (r) {
            Traits<T>::template Get<i>(obj) = *r;
            return true;
        }
        if (defaultR) {
            Traits<T>::template Get<i>(obj) = *defaultR;
            return true;
        }
        return false;
    }
    // visit struct
    template<typename T, std::size_t i>
    static auto VisitMember(const JsonNode &node, const JsonNode &defaultNode, T &obj)
        -> Detail::isMatch<std::is_integral_v<decltype(Traits<memberType<T, i>>::COUNT)>, bool>
    {
        return StructVisitor<SETVAL>::VisitStruct(node, defaultNode, Traits<T>::template Get<i>(obj),
            std::make_index_sequence<Traits<memberType<T, i>>::COUNT> {});
    }
};
}  // namespace Detail

template<Action act, typename T>
auto Visit(const JsonNode &node, const JsonNode &defaultNode, T &obj)
    -> Detail::isMatch<Detail::G_IS_NUM<decltype(Traits<T>::COUNT)>, bool>
{
    static_assert(act == SETVAL,
        "Only for setting member of struct with default node!");
    return Detail::StructVisitor<act>::VisitStruct(node, defaultNode, obj,
                                                   std::make_index_sequence<Traits<T>::COUNT> {});
}

template<Action act, typename T>
auto Visit(const JsonNode &node, T &obj) -> Detail::isMatch<Detail::G_IS_NUM<decltype(Traits<T>::COUNT)>, bool>
{
    static_assert(act == SETVAL,
        "Only for setting member of struct without default node!");
    static JsonNode dummyNode {};
    return Detail::StructVisitor<act>::VisitStruct(node, {}, obj,
                                                   std::make_index_sequence<Traits<T>::COUNT> {});
}
}  // namespace updater

#endif
