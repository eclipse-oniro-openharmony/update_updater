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
static std::string TEST_UPDATER_PACKAGE_PATH = "/data/updater/src/updater.zip";
static std::string TEST_FLASH_IMAGE_NAME = "/data/updater/src/image/userdata.img";

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
        hTaskInfo = task.get();
        hTaskInfo->channelId = 1;
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 0;
        std::shared_ptr<HostUpdater> flashHost = std::make_shared<HostUpdater>(hTaskInfo);
        flashHost->CommandDispatch(command,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(cmd.data())), cmd.size());
        return 0;
    }
};

HWTEST_F(FLashHostUnitTest, TestFlashHost, TestSize.Level1)
{
    FLashHostUnitTest test;

    std::string cmdParam = "update  ";
    cmdParam += TEST_UPDATER_PACKAGE_PATH;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_UPDATE_INIT, cmdParam));

    cmdParam = "flash  ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    cmdParam += TEST_FLASH_IMAGE_NAME;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_FLASH_INIT, cmdParam));

    cmdParam = "";
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_CHECK, cmdParam));

    cmdParam = "";
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_BEGIN, cmdParam));

    cmdParam = "";
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_DATA, cmdParam));

    cmdParam = "";
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_FINISH, cmdParam));

    cmdParam = "erase -f ";
    cmdParam += TEST_PARTITION_NAME;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_ERASE, cmdParam));

    cmdParam = "format -f ";
    cmdParam += TEST_PARTITION_NAME;
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_FORMAT, cmdParam));

    uint32_t percentage = 30; // 30 progress
    cmdParam.resize(sizeof(percentage));
    memcpy_s(cmdParam.data(), cmdParam.size(), &percentage, sizeof(percentage));
    EXPECT_EQ(0, test.TestFlashHost(CMD_UPDATER_PROGRESS, cmdParam));
}
} // namespace