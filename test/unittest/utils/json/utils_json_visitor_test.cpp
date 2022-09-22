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

#include "utils_json_visitor_test.h"

#include "json_visitor.h"
#include "macros.h"

namespace Updater {
DEFINE_STRUCT_TRAIT(D, "D",
    (int, d1),
    (std::string, d2),
    (bool, d3)
);

DEFINE_STRUCT_TRAIT(Color, "Color",
    (int, r),
    (std::string, g),
    (bool, b)
);

DEFINE_STRUCT_TRAIT(E, "E",
    (int, d1),
    (Color, d2)
);

DEFINE_STRUCT_TRAIT(F, "F",
    (int, d1),
    (std::string, d2)
);

DEFINE_STRUCT_TRAIT(G, "G",
    (int, d1),
    (bool, d2),
    (F, d3)
);

DEFINE_STRUCT_TRAIT(H, "H",
    (int, d1),
    (bool, d2),
    (G, d3)
);

DEFINE_STRUCT_TRAIT(I, "I",
    (std::vector<std::string>, d1)
);

DEFINE_STRUCT_TRAIT(J, "J",
    (std::vector<std::vector<std::string>>, d1)
);

DEFINE_STRUCT_TRAIT(K, "K",
    (int, d1),
    (std::string, d2)
);

DEFINE_STRUCT_TRAIT(L, "L",
    (std::vector<int>, d1)
);
}

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
using PairType = std::pair<std::string_view, std::string_view>;
template<typename T, std::size_t N>
void TestInvalidCases(T &obj, const std::array<PairType, N> &replaceData, const std::string &jsonStr)
{
    for (auto data : replaceData) {
        auto pos = jsonStr.find(data.first);
        ASSERT_NE(pos, std::string::npos) << data.first;
        {
            // make type not matched
            std::string newJsonStr = jsonStr;
            newJsonStr.replace(pos, data.first.size(), data.second.data(), data.second.size());
            JsonNode node {newJsonStr};
            EXPECT_EQ(false, Visit<SETVAL>(node[Traits<T>::STRUCT_KEY], obj)) << data.first;
            EXPECT_EQ(false, Visit<SETVAL>({}, node[Traits<T>::STRUCT_KEY], obj)) << data.first;
        }
        {
            // make field not existed
            std::string newJsonStr = jsonStr;
            newJsonStr.replace(pos, data.first.size(), "");
            JsonNode node {newJsonStr};
            EXPECT_EQ(false, Visit<SETVAL>(node[Traits<T>::STRUCT_KEY], obj)) << data.first;
            EXPECT_EQ(false, Visit<SETVAL>({}, node[Traits<T>::STRUCT_KEY], obj)) << data.first;
        }
    }
}
}

namespace UpdaterUt {
// do something at the each function begining
void UtilsJsonVisitorUnitTest::SetUp(void)
{
    cout << "Updater Unit UtilsJsonVisitorUnitTest Begin!" << endl;
}

// do something at the each function end
void UtilsJsonVisitorUnitTest::TearDown(void)
{
    cout << "Updater Unit UtilsJsonVisitorUnitTest End!" << endl;
}

// init
void UtilsJsonVisitorUnitTest::SetUpTestCase(void)
{
    cout << "SetUpTestCase" << endl;
}

// end
void UtilsJsonVisitorUnitTest::TearDownTestCase(void)
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(UtilsJsonVisitorUnitTest, testD, TestSize.Level0)
{
    std::string dJson = R"({
        "D": {
            "d1":1,
            "d2":"true",
            "d3":true
        }
    })";
    JsonNode node {dJson};
    D d {};
    EXPECT_EQ(Visit<SETVAL>(node["D"], d), true);
    EXPECT_EQ(d.d1, 1);
    EXPECT_EQ(d.d2, "true");
    EXPECT_EQ(d.d3, true);

    constexpr std::array replaceData {
        PairType { R"("d1":1)", R"("d1":"1")" }, PairType { R"("d2":"true")", R"("d2":true)" },
        PairType { R"("d3":true)", R"("d3":"true")" }
    };
    TestInvalidCases(d, replaceData, dJson);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testE, TestSize.Level0)
{
    std::string eJson = R"({
        "E": {
            "d1":1,
            "d2": {
                "r":1,
                "g":"foo",
                "b":true
            }
        }
    })";
    E e {};
    JsonNode node {eJson};
    EXPECT_EQ(Visit<SETVAL>(node["E"], e), true);
    EXPECT_EQ(e.d1, 1);
    EXPECT_EQ(e.d2.r, 1);
    EXPECT_EQ(e.d2.g, "foo");
    EXPECT_EQ(e.d2.b, true);

    constexpr std::array replaceData {
        PairType { R"("d1":1)", R"("d1":"1")" }, PairType { R"("r":1)", R"("r":"1")" },
        PairType { R"("g":"foo")", R"("g":1)" }, PairType { R"("b":true)", R"("b":"true")" },
        PairType { R"("d2": {
                "r":1,
                "g":"foo",
                "b":true
            })", R"("d2":2)"},
    };
    TestInvalidCases(e, replaceData, eJson);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testH, TestSize.Level0)
{
    std::string hJson = R"({
        "H": {
            "d1":1,
            "d2":true,
            "d3": {
                "d1":2,
                "d2":false,
                "d3": {
                    "d1":3,
                    "d2":"foo"
                }
            }
        }
    })";
    H h {};
    JsonNode node {hJson};
    EXPECT_EQ(Visit<SETVAL>(node["H"], h), true);
    EXPECT_EQ(h.d1, 1);
    EXPECT_EQ(h.d2, true);
    EXPECT_EQ(h.d3.d1, 2);
    EXPECT_EQ(h.d3.d2, false);
    EXPECT_EQ(h.d3.d3.d1, 3);
    EXPECT_EQ(h.d3.d3.d2, "foo");

    constexpr std::array replaceData {
        PairType { R"("d1":1)", R"("d1":"1")" }, PairType { R"("d1":2)", R"("d1":"2")" },
        PairType { R"("d1":3)", R"("d1":"3")" }, PairType { R"("d2":true)", R"("d2":"true")" },
        PairType { R"("d2":false)", R"("d2":"false")" }, PairType { R"("d2":"foo")", R"("d2":1)" },
        PairType { R"("d3": {
                "d1":2,
                "d2":false,
                "d3": {
                    "d1":3,
                    "d2":"foo"
                }
            })", R"("d3":1)"},
        PairType { R"("d3": {
                    "d1":3,
                    "d2":"foo"
                })", R"("d3":2)"}
    };
    TestInvalidCases(h, replaceData, hJson);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testI, TestSize.Level0)
{
    std::string iJson = R"({ "I" : { "d1": [ "foo", "bar", "baz" ] } })";
    I i {};
    JsonNode node {iJson};
    EXPECT_EQ(Visit<SETVAL>(node["I"], i), true);
    EXPECT_EQ(i.d1, std::vector<std::string>({"foo", "bar", "baz"}));
    i = {};
    EXPECT_EQ(Visit<SETVAL>({}, node["I"], i), true);
    EXPECT_EQ(i.d1, std::vector<std::string>({"foo", "bar", "baz"}));
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({"I" : { "d1" : 1 }})"s} ["I"], i), false);
    EXPECT_EQ(Visit<SETVAL>({}, JsonNode {R"({"I" : { "d1" : 1 }})"s} ["I"], i), false);
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({ "I" : { "d1": "foo" } })"s} ["I"], i), false);
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({ "I" : { "d1": [ 1 ] } })"s} ["I"], i), false);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testJ, TestSize.Level0)
{
    J j {};
    JsonNode node {R"({"J" : {"d1" : [["foo","bar","baz"],["foo1","bar1","baz1"]]}})"s};
    EXPECT_EQ(Visit<SETVAL>(node["J"], j), true);
    ASSERT_EQ(j.d1.size(), 2);
    EXPECT_EQ(j.d1[0], std::vector<std::string>({"foo", "bar", "baz"}));
    EXPECT_EQ(j.d1[1], std::vector<std::string>({"foo1", "bar1", "baz1"}));

    j = {};
    EXPECT_EQ(Visit<SETVAL>({}, node["J"], j), true);
    ASSERT_EQ(j.d1.size(), 2);
    EXPECT_EQ(j.d1[0], std::vector<std::string>({"foo", "bar", "baz"}));
    EXPECT_EQ(j.d1[1], std::vector<std::string>({"foo1", "bar1", "baz1"}));

    j = {};
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({"J" : { "d1" : 1 }})"s} ["J"], j), false);
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({"J" : { "d1" : [1] }})"s} ["J"], j), false);
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({"J" : { "d1" : [[1]] }})"s} ["J"], j), false);
    EXPECT_EQ(Visit<SETVAL>({}, JsonNode {R"({"J" : { "d1" : 1 }})"s} ["J"], j), false);
    EXPECT_EQ(Visit<SETVAL>({}, JsonNode {R"({"J" : { "d1" : [1] }})"s} ["J"], j), false);
    EXPECT_EQ(Visit<SETVAL>({}, JsonNode {R"({"J" : { "d1" : [[1]] }})"s} ["J"], j), false);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testInvalidK, TestSize.Level0)
{
    K k {};
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({ "K" : { "d1" : 1 } })"s} ["K"], k), false);
    EXPECT_EQ(Visit<SETVAL>(JsonNode {R"({ "K" : { "d1" : "1" } })"s} ["K"], k), false);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testNoDefaultAndNonDefaultK, TestSize.Level0)
{
    std::string kJson = R"({
        "K" : { "d1" : 1 },
        "KNonDefault0" : { "d1" : 2 },
        "KNonDefault1" : { "d1" : 2, "d2" : "v2" }
    })";
    K k {};
    JsonNode node {kJson};
    EXPECT_EQ(Visit<SETVAL>(node["KNonDefault0"], node["K"], k), false);
    EXPECT_EQ(Visit<SETVAL>(node["KNonDefault0"], JsonNode {"{"s}, k), false);
    EXPECT_EQ(Visit<SETVAL>(JsonNode {"{"s}, node["KNonDefault0"], k), false);
    EXPECT_EQ(Visit<SETVAL>(node["KNonDefault1"], node["K"], k), true);
    EXPECT_EQ(k.d1, 2);
    EXPECT_EQ(k.d2, "v2");
}

HWTEST_F(UtilsJsonVisitorUnitTest, testArrayL, TestSize.Level0)
{
    std::string lJson = R"({
        "L" : { "d1" : [1] },
        "LNonDefault0" : { "d1" : [2] },
        "LNonDefault1" : { "d1" : "2" },
        "LNonDefault2" : { "d1" : ["2"] }
    })";
    L l {};
    JsonNode node {lJson};
    EXPECT_EQ(Visit<SETVAL>(node["LNonDefault0"], node["L"], l), true);
    EXPECT_EQ(l.d1, std::vector<int>({2, 1}));

    l = {};
    EXPECT_EQ(Visit<SETVAL>(node["L"], node["LNonDefault0"], l), true);
    EXPECT_EQ(l.d1, std::vector<int>({1, 2}));

    EXPECT_EQ(Visit<SETVAL>(node["LNonDefault1"], node["L"], l), false);
    EXPECT_EQ(Visit<SETVAL>(node["L"], node["LNonDefault1"], l), false);
    EXPECT_EQ(Visit<SETVAL>(node["L"], node["LNonDefault2"], l), false);
    EXPECT_EQ(Visit<SETVAL>(node["LNonDefault2"], node["L"], l), false);
}
} // namespace UpdaterUt
