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

#include "utils_json_test.h"

#include "cJSON.h"
#include "json_node.h"

using namespace std::literals;
using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace UpdaterUt {
// do something at the each function begining
void UtilsJsonNodeUnitTest::SetUp(void)
{
    cout << "Updater Unit UtilsJsonNodeUnitTest Begin!" << endl;
}

// do something at the each function end
void UtilsJsonNodeUnitTest::TearDown(void)
{
    cout << "Updater Unit UtilsJsonNodeUnitTest End!" << endl;
}

// init
void UtilsJsonNodeUnitTest::SetUpTestCase(void)
{
    cout << "SetUpTestCase" << endl;
}

// end
void UtilsJsonNodeUnitTest::TearDownTestCase(void)
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(UtilsJsonNodeUnitTest, TestStrNode, TestSize.Level0)
{
    std::string str = R"({"key": "value1"})";
    JsonNode node(str);
    EXPECT_EQ(node["key"].Type(), NodeType::STRING);
    EXPECT_EQ(node["key"], "value1");
}

HWTEST_F(UtilsJsonNodeUnitTest, TestIntNode, TestSize.Level0)
{
    std::string str = R"({"key": 1})";
    JsonNode node(str);
    EXPECT_EQ(node["key"].Type(), NodeType::INT);
    EXPECT_EQ(node["key"], 1);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestBoolNode, TestSize.Level0)
{
    std::string str = R"({"key": true})";
    JsonNode node(str);
    EXPECT_EQ(node["key"].Type(), NodeType::BOOL);
    EXPECT_EQ(node["key"], true);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestArrNode, TestSize.Level0)
{
    std::string str = R"({"key": [1, true, "value"]})";
    JsonNode node(str);
    EXPECT_EQ(node["key"].Type(), NodeType::ARRAY);
    EXPECT_EQ(node["key"][0], 1);
    EXPECT_EQ(node["key"][1], true);
    EXPECT_EQ(node["key"][2], "value");
}


HWTEST_F(UtilsJsonNodeUnitTest, TestObjNode, TestSize.Level0)
{
    std::string str = R"({"key": {"key" : "value"}}})";
    JsonNode node(str);
    EXPECT_EQ(node["key"].Type(), NodeType::OBJECT);
    EXPECT_EQ(node["key"]["key"], "value");
}

HWTEST_F(UtilsJsonNodeUnitTest, TestNULNode, TestSize.Level0)
{
    std::string str = R"({"key1": null})";
    JsonNode node(str);
    EXPECT_EQ(node["key1"].Type(), NodeType::NUL);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidNode, TestSize.Level0)
{
    std::string str = R"({"key":})";
    JsonNode node(str);
    EXPECT_EQ(node.Type(), NodeType::UNKNOWN);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestAll, TestSize.Level0)
{
    static const std::string str = R"(
    {
        "A": "B",
        "C": {
            "D": "E",
            "F": {
                "G": {
                    "H": "I",
                    "J": 8879,
                    "K": {
                        "L": "M",
                        "N": ["O", "P"]
                    },
                    "L": true
                }
            }
        }
    })";
    JsonNode node(str);
    const JsonNode &nodeC = node["C"];
    const JsonNode &nodeG = nodeC["F"]["G"];
    EXPECT_EQ(node.Type(), NodeType::OBJECT);
    EXPECT_EQ(node["A"], "B");
    EXPECT_EQ(nodeC["D"], "E");
    EXPECT_EQ(nodeG["H"], "I");
    EXPECT_EQ(nodeG["J"], 8879);
    EXPECT_EQ(nodeG["K"]["L"], "M");
    EXPECT_EQ(nodeG["K"]["N"][0], "O");
    EXPECT_EQ(nodeG["K"]["N"][1], "P");
    EXPECT_EQ(nodeG["L"], true);
}
} // namespace UpdaterUt
