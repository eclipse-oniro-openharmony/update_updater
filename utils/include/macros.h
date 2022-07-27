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

#ifndef MACRO_H
#define MACRO_H

#define DISALLOW_COPY_MOVE_ASSIGN(ClassName)            \
    ClassName &operator = (const ClassName &) = delete; \
    ClassName &operator = (ClassName &&) = delete

#define DISALLOW_COPY_MOVE_CONSTRUCT(ClassName) \
    ClassName(const ClassName &) = delete;      \
    ClassName(ClassName &&) = delete

#define DISALLOW_COPY_MOVE(ClassName)     \
    DISALLOW_COPY_MOVE_ASSIGN(ClassName); \
    DISALLOW_COPY_MOVE_CONSTRUCT(ClassName)

#define STRINGFY_WRAPPER(x) #x
#define STRINGFY(x) STRINGFY_WRAPPER(x)

#define SIZE(...) SIZE_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define SIZE_IMPL(e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, N, ...) N

#define CONCAT(x, y) CONCAT_WRAPPER(x, y)
#define CONCAT_WRAPPER(x, y) x##y

#define GET_TYPE_2(e1, e2) e1
#define GET_TYPE_3(e1, e2, e3) e1, e2
#define GET_TYPE_4(e1, e2, e3, e4) e1, e2, e3

#define GET_NAME_2(e1, e2) e2
#define GET_NAME_3(e1, e2, e3) e3
#define GET_NAME_4(e1, e2, e3, e4) e4

#define GET_TYPE(...) CONCAT(GET_TYPE_, SIZE(__VA_ARGS__))(__VA_ARGS__)
#define GET_NAME(...) CONCAT(GET_NAME_, SIZE(__VA_ARGS__))(__VA_ARGS__)
#define GET_KEY(...) STRINGFY(GET_NAME(__VA_ARGS__))

#define REPEAT(MACRO, ...) CONCAT(REPEAT_, SIZE(__VA_ARGS__))(MACRO, __VA_ARGS__)
#define REPEAT_1(MACRO, element, ...) MACRO(0, element)
#define REPEAT_2(MACRO, element, ...) MACRO(1, element) REPEAT_1(MACRO, __VA_ARGS__)
#define REPEAT_3(MACRO, element, ...) MACRO(2, element) REPEAT_2(MACRO, __VA_ARGS__)
#define REPEAT_4(MACRO, element, ...) MACRO(3, element) REPEAT_3(MACRO, __VA_ARGS__)
#define REPEAT_5(MACRO, element, ...) MACRO(4, element) REPEAT_4(MACRO, __VA_ARGS__)
#define REPEAT_6(MACRO, element, ...) MACRO(5, element) REPEAT_5(MACRO, __VA_ARGS__)
#define REPEAT_7(MACRO, element, ...) MACRO(6, element) REPEAT_6(MACRO, __VA_ARGS__)
#define REPEAT_8(MACRO, element, ...) MACRO(7, element) REPEAT_7(MACRO, __VA_ARGS__)
#define REPEAT_9(MACRO, element, ...) MACRO(8, element) REPEAT_8(MACRO, __VA_ARGS__)
#define REPEAT_10(MACRO, element, ...) MACRO(9, element) REPEAT_9(MACRO, __VA_ARGS__)

#define COMMA_IF(i) CONCAT(COMMA_IF_, i)
#define COMMA_IF_0
#define COMMA_IF_1 ,
#define COMMA_IF_2 ,
#define COMMA_IF_3 ,
#define COMMA_IF_4 ,
#define COMMA_IF_5 ,
#define COMMA_IF_6 ,
#define COMMA_IF_7 ,
#define COMMA_IF_8 ,
#define COMMA_IF_9 ,

#define GET_MEMBER_DECLARTION(idx, tuple) GET_TYPE tuple GET_NAME tuple;
#define GET_MEMBER_KEY(idx, tuple) GET_KEY tuple COMMA_IF(idx)
#define GET_MEMBER(idx, tuple) obj.GET_NAME tuple COMMA_IF(idx)

#define DEFINE_STRUCT_TRAIT(NAME, KEY, ...) \
    DEFINE_STRUCT(NAME, __VA_ARGS__);       \
    DEFINE_TRAIT(NAME, KEY, __VA_ARGS__)

#define DEFINE_STRUCT(NAME, ...)                   \
    struct NAME {                                  \
        REPEAT(GET_MEMBER_DECLARTION, __VA_ARGS__) \
    }

#define DEFINE_TRAIT(NAME, KEY, ...)                                                       \
    template <> struct Traits<NAME> {                                                      \
        constexpr static const char *STRUCT_KEY = KEY;                                     \
        constexpr static const char *MEMBER_KEY[] = {REPEAT(GET_MEMBER_KEY, __VA_ARGS__)}; \
        constexpr static int COUNT = sizeof(MEMBER_KEY) / sizeof(char *);                  \
        template <std::size_t idx> constexpr static auto &Get(NAME &obj)                   \
        {                                                                                  \
            return Detail::Get<idx>(REPEAT(GET_MEMBER, __VA_ARGS__));                      \
        }                                                                                  \
    }
#endif