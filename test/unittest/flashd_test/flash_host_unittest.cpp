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

#include "common/common.h"
#include "common/transfer.h"
#include "host_updater.h"
#include "serial_struct.h"
#include "unittest_comm.h"

using namespace std;
using namespace Hdc;
using namespace testing::ext;
namespace {
static std::string TEST_PARTITION_NAME = "data";
static std::string TEST_UPDATER_PACKAGE_PATH = "/data/updater/updater/updater.zip";
static std::string TEST_FLASH_IMAGE_NAME = "/data/updater/updater/test.img";

class FLashHostUnitTest : public testing::Test {
public:
    FLashHostUnitTest() {}
    ~FLashHostUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

public:
    int TestFlashHost(uint16_t command, const std::string &cmd)
    {
        uv_loop_t loopMain;
        uv_loop_init(&loopMain);

        HTaskInfo hTaskInfo = nullptr;
        std::shared_ptr<TaskInformation> task = std::make_shared<TaskInformation>();
        if (task == nullptr) {
            return -1;
        }
        hTaskInfo = task.get();
        hTaskInfo->channelId = 1;
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 0;
        std::shared_ptr<HostUpdater> flashHost = std::make_shared<HostUpdater>(hTaskInfo);
        if (flashHost == nullptr) {
            return -1;
        }
        flashHost->CommandDispatch(command,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(cmd.data())), cmd.size());
        return 0;
    }

    int TestFlashProgress(uint16_t command, const std::string &cmd, uint32_t progress)
    {
        uv_loop_t loopMain;
        uv_loop_init(&loopMain);

        HTaskInfo hTaskInfo = nullptr;
        std::shared_ptr<TaskInformation> task = std::make_shared<TaskInformation>();
        if (task == nullptr) {
            return -1;
        }
        hTaskInfo = task.get();
        hTaskInfo->channelId = 1;
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 0;
        std::shared_ptr<HostUpdater> flashHost = std::make_shared<HostUpdater>(hTaskInfo);
        if (flashHost == nullptr) {
            return -1;
        }
        flashHost->CommandDispatch(command,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(cmd.data())), cmd.size());
        flashHost->OpenFile();

        std::vector<uint8_t> data(MAX_SIZE_IOBUF * 2); // 2
        flashHost->CommandDispatch(CMD_UPDATER_BEGIN, const_cast<uint8_t *>(data.data()), data.size());

        std::string cmdInfo = "";
        flashHost->CommandDispatch(CMD_UPDATER_CHECK,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(cmdInfo.data())), cmdInfo.size());

        flashHost->CommandDispatch(CMD_UPDATER_DATA, const_cast<uint8_t *>(data.data()), data.size());

        vector<uint8_t> info = {0, 1, 's', 'u', 'c', 'c', 'e', 's', 's'};
        flashHost->CommandDispatch(CMD_UPDATER_FINISH,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(info.data())), info.size());

        uint32_t percentage = 30; // 30 progress
        cmdInfo.resize(sizeof(percentage));
        (void)memcpy_s(cmdInfo.data(), cmdInfo.size(), &percentage, sizeof(percentage));
        flashHost->CommandDispatch(CMD_UPDATER_PROGRESS,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(cmdInfo.data())), cmdInfo.size());

        percentage = (uint32_t)progress;
        cmdInfo.resize(sizeof(percentage));
        (void)memcpy_s(cmdInfo.data(), cmdInfo.size(), &percentage, sizeof(percentage));
        flashHost->CommandDispatch(CMD_UPDATER_PROGRESS,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(cmdInfo.data())), cmdInfo.size());

        return 0;
    }
};

HWTEST_F(FLashHostUnitTest, TestFlashHost, TestSize.Level1)
{
    FLashHostUnitTest test;
    Base::SetLogLevel(LOG_LAST);  // debug log print

    std::string cmdParam = TEST_UPDATER_PACKAGE_PATH;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_UPDATE_INIT, cmdParam));

    cmdParam = " -f ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    cmdParam += TEST_FLASH_IMAGE_NAME;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_FLASH_INIT, cmdParam));

    cmdParam = "erase -f ";
    cmdParam += TEST_PARTITION_NAME;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_ERASE, cmdParam));

    cmdParam = "format -f ";
    cmdParam += TEST_PARTITION_NAME;
    cmdParam += "  -t ext4";
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_FORMAT, cmdParam));
}

HWTEST_F(FLashHostUnitTest, TestFlashProgress, TestSize.Level1)
{
    FLashHostUnitTest test;
    Base::SetLogLevel(LOG_LAST);  // debug log print

    std::string cmdParam = TEST_UPDATER_PACKAGE_PATH;
    EXPECT_EQ(0, test.TestFlashProgress(CMD_UPDATER_UPDATE_INIT, cmdParam, -1));

    cmdParam = " -f ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    cmdParam += TEST_FLASH_IMAGE_NAME;
    EXPECT_EQ(0, test.TestFlashProgress(CMD_UPDATER_FLASH_INIT, cmdParam, -1));
}

HWTEST_F(FLashHostUnitTest, TestFlashProgressFinish, TestSize.Level1)
{
    FLashHostUnitTest test;
    Base::SetLogLevel(LOG_LAST);  // debug log print

    std::string cmdParam = TEST_UPDATER_PACKAGE_PATH;
    EXPECT_EQ(0, test.TestFlashProgress(CMD_UPDATER_UPDATE_INIT, cmdParam, 100));

    cmdParam = " -f ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    cmdParam += TEST_FLASH_IMAGE_NAME;
    EXPECT_EQ(0, test.TestFlashProgress(CMD_UPDATER_FLASH_INIT, cmdParam, 100));
}
} // namespace