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
#include <fstream>
#include "cJSON.h"
#include "json_node.h"
#include "utils.h"

using namespace std::literals;
using namespace std;
using namespace testing::ext;
using namespace Updater;
using namespace Utils;

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

HWTEST_F(UtilsJsonNodeUnitTest, TestGetKey, TestSize.Level0)
{
    {
        std::string str = R"({"key": "value1"})";
        JsonNode node(str);
        EXPECT_EQ(node.Key(), std::nullopt);
        EXPECT_EQ(node["key"].Key(), "key");
    }
    {
        std::string str = R"({"key"})";
        JsonNode node(str);
        EXPECT_EQ(node.Key(), std::nullopt);
    }
}

HWTEST_F(UtilsJsonNodeUnitTest, TestEqualTypeNotMatched, TestSize.Level0)
{
    std::string str = R"({"key": "value1"})";
    JsonNode node(str);
    EXPECT_EQ((node["key"] == 1), false);
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

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidKey, TestSize.Level0)
{
    std::string str = R"({"key": "value1"})";
    JsonNode node(str);
    EXPECT_EQ(node["key1"].Type(), NodeType::UNKNOWN);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidIndex, TestSize.Level0)
{
    {
        std::string str = R"({"key": "value1"})";
        JsonNode node(str);
        EXPECT_EQ(node["key"].Type(), NodeType::STRING);
        EXPECT_EQ(node["key1"].Type(), NodeType::UNKNOWN);
        EXPECT_EQ(node["key"].Type(), NodeType::STRING);
    }
    {
        std::string str = R"({"key": [1]})";
        JsonNode node(str);
        EXPECT_EQ(node["key"].Type(), NodeType::ARRAY);
        EXPECT_EQ(node["key"][0].Type(), NodeType::INT);
        EXPECT_EQ(node["key"][1].Type(), NodeType::UNKNOWN);
    }
}

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidPath0, TestSize.Level0)
{
    JsonNode node(Fs::path {R"(\invalid)"});
    EXPECT_EQ(node.Type(), NodeType::UNKNOWN);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidPath1, TestSize.Level0)
{
    JsonNode node(Fs::path {"/data/noexist"});
    EXPECT_EQ(node.Type(), NodeType::UNKNOWN);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidPath2, TestSize.Level0)
{
    constexpr auto invalidContent = R"({ "key" : "value")";
    constexpr auto invalidJsonPath = "/tmp/tmp.json";
    {
        std::ofstream ofs(Fs::path {invalidJsonPath});
        ofs << invalidContent;
        ofs.flush();
    }
    JsonNode node(Fs::path {invalidJsonPath});
    EXPECT_EQ(node.Type(), NodeType::UNKNOWN);
    DeleteFile(invalidJsonPath);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestVectorAssign, TestSize.Level0)
{
    std::string str = R"({"key":[1, true, "value"]})";
    JsonNode node {str};
    constexpr int intVal = 2;
    constexpr bool boolVal = false;
    const char *strVal = "newValue";
    int idx = 0;
    node["key"][idx++] = intVal;
    node["key"][idx++] = boolVal;
    node["key"][idx++] = strVal;
    EXPECT_EQ(node["key"][--idx], strVal);
    EXPECT_EQ(node["key"][--idx], boolVal);
    EXPECT_EQ(node["key"][--idx], intVal);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestIntAssign, TestSize.Level0)
{
    std::string str = R"({"key":1})";
    JsonNode node {str};
    constexpr int intVal = 2;
    node["key"] = intVal;
    EXPECT_EQ(node["key"], intVal);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestBoolAssign, TestSize.Level0)
{
    std::string str = R"({"key":true})";
    JsonNode node {str};
    constexpr bool boolVal = false;
    node["key"] = boolVal;
    EXPECT_EQ(node["key"], boolVal);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestStrAssign, TestSize.Level0)
{
    std::string str = R"({"key":"value"})";
    JsonNode node {str};
    const char *strVal = "newValue";
    node["key"] = strVal;
    EXPECT_EQ(node["key"], strVal);
}

HWTEST_F(UtilsJsonNodeUnitTest, TestMultiLevelAssign, TestSize.Level0)
{
    std::string str = R"({"key1":{
        "key2":1,
        "key3":"value"
    }})";
    JsonNode node {str};
    constexpr int newValue = 2;
    node["key1"]["key2"] = newValue;
    node["key1"]["key3"] = "value2";
    EXPECT_EQ(node["key1"]["key2"], newValue);
    EXPECT_EQ(node["key1"]["key3"], "value2");
}

HWTEST_F(UtilsJsonNodeUnitTest, TestInvalidAssign, TestSize.Level0)
{
    std::string str = R"({"key" : 1})";
    JsonNode node {str};
    constexpr int value = 1;
    node["key"] = false;
    EXPECT_EQ(node["key"], value);
    node["key"] = "newValue";
    EXPECT_EQ(node["key"], value);
}
} // namespace UpdaterUt
