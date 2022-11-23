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

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include "applypatch/block_set.h"
#include "applypatch/store.h"
#include "applypatch/transfer_manager.h"
#include "log/log.h"
#include "utils.h"


using namespace testing::ext;
using namespace Updater;
using namespace std;
namespace UpdaterUt {
class AllCmdUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
    bool WriteTestBin(int fd, const uint8_t &data, size_t size) const;
    bool GetTransferContents(const std::string &transferFile, std::string &contents) const;
    int AllCmdUnitTestMove(int &fd, std::vector<std::string> &allCmd, TransferManager &tm);
    void FillTransferHeader(std::vector<std::string> &command, const std::string &headerBuffer) const
    {
        std::vector<std::string> headInfos = Updater::Utils::SplitString(headerBuffer);
        for (const auto &headInfo : headInfos) {
            command.push_back(headInfo);
        }
    }
};

void AllCmdUnitTest::SetUpTestCase()
{
    cout << "Updater Unit AllCmdUnitTest Setup!" << endl;
}

void AllCmdUnitTest::SetUp()
{
    cout << "Updater Unit AllCmdUnitTest Begin!" << endl;
}

void AllCmdUnitTest::TearDown()
{
    cout << "Updater Unit AllCmdUnitTest End!" << endl;
}

bool AllCmdUnitTest::GetTransferContents(const std::string &transferFile, std::string &contents) const
{
    int fd = open(transferFile.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    contents.clear();
    bool rc = Updater::Utils::ReadFileToString(fd, contents);
    close(fd);
    return rc;
}

bool AllCmdUnitTest::WriteTestBin(int fd, const uint8_t &data, size_t size) const
{
    ssize_t written = 0;
    size_t rest = size;
    size_t count = 4096;

    const uint8_t *p = &data;
    while (rest > 0) {
        do {
            written = write(fd, p, count);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return false;
        }
        rest -= written;
    }
    return true;
}

// Testcase for testing all commands without "new" command.
// new command is not easy to simulate, it depends on
// compression and other condition.
// Leave new command to be covered by update_image_block test.
HWTEST_F(AllCmdUnitTest, allCmd_test_001, TestSize.Level1)
{
    TransferManager *tm = TransferManager::GetTransferManagerInstance();
    // Read source

    char filename[] = {"/data/updater/updater/allCmdUnitTest.bin"};
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        std::cout << "Failed to open test data allCmdUnitTest.bin : " << errno << std::endl;
        return;
    }

    std::string transferContents = "";
    // Read transfer list then run all commands.
    // Cover all correct transfer data, expect all commands return correctly.
    bool rc = GetTransferContents("/data/updater/applypatch/cmd_001_correct.transfer.list", transferContents);
    EXPECT_TRUE(rc);
    std::vector<std::string> transferLines = Updater::Utils::SplitString(transferContents, "\n");
    std::cout << "Dump transfer lines: " << std::endl;
    for (const auto &line : transferLines) {
        std::cout << line << std::endl;
    }
    std::cout << "Dump transfer line done." << std::endl;
    TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase = "/data/updater/update_tmp";
    std::string storePath = "/data/updater/update_tmp";
    Store::CreateNewSpace(storePath, false);
    rc = tm->CommandsParser(fd, transferLines);
    EXPECT_TRUE(rc);
}

int AllCmdUnitTest::AllCmdUnitTestMove(int &fd, std::vector<std::string> &allCmd, TransferManager &tm)
{
    size_t bufferSize = 4096;
    size_t count = 10;
    std::vector<uint8_t> buffer1(bufferSize, 1);
    lseek64(fd, 0, SEEK_SET);
    auto res = WriteTestBin(fd, *buffer1.data(), bufferSize * count);
    if (!res) {
        printf("Write to bin error\n");
    }

    std::string baseBlockPair = "2,1,2";
    BlockSet baseBlock;
    baseBlock.ParserAndInsert(baseBlockPair);
    if (baseBlock.WriteZeroToBlock(fd, false) != 0) {
        std::cout << "Write 0 to bin error: " << errno << std::endl;
    }

    allCmd.pop_back();
    allCmd.push_back("move ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7 2,0,1 1 2,1,2");
    bool result = tm.CommandsParser(fd, allCmd);
    LOG(INFO) << "CommandsParser result:" << result;

    lseek64(fd, 0, SEEK_SET);
    res = WriteTestBin(fd, *buffer1.data(), bufferSize * count);
    if (!res) {
        std::cout << "Write to bin error" << std::endl;
    }
    if (baseBlock.WriteZeroToBlock(fd, false) != 0) {
        std::cout << "Write 0 to bin error: " << errno << std::endl;
    }

    close(fd);
    return 0;
}

HWTEST_F(AllCmdUnitTest, allCmd_test_002, TestSize.Level1)
{
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    std::string filePath = "/tmp/test.bin";
    size_t bufferSize = 4096;
    size_t count = 10;
    mode_t dirMode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase = "/tmp/cmdtest";
    Store::DoFreeSpace(TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase);
    Utils::MkdirRecursive(TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase, dirMode);
    std::vector<uint8_t> buffer(bufferSize, 0);
    int fd = open(filePath.c_str(), O_RDWR | O_CREAT, dirMode);
    if (fd < 0) {
        printf("Failed to open block %s, errno: %d\n", filePath.c_str(), errno);
        return;
    }
    lseek64(fd, 0, SEEK_SET);
    auto res = WriteTestBin(fd, *buffer.data(), bufferSize * count);
    if (!res) {
        printf("Write to bin error\n");
    }

    bool result = false;
    mode_t mode = (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    //
    // 构造命令
    std::vector<std::string> allCmd;
    FillTransferHeader(allCmd, "4 27280 0 3616");
    allCmd.push_back("stash ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7 2,0,1");  // 清第一块
    result = tm->CommandsParser(fd, allCmd);
    EXPECT_TRUE(result);
    Store::DoFreeSpace(TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase);
    Utils::MkdirRecursive(TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase, mode);

    EXPECT_EQ(AllCmdUnitTestMove(fd, allCmd, *tm), 0);
    unlink(filePath.c_str());
}
} // updater_ut
