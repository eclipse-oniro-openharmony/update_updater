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
#include <gtest/gtest.h>
#include <memory>
#include <sys/ioctl.h>
#include "common/flashd_define.h"
#include "daemon/format_commander.h"
#include "daemon/commander_factory.h"
#include "daemon/flashd_utils.h"
#include "partition.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "misc_info/misc_info.h"

using namespace std;
using namespace Flashd;
using namespace testing::ext;

namespace {

class FLashServiceUnitTest : public testing::Test {
public:
    FLashServiceUnitTest()
    {
        std::cout<<"FLashServiceUnitTest()";
        InitUpdaterLogger("FLASHD", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    }
    ~FLashServiceUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
    std::unique_ptr<Flashd::Commander> commander_ = nullptr;
};

HWTEST_F(FLashServiceUnitTest, FormatCommanderDoCommand, TestSize.Level1)
{
    LoadFstab();
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    Flashd::UpdaterState ret = UpdaterState::DOING;
    auto callbackFail = [&ret](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        ret = state;
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_FORMAT_PARTITION, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmdstr = "format data";
    uint8_t *payload = nullptr;
    int payloadSize = 12;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "format";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 5;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "format databack";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 16;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);
}

HWTEST_F(FLashServiceUnitTest, UpdateCommanderDoCommand, TestSize.Level1)
{
    LoadFstab();
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    Flashd::UpdaterState ret = UpdaterState::DOING;
    auto callbackFail = [&ret](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        ret = state;
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_UPDATE_SYSTEM, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmdstr = "test.zip";
    uint8_t *payload = nullptr;
    int payloadSize = 12;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "update 123.zip";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 5;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "format databack";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 16;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);
}

HWTEST_F(FLashServiceUnitTest, EraseCommanderDoCommand, TestSize.Level1)
{
    LoadFstab();
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    Flashd::UpdaterState ret = UpdaterState::DOING;
    auto callbackFail = [&ret](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        ret = state;
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_ERASE_PARTITION, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmdstr = "erase misc";
    uint8_t *payload = nullptr;
    int payloadSize = 11;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "erase";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 5;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "erase misctest";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 15;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);
}

HWTEST_F(FLashServiceUnitTest, FlashCommanderDoCommand, TestSize.Level1)
{
    LoadFstab();
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    Flashd::UpdaterState ret = UpdaterState::DOING;
    auto callbackFail = [&ret](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        ret = state;
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_FLASH_PARTITION, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmdstr = "flash updater updater.img";
    uint8_t *payload = nullptr;
    int payloadSize = 26;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "flash updatertest updater.img";
    payload = static_cast<uint8_t*>(cmdstr.c_str());
    payloadSize = 30;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::SUCCESS, ret);
}

HWTEST_F(FLashServiceUnitTest, GetFileName, TestSize.Level1)
{
    std::string testStr = "data/test/test.zip";
    std::string res = GetFileName(testStr);
    EXPECT_EQ("test.zip", res);

    testStr = "D:\\test\\test.zip";
    res = GetFileName(testStr);
    EXPECT_EQ("test.zip", res);

    testStr = "test.zip";
    res = GetFileName(testStr);
    EXPECT_EQ("", res);
}

HWTEST_F(FLashServiceUnitTest, Split, TestSize.Level1)
{
    std::string input = "";
    std::vector<std::string> output = Split(input, { "-f" });
    std::string res = "";
    for (auto s : output) {
        res += s;
    }
    EXPECT_EQ("", res);
    input = "flash updater updater.img";
    output = Split(input, { "-f" });
    res = "";
    for (auto s : output) {
        res += s;
    }
    EXPECT_EQ("flashupdaterupdater.img", res);
}

HWTEST_F(FLashServiceUnitTest, GetWriter, TestSize.Level1)
{
    std::string partName = "";
    std::string temp = "";
    uint8_t *buffer = static_cast<uint8_t*>(temp.c_str());
    int bufferSize = 24;
    std::unique_ptr<FlashdWriter> writer = FlashdImageWriter::GetInstance().GetWriter(partName, buffer, bufferSize);
    if (writer == nullptr) {
        std::cout << "writer is nullptr";
    }
    EXPECT_NE(nullptr, writer);

    partName = "test";
    writer = FlashdImageWriter::GetInstance().GetWriter(partName, buffer, bufferSize);
    EXPECT_NE(nullptr, writer);
}

HWTEST_F(FLashServiceUnitTest, PartitionDoErase, TestSize.Level1)
{
    std::string partitionName = "test";
    Partition partTest(partitionName);
    int ret = partTest.DoErase();
    EXPECT_EQ(FLASHING_OPEN_PART_ERROR, ret);
}

HWTEST_F(FLashServiceUnitTest, PartitionDoFormat, TestSize.Level1)
{
    std::string partitionName = "test";
    Partition partTest(partitionName);
    int ret = partTest.DoFormat();
    EXPECT_EQ(-1, ret);
}

HWTEST_F(FLashServiceUnitTest, PartitionDoFlash, TestSize.Level1)
{
    std::string temp = "test.img";
    uint8_t *buffer = static_cast<uint8_t*>(temp.c_str());
    int bufferSize = 0;
    std::unique_ptr<FlashdWriter> writer = nullptr;
    std::string partName = "updater";
    std::string cmdstr = "flash updater updater.img";
    uint8_t *payload = static_cast<uint8_t*>(cmdstr.c_str());
    int payloadSize = 26;
    std::unique_ptr<Partition> partition_ = std::make_unique<Partition>(partName, std::move(writer));
    EXPECT_NE(nullptr, partition_);
    int ret = partition_->DoFlash(payload, payloadSize);
    EXPECT_EQ(FLASHING_ARG_INVALID, ret);
    writer = FlashdImageWriter::GetInstance().GetWriter(partName, buffer, bufferSize);
    EXPECT_NE(nullptr, partition_);

    payloadSize = 0;
    partition_ = std::make_unique<Partition>(partName, std::move(writer));
    ret = partition_->DoFlash(payload, payloadSize);
    EXPECT_EQ(FLASHING_ARG_INVALID, ret);
    payloadSize = 26;
    payload = nullptr;
    ret = partition_->DoFlash(payload, payloadSize);
    EXPECT_EQ(FLASHING_ARG_INVALID, ret);
}

HWTEST_F(FLashServiceUnitTest, UpdateProgress, TestSize.Level1)
{
    std::string partitionName = "test";
    Partition partTest(partitionName);
    int ret = partTest.DoFormat();
    EXPECT_EQ(-1, ret);
}

HWTEST_F(FLashServiceUnitTest, UpdateCommanderDoCommand2, TestSize.Level1)
{
    LoadFstab();
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    Flashd::UpdaterState ret = UpdaterState::DOING;
    auto callbackFail = [&ret](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        ret = state;
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_UPDATE_SYSTEM, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmd = "update";
    size_t size = 12;
    commander->DoCommand(cmd, size);
    EXPECT_EQ(UpdaterState::FAIL, ret);
    cmd = "update test test.zip";
    commander->DoCommand(cmd, size);
    EXPECT_EQ(UpdaterState::FAIL, ret);
}

HWTEST_F(FLashServiceUnitTest, FlashCommanderDoCommand2, TestSize.Level1)
{
    LoadFstab();
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    Flashd::UpdaterState ret = UpdaterState::DOING;
    auto callbackFail = [&ret](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        ret = state;
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_FLASH_PARTITION, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmd = "flash";
    size_t size = 12;
    commander->DoCommand(cmd, size);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmd = "flash test test.img";
    size = 20;
    commander->DoCommand(cmd, size);
    EXPECT_EQ(UpdaterState::FAIL, ret);
}

} // namespace