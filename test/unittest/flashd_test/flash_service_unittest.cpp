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
    FLashServiceUnitTest() {
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

    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "format";
    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 5;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "format databack";
    payload = (uint8_t *)cmdstr.c_str();
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

    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "update 123.zip";
    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 5;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "format databack";
    payload = (uint8_t *)cmdstr.c_str();
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

    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "erase misctest";
    payload = (uint8_t *)cmdstr.c_str();
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

    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::FAIL, ret);

    cmdstr = "flash updatertest updater.img";
    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 30;
    commander->DoCommand(payload, payloadSize);
    EXPECT_EQ(UpdaterState::SUCCESS, ret);
}
} // namespace