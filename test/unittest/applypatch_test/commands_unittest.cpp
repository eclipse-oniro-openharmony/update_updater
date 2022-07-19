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
#include "applypatch/command.h"
#include "log/log.h"

using namespace testing::ext;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
class CommandsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
};

void CommandsUnitTest::SetUpTestCase()
{
    cout << "Updater Unit CommandsUnitTest Setup!" << endl;
}

void CommandsUnitTest::SetUp()
{
    cout << "Updater Unit CommandsUnitTest Begin!" << endl;
}

void CommandsUnitTest::TearDown()
{
    cout << "Updater Unit CommandsUnitTest End!" << endl;
}

HWTEST_F(CommandsUnitTest, command_test_001, TestSize.Level0)
{
    std::string hashValue = "5aa246ebe8e817740f12cc0f6e536c5ea22e5db177563a1caea5a86614275546";
    std::string blockInfo = "2,20755,21031 276 2,20306,20582";
    std::string cmdLine = std::string("move ") + hashValue + " " + blockInfo;
    Command *cmd = new Command();
    cmd->Init(cmdLine);
    auto type = cmd->GetCommandType();
    EXPECT_EQ(type, CommandType::MOVE);
    auto sha256 = cmd->GetArgumentByPos(10);
    sha256 = cmd->GetArgumentByPos(1);
    EXPECT_EQ(sha256, "5aa246ebe8e817740f12cc0f6e536c5ea22e5db177563a1caea5a86614275546");
    EXPECT_EQ(cmd->GetCommandLine(), cmdLine);
    delete cmd;
}

HWTEST_F(CommandsUnitTest, command_test_002, TestSize.Level0)
{
    std::string hashValue = "5aa246ebe8e817740f12cc0f6e536c5ea22e5db177563a1caea5a86614275546";
    std::string blockInfo = "2,20755,21031 276 2,20306,20582";
    std::string cmdLine = std::string("move ") + hashValue + " " + blockInfo;
    Command *cmd = new Command();
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "abort";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "bsdiff 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "earse 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "free 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "pkgdiff 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "move 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "new 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "stash 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "zero 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    cmdLine = "last 1,1";
    EXPECT_EQ(cmd->Init(cmdLine), true);
    delete cmd;
}
} // updater_ut
