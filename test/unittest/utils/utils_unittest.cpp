/*
* Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unittest_comm.h>
#include "utils.h"

using namespace Updater;
using namespace testing::ext;
using namespace std;
using namespace Utils;

namespace UpdaterUt {
class UtilsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(UtilsUnitTest, updater_utils_test_001, TestSize.Level0)
{
    string emptyStr = Utils::Trim("");
    EXPECT_STREQ(emptyStr.c_str(), "");
    emptyStr = Utils::Trim("   ");
    EXPECT_STREQ(emptyStr.c_str(), "");
    emptyStr = Utils::Trim("aa   ");
    EXPECT_STREQ(emptyStr.c_str(), "aa");
}

HWTEST_F(UtilsUnitTest, ConvertSha256Hex, TestSize.Level0)
{
    uint8_t a[1] = {0};
    a[0] = 1;
    string newStr = Utils::ConvertSha256Hex(a, 1);
    EXPECT_STREQ(newStr.c_str(), "01");
}

HWTEST_F(UtilsUnitTest, updater_utils_test_003, TestSize.Level0)
{
    string str = "aaa\nbbb";
    vector<string> newStr = Utils::SplitString(str, "\n");
    EXPECT_EQ(newStr[0], "aaa");
    EXPECT_EQ(newStr[1], "bbb");
}

HWTEST_F(UtilsUnitTest, MkdirRecursive, TestSize.Level0)
{
    std::string path(5000, 'a');
    int ret = MkdirRecursive("/data/xx?xx", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    EXPECT_EQ(ret, 0);

    path = path + "/";
    ret = MkdirRecursive(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    EXPECT_EQ(ret, -1);

    path = "/test/xx?xx";
    ret = MkdirRecursive(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(UtilsUnitTest, updater_utils_test_005, TestSize.Level0)
{
    string input = "";
    int output = Utils::String2Int<int>(input, 10);
    EXPECT_EQ(output, 0);
    input = "0x01";
    output = Utils::String2Int<int>(input, 10);
    EXPECT_EQ(output, 1);
}

HWTEST_F(UtilsUnitTest, GetFilesFromDirectory, TestSize.Level0)
{
    std::vector<std::string> files;
    string path = "";
    int ret = GetFilesFromDirectory(path, files, true);
    EXPECT_EQ(ret, -1);

    path = "/data/updater";     //构造文件目录
    ret = GetFilesFromDirectory(path, files, true);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(UtilsUnitTest, RemoveDirTest, TestSize.Level0)
{
    string path = "";
    EXPECT_EQ(Utils::RemoveDir(path), false);
    path = TEST_PATH_FROM + "../utils/nonExistDir";
    EXPECT_EQ(Utils::RemoveDir(path), false);
    path = "/data/updater/rmDir";
    int ret = mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret == 0) {
        ofstream tmpFile;
        string filePath = path + "/tmpFile";
        tmpFile.open(filePath.c_str());
        if (tmpFile.is_open()) {
            tmpFile.close();
            EXPECT_EQ(Utils::RemoveDir(path), true);
        }
    }
}

HWTEST_F(UtilsUnitTest, IsUpdaterMode, TestSize.Level0)
{
    EXPECT_EQ(Utils::IsUpdaterMode(), false);
}

HWTEST_F(UtilsUnitTest, DeleteFile, TestSize.Level0)
{
    std::string path = "";
    int32_t ret = DeleteFile(path);
    EXPECT_EQ(ret, -1);

    path = "data/test/UtilDel.txt";
    ret = DeleteFile(path);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UtilsUnitTest, WriteFully, TestSize.Level0)
{
    std::string path = "/data/test/WriteFullyTest.txt";
    int fd = open(path.c_str() ,O_RDWR | O_CREAT, 0666);
    std::string str = "aaaaa";
    uint8_t *data = reinterpret_cast<uint8_t*>(str.data());
    bool ret = WriteFully(fd, data, str.size());
    EXPECT_EQ(ret, true);
}

HWTEST_F(UtilsUnitTest, ReadFileToString, TestSize.Level0)
{
    std::string path = "/data/test/WriteFullyTest.txt";
    int fd = open(path.c_str() ,O_RDWR | O_CREAT, 0666);
    std::string str = "";
    bool ret = ReadFileToString(fd, str);
    EXPECT_EQ(ret, true);
}

HWTEST_F(UtilsUnitTest, WriteStringToFile, TestSize.Level0)
{
    std::string path = "/data/test/WriteFullyTest.txt";
    int fd = open(path.c_str() ,O_RDWR | O_CREAT, 0666);
    std::string str = "aaaaabbbb";
    bool ret = WriteStringToFile(fd, str);
    EXPECT_EQ(ret, true);
}

HWTEST_F(UtilsUnitTest, CopyFile, TestSize.Level0)
{
    std::string src = "";
    std::string dest = "";
    bool ret = CopyFile(src, dest);
    EXPECT_EQ(ret, false);
}


} // updater_ut
