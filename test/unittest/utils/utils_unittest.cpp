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
#include <limits>
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <unittest_comm.h>
#include "utils.h"
#include "misc_info/misc_info.h"
#include "securec.h"
#include "updater_const.h"
#include "fs_manager/mount.h"

using namespace Updater;
using namespace testing::ext;
using namespace std;

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

HWTEST_F(UtilsUnitTest, updater_utils_test_002, TestSize.Level0)
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

HWTEST_F(UtilsUnitTest, updater_utils_test_004, TestSize.Level0)
{
    string path = string(PATH_MAX, 'a') + "/";
    EXPECT_EQ(Utils::MkdirRecursive(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH), -1);
    path = "/data/updater/firstDir/secondDir";
    EXPECT_EQ(Utils::MkdirRecursive(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH), 0);
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

HWTEST_F(UtilsUnitTest, updater_utils_test_006, TestSize.Level0)
{
    std::vector<std::string> files;
    string path = "/no_exist";
    EXPECT_EQ(Utils::GetFilesFromDirectory(path, files, true), -1);
    path = "/data";
    Utils::GetFilesFromDirectory(path, files, true);
}

HWTEST_F(UtilsUnitTest, WriteFully, TestSize.Level0)
{
    string path = "/data/updater/test_file";
    uint8_t a[1] = {0};
    auto fd = open(path.c_str(), O_RDWR | O_CREAT, 0777);
    EXPECT_EQ(fd != -1, true);
    EXPECT_EQ(Utils::WriteFully(fd, a, 1), true);
    close(fd);
    auto readFd = open(path.c_str(), O_RDONLY);
    EXPECT_EQ(readFd != -1, true);
    EXPECT_EQ(Utils::WriteFully(readFd, a, 1), false);
    close(readFd);
    EXPECT_EQ(Utils::DeleteFile(path), 0);
}

HWTEST_F(UtilsUnitTest, ReadFully, TestSize.Level0)
{
    string path = "/data/updater/test_file";
    uint8_t a[1] = {0};
    auto fd = open(path.c_str(), O_RDWR | O_CREAT, 0777);
    EXPECT_EQ(fd != -1, true);
    EXPECT_EQ(Utils::WriteFully(fd, a, 1), true);
    EXPECT_EQ(lseek(fd, 0, SEEK_SET) != -1, true);
    EXPECT_EQ(Utils::ReadFully(fd, a, 1), true);
    close(fd);

    auto writeFd = open(path.c_str(),  O_WRONLY);
    EXPECT_EQ(writeFd != -1, true);
    EXPECT_EQ(Utils::ReadFully(writeFd, a, 1), false);
    close(writeFd);
    EXPECT_EQ(Utils::DeleteFile(path), 0);
}

HWTEST_F(UtilsUnitTest, ReadFileToString, TestSize.Level0)
{
    string path = "/data/updater/test_file";
    uint8_t a[1] = {0};
    auto fd = open(path.c_str(), O_RDWR | O_CREAT, 0777);
    EXPECT_EQ(fd != -1, true);
    EXPECT_EQ(Utils::WriteFully(fd, a, 1), true);
    EXPECT_EQ(lseek(fd, 0, SEEK_SET) != -1, true);
    string content;
    EXPECT_EQ(Utils::ReadFileToString(fd, content), true);
    close(fd);

    auto writeFd = open(path.c_str(),  O_WRONLY);
    EXPECT_EQ(writeFd != -1, true);
    EXPECT_EQ(Utils::ReadFileToString(writeFd, content), false);
    close(writeFd);
    EXPECT_EQ(Utils::DeleteFile(path), 0);
}

HWTEST_F(UtilsUnitTest, WriteStringToFile, TestSize.Level0)
{
    string path = "/data/updater/test_file";
    uint8_t a[1] = {0};
    auto fd = open(path.c_str(), O_RDWR | O_CREAT, 0777);
    EXPECT_EQ(fd != -1, true);
    EXPECT_EQ(Utils::WriteFully(fd, a, 1), true);
    string content = "111";
    EXPECT_EQ(Utils::WriteStringToFile(fd, content), true);
    close(fd);

    auto readFd = open(path.c_str(), O_RDONLY);
    EXPECT_EQ(readFd != -1, true);
    EXPECT_EQ(Utils::WriteStringToFile(readFd, content), false);
    close(readFd);
    EXPECT_EQ(Utils::DeleteFile(path), 0);
}

HWTEST_F(UtilsUnitTest, CopyFile, TestSize.Level0)
{
    string src = "/data/updater/no_exist";
    string dest = "/data/updater/dest_file";
    EXPECT_EQ(Utils::CopyFile(src, dest), false);
    src = "/data/updater/src_file";
    auto fd = open(src.c_str(), O_RDWR | O_CREAT, 0777);
    EXPECT_EQ(fd != -1, true);
    uint8_t a[1] = {0};
    EXPECT_EQ(Utils::WriteFully(fd, a, 1), true);
    close(fd);
    EXPECT_EQ(Utils::CopyFile(src, dest), true);
}

HWTEST_F(UtilsUnitTest, CheckDumpResult, TestSize.Level0)
{
    string path = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    auto fd = open(path.c_str(), O_RDWR | O_CREAT, 0777);
    EXPECT_EQ(fd != -1, true);
    EXPECT_EQ(Utils::WriteStringToFile(fd, "fail:\n"), true);
    EXPECT_EQ(lseek(fd, 0, SEEK_SET) != -1, true);
    close(fd);
    EXPECT_EQ(Utils::CheckDumpResult(), true);

    EXPECT_EQ(Utils::DeleteFile(path), 0);
    EXPECT_EQ(Utils::CheckDumpResult(), false);
}

HWTEST_F(UtilsUnitTest, WriteDumpResult, TestSize.Level0)
{
    Utils::WriteDumpResult("pass");
}

HWTEST_F(UtilsUnitTest, RemoveDirTest, TestSize.Level0)
{
    string path = "";
    EXPECT_EQ(Utils::RemoveDir(path), false);
    path = "/data/updater/nonExistDir";
    EXPECT_EQ(Utils::RemoveDir(path), false);
    path = "/data/updater/rmDir";
    int ret = mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    string secPath = path + "/secRmDir";
    int secRet = mkdir(secPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret == 0 && secRet == 0) {
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
    string filePath = "";
    EXPECT_EQ(Utils::DeleteFile(filePath), -1);
    filePath = "/data/updater/tmpFile";
    ofstream tmpFile;
    tmpFile.open(filePath.c_str());
    EXPECT_EQ(tmpFile.is_open(), true);
    tmpFile.close();
    EXPECT_EQ(Utils::DeleteFile(filePath), 0);
}

HWTEST_F(UtilsUnitTest, UsSleep, TestSize.Level1)
{
    Utils::UsSleep(1);
}

HWTEST_F(UtilsUnitTest, PathToRealPath, TestSize.Level1)
{
    string emptyPath = "";
    string realPath = "";
    EXPECT_EQ(Utils::PathToRealPath(emptyPath, realPath), false);

    string path(PATH_MAX + 1, '/');
    EXPECT_EQ(Utils::PathToRealPath(path, realPath), false);

    path = "/data/updater/no_exist";
    EXPECT_EQ(Utils::PathToRealPath(path, realPath), false);

    path = "/data/../system";
    EXPECT_EQ(Utils::PathToRealPath(path, realPath), true);
    EXPECT_EQ(realPath, "/system");
}

HWTEST_F(UtilsUnitTest, DoReboot, TestSize.Level0)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    string rebootTarget = "";
    Utils::DoReboot(rebootTarget);
    struct UpdateMessage msg {};
    struct UpdateMessage resMsg {};
    EXPECT_EQ(ReadUpdaterMessage(path, msg), true);
    EXPECT_EQ(strcmp(msg.command, resMsg.command), 0);

    rebootTarget = "updater";
    string command = "not_boot_updater";
    string resCmd = "boot_updater";
    EXPECT_EQ(strncpy_s(msg.command, sizeof(msg.command) - 1, command.c_str(), command.size()), 0);
    EXPECT_EQ(strncpy_s(resMsg.command, sizeof(resMsg.command) - 1, resCmd.c_str(), resCmd.size()), 0);
    EXPECT_EQ(WriteUpdaterMessage(path, msg), true);
    Utils::DoReboot(rebootTarget);
    EXPECT_EQ(ReadUpdaterMessage(path, msg), true);
    EXPECT_EQ(strcmp(msg.command, resMsg.command), 0);

    rebootTarget = "flashd";
    command = "not_flashd";
    resCmd = "boot_flash";
    EXPECT_EQ(strncpy_s(msg.command, sizeof(msg.command) - 1, command.c_str(), command.size()), 0);
    EXPECT_EQ(strncpy_s(resMsg.command, sizeof(resMsg.command) - 1, resCmd.c_str(), resCmd.size()), 0);
    EXPECT_EQ(WriteUpdaterMessage(path, msg), true);
    Utils::DoReboot(rebootTarget);
    EXPECT_EQ(ReadUpdaterMessage(path, msg), true);
    EXPECT_EQ(strcmp(msg.command, resMsg.command), 0);

    rebootTarget = "bootloader";
    command = "not_boot_loader";
    resCmd = "boot_loader";
    EXPECT_EQ(strncpy_s(msg.command, sizeof(msg.command) - 1, command.c_str(), command.size()), 0);
    EXPECT_EQ(strncpy_s(resMsg.command, sizeof(resMsg.command) - 1, resCmd.c_str(), resCmd.size()), 0);
    EXPECT_EQ(WriteUpdaterMessage(path, msg), true);
    Utils::DoReboot(rebootTarget);
    EXPECT_EQ(ReadUpdaterMessage(path, msg), true);
    EXPECT_EQ(strcmp(msg.command, resMsg.command), 0);

    rebootTarget = "updater";
    string extData = "--update_package=/update/ota_package.zip";
    EXPECT_EQ(strncpy_s(msg.update, sizeof(msg.update) - 1, extData.c_str(), extData.size()), 0);
    EXPECT_EQ(strncpy_s(resMsg.update, sizeof(resMsg.update) - 1, extData.c_str(), extData.size()), 0);
    Utils::DoReboot(rebootTarget, extData);
    EXPECT_EQ(ReadUpdaterMessage(path, msg), true);
    EXPECT_EQ(strcmp(msg.update, resMsg.update), 0);
}
} // updater_ut
