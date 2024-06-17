/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "updater_utils_fuzzer.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "cJSON.h"
#include "json_node.h"
#include "log/log.h"
#include "utils.h"

using namespace std::literals;
using namespace std;
using namespace Updater;
using namespace Utils;

namespace {
void CloseStdout(void)
{
    int fd = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return;
    }
    dup2(fd, 1);
    close(fd);
}

void TestTrim(const uint8_t* data, size_t size)
{
    Utils::Trim("");
    Utils::Trim("   ");
    Utils::Trim("aa   ");
    Utils::Trim(std::string(reinterpret_cast<const char*>(data), size));
}

void TestConvertSha256Hex(void)
{
    uint8_t a[1] = {0};
    a[0] = 1;
    Utils::ConvertSha256Hex(a, 1);
}

void TestSplitString(void)
{
    Utils::SplitString("aaa\nbbb", "\n");
}

void TestMkdirRecursive(void)
{
    Utils::MkdirRecursive("/data/xx?xx", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

void TestString2Int(void)
{
    Utils::String2Int<int>("", 10); // 10 : size
    Utils::String2Int<int>("0x01", 10); // 10 : size
}

void TestGetFilesFromDirectory(void)
{
    std::vector<std::string> files;
    Utils::SaveLogs();
    Utils::CompressLogs("/data/updater/log/updater_log_test");
    Utils::GetFilesFromDirectory("/data/updater/log", files, true);
}

void TestRemoveDir(void)
{
    string path = "";
    Utils::RemoveDir(path);
    path = "/data/updater/utils/nonExistDir";
    Utils::RemoveDir(path);
    path = "/data/updater/rmDir";
    int ret = mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret == 0) {
        ofstream tmpFile;
        string filePath = path + "/tmpFile";
        tmpFile.open(filePath.c_str());
        if (tmpFile.is_open()) {
            tmpFile.close();
            Utils::RemoveDir(path);
        }
    }
}

void TestIsUpdaterMode(void)
{
    Utils::IsUpdaterMode();
}

void TestIsFileExist(void)
{
    Utils::IsFileExist("/bin/test_updater");
    Utils::IsFileExist("/bin/updater_binary");
}

void TestIsDirExist(void)
{
    Utils::IsDirExist("/bin/test_updater");
    Utils::IsDirExist("/bin");
    Utils::IsDirExist("/bin/");
}

void TestCopyUpdaterLogs(void)
{
    const std::string sLog = "/data/updater/main_data/updater.tab";
    const std::string dLog = "/data/updater/main_data/ut_dLog.txt";
    Utils::CopyUpdaterLogs(sLog, dLog);
    unlink(dLog.c_str());
}

void TestGetDirSizeForFile(void)
{
    Utils::GetDirSizeForFile("xxx");
    Utils::GetDirSizeForFile("xxx/xxx");
    Utils::GetDirSizeForFile("/data/updater/updater/etc/fstab.ut.updater");
}

void TestJsonNodeValueKey(void)
{
    {
        std::string str = R"({"key": "value1"})";
        JsonNode node(str);
        node.Key();
        node["key"].Key();
    }
    {
        std::string str = R"({"key"})";
        JsonNode node(str);
        node.Key();
    }
}

void TestJsonNodeKey(void)
{
    std::string str = R"({"key": "value1"})";
    JsonNode node(str);
    node["key"];
}

void TestJsonNodeValueTypeStr(void)
{
    std::string str = R"({"key": "value1"})";
    JsonNode node(str);
    node["key"].Type();
    node["key"];
}

void TestJsonNodeValueTypeInt(void)
{
    std::string str = R"({"key": 1})";
    JsonNode node(str);
    node["key"].Type();
    node["key"];
}

void TestJsonNodeValueTypeBool(void)
{
    std::string str = R"({"key": true})";
    JsonNode node(str);
    node["key"].Type();
    node["key"];
}

void TestJsonNodeValueTypeArray(void)
{
    std::string str = R"({"key": [1, true, "value"]})";
    JsonNode node(str);
    node["key"].Type();
    node["key"][0];
    node["key"][1];
    node["key"][2]; // 2 : value index
}


void TestJsonNodeValueTypeObject(void)
{
    std::string str = R"({"key": {"key" : "value"}}})";
    JsonNode node(str);
    node["key"].Type();
    node["key"]["key"];
}

void TestJsonNodeValueTypeNull(void)
{
    std::string str = R"({"key1": null})";
    JsonNode node(str);
    node["key1"].Type();
}

void TestJsonNodeValueTypeUnknow(void)
{
    std::string str = R"({"key":})";
    JsonNode node(str);
    node.Type();
}

void TestJsonNodeValueTypeJsonNode(void)
{
    std::string str = R"(
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
    node.Type();
    node["A"];
    nodeC["D"];
    nodeG["H"];
    nodeG["J"];
    nodeG["K"]["L"];
    nodeG["K"]["N"][0];
    nodeG["K"]["N"][1];
    nodeG["L"];
}

void TestJsonNodeKey1Type(void)
{
    std::string str = R"({"key": "value1"})";
    JsonNode node(str);
    node["key1"].Type();
}

void TestJsonNodeValueTypeString(void)
{
    {
        std::string str = R"({"key": "value1"})";
        JsonNode node(str);
        node["key"].Type();
        node["key1"].Type();
        node["key"].Type();
    }
    {
        std::string str = R"({"key": [1]})";
        JsonNode node(str);
        node["key"].Type();
        node["key"][0].Type();
        node["key"][1].Type();
    }
}

void TestJsonNodeTypeUnknow(void)
{
    JsonNode node(Fs::path {R"(\invalid)"});
    node.Type();
}

void TestJsonNodeTypeUnknow1(void)
{
    JsonNode node(Fs::path {"/data/noexist"});
    node.Type();
}

void TestJsonNodeFileType(void)
{
    constexpr auto invalidContent = R"({ "key" : "value")";
    constexpr auto invalidJsonPath = "/tmp/tmp.json";
    {
        std::ofstream ofs(Fs::path {invalidJsonPath});
        ofs << invalidContent;
        ofs.flush();
    }
    JsonNode node(Fs::path {invalidJsonPath});
    node.Type();
    DeleteFile(invalidJsonPath);
}

void TestJsonNodeOperation(void)
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
    node["key"][--idx];
    node["key"][--idx];
    node["key"][--idx];
}

void TestJsonNodeValueIntChange(void)
{
    std::string str = R"({"key":1})";
    JsonNode node {str};
    constexpr int intVal = 2;
    node["key"] = intVal;
    node["key"];
}

void TestJsonNodeValueBoolChange(void)
{
    std::string str = R"({"key":true})";
    JsonNode node {str};
    constexpr bool boolVal = false;
    node["key"] = boolVal;
    node["key"];
}

void TestJsonNodeValueStrChange(void)
{
    std::string str = R"({"key":"value"})";
    JsonNode node {str};
    const char *strVal = "newValue";
    node["key"] = strVal;
    node["key"];
}

void TestJsonNodeValueArrayChange(void)
{
    std::string str = R"({"key1":{
        "key2":1,
        "key3":"value"
    }})";
    JsonNode node {str};
    constexpr int newValue = 2;
    node["key1"]["key2"] = newValue;
    node["key1"]["key3"] = "value2";
    node["key1"]["key2"];
    node["key1"]["key3"];
}

void TestJsonNodeValueChange(void)
{
    std::string str = R"({"key" : 1})";
    JsonNode node {str};
    node["key"] = false;
    node["key"];
    node["key"] = "newValue";
    node["key"];
}
}

namespace OHOS {
    void FuzzUtils(const uint8_t* data, size_t size)
    {
        CloseStdout();
        TestTrim(data, size);
        TestConvertSha256Hex();
        TestSplitString();
        TestMkdirRecursive();
        TestString2Int();
        TestGetFilesFromDirectory();
        TestRemoveDir();
        TestIsUpdaterMode();
        TestIsFileExist();
        TestIsDirExist();
        TestCopyUpdaterLogs();
        TestGetDirSizeForFile();
        TestJsonNodeValueKey();
        TestJsonNodeKey();
        TestJsonNodeValueTypeStr();
        TestJsonNodeValueTypeInt();
        TestJsonNodeValueTypeBool();
        TestJsonNodeValueTypeArray();
        TestJsonNodeValueTypeObject();
        TestJsonNodeValueTypeNull();
        TestJsonNodeValueTypeUnknow();
        TestJsonNodeValueTypeJsonNode();
        TestJsonNodeKey1Type();
        TestJsonNodeValueTypeString();
        TestJsonNodeTypeUnknow();
        TestJsonNodeTypeUnknow1();
        TestJsonNodeFileType();
        TestJsonNodeOperation();
        TestJsonNodeValueIntChange();
        TestJsonNodeValueBoolChange();
        TestJsonNodeValueStrChange();
        TestJsonNodeValueArrayChange();
        TestJsonNodeValueChange();
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzUtils(data, size);
    return 0;
}
