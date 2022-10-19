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
static std::string TEST_PARTITION_NAME = "data";
static std::string TEST_UPDATER_PACKAGE_PATH = "/data/updater/updater/updater.zip";
static std::string TEST_FLASH_IMAGE_NAME = "/data/updater/updater/updater.zip";

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
public:
    int TestPartitionDoFormat(std::string partitionName)
    {
        // std::string partitionName = "data";
        Partition part(partitionName);
        return part.DoFormat();
    }

    int TestPartitionDoErase(std::string partitionName)
    {
        // std::string partitionName = "data";
        Partition part(partitionName);
        return part.DoErase();
    }
    // void TestDoFormat()
    // {
    //     commander_ = CreateCommander(CMDSTR_FORMAT_PARTITION);
    //     EXPECT_NE(nullptr, commander_);
    //     uint8_t *payload = "format data";
    //     int payloadSize = 12;
    //     commander_->DoCommand(payload, payloadSize);
    // }

};

HWTEST_F(FLashServiceUnitTest, FormatCommanderDoCommand, TestSize.Level1)
{
    InitUpdaterLogger("FLASHD", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    LoadFstab();
    mkdir("/data/test", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    int ret = access("/data/test" , 0);
    EXPECT_EQ(0, ret);
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    auto callbackFail = [](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        // SendToHost(type, state, msg);
        LOG(INFO) << "state is" << (int)state;
        EXPECT_EQ(UpdaterState::FAIL, state);
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_FORMAT_PARTITION, callbackFail);
    EXPECT_NE(nullptr, commander);
    std::string cmdstr = "format data";
    uint8_t *payload = nullptr;
    int payloadSize = 12;
    commander->DoCommand(payload, payloadSize);

    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);

    cmdstr = "format";
    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 5;
    commander->DoCommand(payload, payloadSize);

    cmdstr = "format databack";
    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 16;
    commander->DoCommand(payload, payloadSize);

    auto callbackSuccess = [](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        // SendToHost(type, state, msg);
        LOG(INFO) << "state is" << (int)state;
        EXPECT_EQ(UpdaterState::SUCCESS, state);
    };
    std::unique_ptr<Flashd::Commander> commander2 = nullptr;
    commander2 = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_FORMAT_PARTITION, callbackSuccess);
    cmdstr = "format data";
    payload = (uint8_t *)cmdstr.c_str();
    payloadSize = 12;
    commander2->DoCommand(payload, payloadSize);

    ret = access("/data/updater/test" , 0);
    EXPECT_EQ(-1, ret);


}

HWTEST_F(FLashServiceUnitTest, UpdateCommanderDoCommand, TestSize.Level1)
{
    LoadFstab();
    FLashServiceUnitTest test;
    std::unique_ptr<Flashd::Commander> commander = nullptr;
    auto callbackFail = [](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        // SendToHost(type, state, msg);
        LOG(INFO) << "state is" << (int)state;
        EXPECT_EQ(UpdaterState::FAIL, state);
    };
    commander = Flashd::CommanderFactory::GetInstance().CreateCommander(Hdc::CMDSTR_FORMAT_PARTITION, callbackFail);
    std::string cmdstr = "update";
    uint8_t *payload = nullptr;
    int payloadSize = 12;
    commander->DoCommand(payload, payloadSize);

    payload = (uint8_t *)cmdstr.c_str();;
    payloadSize = 0;
    commander->DoCommand(payload, payloadSize);

    std::string options = "";
    unsigned int fileSize = 2398698;
    commander->DoCommand(options, fileSize);



}


HWTEST_F(FLashServiceUnitTest, TestPartitionDoFormat, TestSize.Level1)
{
    LoadFstab();
    FLashServiceUnitTest test;
    // if (access(FLASHD_FILE_PATH, F_OK) == -1) {
    //     mkdir(FLASHD_FILE_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    // }
    mkdir("/data/updater/test", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    int ret = access("/data/updater/test" , 0);
    EXPECT_EQ(0, ret);
    ret = test.TestPartitionDoFormat("data");
    EXPECT_EQ(0, ret);
    ret = access("/data/updater/test" , 0);
    EXPECT_EQ(-1, ret);
}

HWTEST_F(FLashServiceUnitTest, TestPartitionDoErase, TestSize.Level1)
{
    LoadFstab();
    FLashServiceUnitTest test;
    struct UpdateMessage msg {};
    // if (strncpy_s(msg.command, sizeof(msg.command), "boot_updater", strlen("boot_updater") + 1) != EOK) {
    //     LOG(ERROR) << "SetRetryCountToMisc strncpy_s failed";
    //     return;
    // }
    int ret = strncpy_s(msg.command, sizeof(msg.command), "boot_updater", strlen("boot_updater") + 1); 
    EXPECT_EQ(0, ret);
    bool isTrue = WriteUpdaterMiscMsg(msg);
    EXPECT_EQ(1, isTrue);

    struct UpdateMessage boot {};
    // read from misc
    isTrue = ReadUpdaterMiscMsg(boot);
    EXPECT_EQ(1, isTrue);
    LOG(INFO)<< "GetBootMode(mode)  " << boot.command;
    // ret = (boot.command == "boot_updater") ? 1 : 0;
    ret = strcmp(boot.command, "boot_updater");
    EXPECT_EQ(0, ret);
    ret = test.TestPartitionDoErase("misc");
    EXPECT_EQ(0, ret);
    // ret = access("/data/updater/test_erase" , 0);
    isTrue = ReadUpdaterMiscMsg(boot);
    LOG(INFO)<< "GetBootMode(mode)  " << boot.command;
    EXPECT_EQ(1, isTrue);
    // ret = (boot.command == "") ? 1 : 0;
    ret = strcmp(boot.command, "");
    EXPECT_EQ(0, ret);
}
} // namespace