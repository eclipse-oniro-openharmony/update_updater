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

#ifndef TRAITS_UTIL_H
#define TRAITS_UTIL_H

#include <string>
#include <vector>

namespace Updater {
namespace Detail {
template<typename T>
inline constexpr bool G_IS_NUM = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template<typename T>
inline constexpr bool G_IS_BOOL = std::is_same_v<bool, T>;

template<typename T>
inline constexpr bool G_IS_STR = (std::is_same_v<char *, T> || std::is_same_v<const char *, T> ||
                        std::is_same_v<std::string, T>);

template<typename T>
inline constexpr bool G_IS_PRINTABLE = (G_IS_NUM<T> || G_IS_BOOL<T> || G_IS_STR<T>);

template<typename T>
inline constexpr bool G_IS_BASE_TYPE = (G_IS_NUM<T> || G_IS_BOOL<T> || G_IS_STR<T>);

template<typename T>
struct IsVector : std::false_type {};

template<typename T>
struct IsVector<std::vector<T>> : std::true_type {};

template<typename T>
constexpr bool G_IS_VECTOR = IsVector<T>::value;

template<bool b, typename T>
using isMatch = typename std::enable_if_t<b, T>;

template<typename T>
struct StandardTypeHelper {
    static_assert(G_IS_BASE_TYPE<T>);
    using type = std::conditional_t<G_IS_NUM<T>, int, std::conditional_t<G_IS_STR<T>, std::string, bool>>;
};

template<typename T>
using StandardType = typename StandardTypeHelper<T>::type;

template<std::size_t idx, typename...T>
inline auto &Get(T &&...t)
{
    return std::get<idx>(std::forward_as_tuple(t...));
}
}
}
#endif
