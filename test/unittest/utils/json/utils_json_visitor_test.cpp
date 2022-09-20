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

std::string g_jsonStr = R"({
    "D": {
        "d1":1,
        "d2":"true",
        "d3":true
    },
    "E": {
        "d1":1,
        "d2": {
            "r":1,
            "g":"foo",
            "b":true
        }
    },
    "H": {
        "d1":1,
        "d2":true,
        "d3": {
            "d1":2,
            "d2":true,
            "d3": {
                "d1":3,
                "d2":"foo"
            }
        }
    },
    "I" : {
        "d1": [
            "foo",
            "bar",
            "baz"
        ]
    },
    "J" : {
        "d1" : [
            [
                "foo",
                "bar",
                "baz"
            ],
            [
                "foo1",
                "bar1",
                "baz1"
            ]
        ]
    },
    "K" : {
        "d1" : 1
    },
    "KNonDefault0" : {
        "d1" : 2
    },
    "KNonDefault1" : {
        "d1" : 2,
        "d2" : "v2"
    },
    "L" : {
        "d1" : [1]
    },
    "LNonDefault0" : {
        "d1" : [2]
    },
    "LNonDefault1" : {
        "d1" : "2"
    },
    "LNonDefault2" : {
        "d1" : ["2"]
    }
})";

JsonNode g_node(g_jsonStr);

HWTEST_F(UtilsJsonVisitorUnitTest, testD, TestSize.Level0)
{
    D d {};
    bool res = Visit<SETVAL>(g_node["D"], d);
    EXPECT_EQ(res, true);
    EXPECT_EQ(d.d1, 1);
    EXPECT_EQ(d.d2, "true");
    EXPECT_EQ(d.d3, true);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testE, TestSize.Level0)
{
    E e {};
    bool res = Visit<SETVAL>(g_node["E"], e);
    EXPECT_EQ(res, true);
    EXPECT_EQ(e.d1, 1);
    EXPECT_EQ(e.d2.r, 1);
    EXPECT_EQ(e.d2.g, "foo");
    EXPECT_EQ(e.d2.b, true);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testH, TestSize.Level0)
{
    H h {};
    bool res = Visit<SETVAL>(g_node["H"], h);
    EXPECT_EQ(res, true);
    EXPECT_EQ(h.d1, 1);
    EXPECT_EQ(h.d2, true);
    EXPECT_EQ(h.d3.d1, 2);
    EXPECT_EQ(h.d3.d2, true);
    EXPECT_EQ(h.d3.d3.d1, 3);
    EXPECT_EQ(h.d3.d3.d2, "foo");
}

HWTEST_F(UtilsJsonVisitorUnitTest, testI, TestSize.Level0)
{
    I i {};
    bool res = Visit<SETVAL>(g_node["I"], i);
    EXPECT_EQ(res, true);
    EXPECT_EQ(i.d1, std::vector<std::string>({"foo", "bar", "baz"}));
}

HWTEST_F(UtilsJsonVisitorUnitTest, testJ, TestSize.Level0)
{
    J j {};
    bool res = Visit<SETVAL>(g_node["J"], j);
    EXPECT_EQ(res, true);
    ASSERT_EQ(j.d1.size(), 2);
    EXPECT_EQ(j.d1[0], std::vector<std::string>({"foo", "bar", "baz"}));
    EXPECT_EQ(j.d1[1], std::vector<std::string>({"foo1", "bar1", "baz1"}));
}

HWTEST_F(UtilsJsonVisitorUnitTest, testInvalidK, TestSize.Level0)
{
    K k {};
    bool res = Visit<SETVAL>(g_node["K"], k);
    EXPECT_EQ(res, false);
}

HWTEST_F(UtilsJsonVisitorUnitTest, testNoDefaultAndNonDefaultK, TestSize.Level0)
{
    K k {};
    bool res = Visit<SETVAL>(g_node["KNonDefault0"], g_node["K"], k);
    EXPECT_EQ(res, false);

    res = Visit<SETVAL>(g_node["KNonDefault1"], g_node["K"], k);
    EXPECT_EQ(res, true);
    EXPECT_EQ(k.d1, 2);
    EXPECT_EQ(k.d2, "v2");
}

HWTEST_F(UtilsJsonVisitorUnitTest, testArrayL, TestSize.Level0)
{
    L l {};
    bool res = Visit<SETVAL>(g_node["LNonDefault0"], g_node["L"], l);
    EXPECT_EQ(res, true);
    EXPECT_EQ(l.d1, std::vector<int>({2, 1}));

    l = {};
    res = Visit<SETVAL>(g_node["L"], g_node["LNonDefault0"], l);
    EXPECT_EQ(res, true);
    EXPECT_EQ(l.d1, std::vector<int>({1, 2}));

    l = {};
    res = Visit<SETVAL>(g_node["LNonDefault1"], g_node["L"], l);
    EXPECT_EQ(res, false);

    l = {};
    res = Visit<SETVAL>(g_node["L"], g_node["LNonDefault1"], l);
    EXPECT_EQ(res, false);

    l = {};
    res = Visit<SETVAL>(g_node["L"], g_node["LNonDefault2"], l);
    EXPECT_EQ(res, false);

    l = {};
    res = Visit<SETVAL>(g_node["LNonDefault2"], g_node["L"], l);
    EXPECT_EQ(res, false);
}
} // namespace UpdaterUt
