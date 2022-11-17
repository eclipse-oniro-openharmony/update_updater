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

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include "../applypatch/command_process.h"
#include "applypatch/transfer_manager.h"
#include "applypatch/store.h"
#include "log/log.h"
#include "utils.h"

using namespace testing::ext;
using namespace Updater;
using namespace std;
namespace UpdaterUt {
class CommandFunctionUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
    CommandResult TestCommandFnExec(std::shared_ptr<Command> cmd, std::string cmdLine)
    {
        cmd->Init(cmdLine);
        std::unique_ptr<CommandFunction> cf = CommandFunctionFactory::GetCommandFunction(cmd->GetCommandType());
        CommandResult ret = cf->Execute(const_cast<Command &>(*cmd.get()));
        CommandFunctionFactory::ReleaseCommandFunction(cf);
        return ret;
    }
};

void CommandFunctionUnitTest::SetUpTestCase()
{
    cout << "Updater Unit CommandFunctionUnitTest Setup!" << endl;
}

void CommandFunctionUnitTest::SetUp()
{
    cout << "Updater Unit CommandFunctionUnitTest Begin!" << endl;
}

void CommandFunctionUnitTest::TearDown()
{
    cout << "Updater Unit CommandFunctionUnitTest End!" << endl;
}

HWTEST_F(CommandFunctionUnitTest, command_function_test_001, TestSize.Level1)
{
    std::string filePath = "/data/updater/updater/allCmdUnitTest.bin";
    std::shared_ptr<Command> cmd;
    cmd = std::make_shared<Command>();
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    mode_t dirMode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    tm->GetGlobalParams()->storeBase = "data/updater/updater/tmp/cmdtest";
    Store::DoFreeSpace(TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase);
    Utils::MkdirRecursive(TransferManager::GetTransferManagerInstance()->GetGlobalParams()->storeBase, dirMode);
    int fd = open(filePath.c_str(), O_RDWR | O_CREAT, dirMode);
    if (fd < 0) {
        printf("Failed to open block %s, errno: %d\n", filePath.c_str(), errno);
        return;
    }
    lseek64(fd, 0, SEEK_SET);
    cmd->SetFileDescriptor(fd);
    std::string cmdLine = std::string("erase 2,0,1");
    CommandResult ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    EXPECT_EQ(ret, 0);
    cmdLine = "free 2,0,1";
    ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    tm->GetGlobalParams()->storeCreated = 0;
    EXPECT_EQ(ret, 0);
    cmdLine = "move ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7 2,3,4 1 2,1,2";
    lseek64(fd, 0, SEEK_SET);
    ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    EXPECT_EQ(ret, 0);
    cmdLine = R"(bsdiff 0 132 ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb4
    7c7a85dabd8b48892ca7 3431383721510cf1c211de027cf958c183e16db5fabb6b230
    eb284c85e196aa9 2,0,1 1 - ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb4
    7c7a85dabd8b48892ca7:2,0,1)";
    lseek64(fd, 0, SEEK_SET);
    ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    EXPECT_EQ(ret, -1);
    cmdLine = "abort";
    ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    EXPECT_EQ(ret, 0);
    cmdLine = "new 2,0,1";
    ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    EXPECT_EQ(ret, -1);
    cmdLine = "stash ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7 2,2,3";
    ret = CommandFunctionUnitTest::TestCommandFnExec(cmd, cmdLine);
    EXPECT_EQ(ret, 0);
    cmdLine = "ppop";
    cmd->Init(cmdLine);
    std::unique_ptr<CommandFunction> cf = CommandFunctionFactory::GetCommandFunction(cmd->GetCommandType());
    EXPECT_EQ(cf, nullptr);
    unlink(filePath.c_str());
}
}
