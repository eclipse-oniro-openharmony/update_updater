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

#include "common.h"
#include "daemon_updater.h"
#include "flash_define.h"
#include "flash_service.h"
#include "flashd/flashd.h"
#include "fs_manager/mount.h"
#include "serial_struct.h"
#include "transfer.h"
#include "unittest_comm.h"

using namespace std;
using namespace flashd;
using namespace Hdc;
using namespace testing::ext;

namespace {
static std::string TEST_PARTITION_NAME = "data";
static std::string TEST_UPDATER_PACKAGE_PATH = "/data/updater/updater/updater.zip";
static std::string TEST_FLASH_IMAGE_NAME = "/data/updater/updater/updater.zip";

class FLashServiceUnitTest : public testing::Test {
public:
    FLashServiceUnitTest() {}
    ~FLashServiceUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

public:
    int TestFindAllDevice()
    {
        Updater::LoadFstab();
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        flash->LoadSysDevice();
        return 0;
    }

    int TestDoFlashPartition()
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        flash->DoFlashPartition(TEST_FLASH_IMAGE_NAME, TEST_PARTITION_NAME);
        return 0;
    }

    int TestDoUpdater()
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        flash->DoUpdate(TEST_UPDATER_PACKAGE_PATH);
        flash->PostProgress(UPDATEMOD_UPDATE, 1024 * 1024 * 4, nullptr); // 1024 * 1024 * 4 4M
        return 0;
    }

    int TestDoErasePartition()
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        flash->DoErasePartition(TEST_PARTITION_NAME);
        return 0;
    }

    int TestDoFormatPartition(const std::string &part, const std::string &type)
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        flash->DoFormatPartition(part, type);
        return 0;
    }

    int TestFlashServiceDoResizeParatiton()
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        flash->DoResizeParatiton("data", 4096); // 4096 partition size
        flash->DoResizeParatiton("data", 1024); // 1024 partition size
        return 0;
    }

    int TestFlashServiceDoPrepare(uint8_t type, const std::string &cmdParam)
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        (void)flashd::DoUpdaterPrepare(flash.get(), type, cmdParam, TEST_UPDATER_PACKAGE_PATH);
        return 0;
    }

    int TestFlashServiceDoFlash(uint8_t type, const std::string &cmdParam)
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        (void)flashd::DoUpdaterPrepare(flash.get(), type, cmdParam, TEST_UPDATER_PACKAGE_PATH);
        return 0;
    }

    int TestFlashServiceDoFinish(uint8_t type)
    {
        std::string errorMsg;
        std::shared_ptr<flashd::FlashService> flash = std::make_shared<flashd::FlashService>(errorMsg);
        if (flash == nullptr) {
            return 1;
        }
        (void)flashd::DoUpdaterFinish(flash.get(), type, "");
        return 0;
    }

    int TestDaemonUpdater()
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
        hTaskInfo->serverOrDaemon = 1;
        std::shared_ptr<DaemonUpdater> hdcDamon = std::make_shared<DaemonUpdater>(hTaskInfo);
        if (hdcDamon == nullptr) {
            return -1;
        }

        // cmd: hdc updater packagename
        // check
        HdcTransferBase::TransferConfig transferConfig {};
        transferConfig.functionName = "update";
        transferConfig.options = "update";
        std::string localPath = TEST_UPDATER_PACKAGE_PATH;
        transferConfig.fileSize = 163884012; // 163884012 file size
        WRITE_LOG(LOG_DEBUG, "CheckMaster %s", transferConfig.functionName.c_str());
        transferConfig.optionalName = "updater.zip";
        std::string bufString = SerialStruct::SerializeToString(transferConfig);
        const uint64_t realSize = static_cast<uint64_t>(1024 * 1024 * 1024) * 5;
        std::vector<uint8_t> buffer(sizeof(realSize) + bufString.size());
        int ret = memcpy_s(buffer.data(), buffer.size(), &realSize, sizeof(realSize));
        EXPECT_EQ(0, ret);
        ret = memcpy_s(buffer.data() + sizeof(realSize), buffer.size(), bufString.c_str(), bufString.size());
        EXPECT_EQ(0, ret);
        hdcDamon->CommandDispatch(CMD_UPDATER_CHECK, buffer.data(), buffer.size());

        // begin
        hdcDamon->CommandDispatch(CMD_UPDATER_BEGIN, NULL, 0);

        for (int i = 0; i < 10; i++) { // 10 send time
            HdcTransferBase::TransferPayload payloadHead {};
            payloadHead.compressType = HdcTransferBase::COMPRESS_NONE;
            payloadHead.uncompressSize = transferConfig.fileSize / 10; // 10 time
            payloadHead.compressSize = transferConfig.fileSize / 10; // 10 time
            payloadHead.index = 0;
            std::string bufData = SerialStruct::SerializeToString(payloadHead);
            hdcDamon->CommandDispatch(CMD_UPDATER_DATA, reinterpret_cast<uint8_t *>(bufData.data()), bufData.size());
        }
        // end
        hdcDamon->DoTransferFinish();
        return 0;
    }

    int TestHdcDaemonInvalid()
    {
        uv_loop_t loopMain;
        uv_loop_init(&loopMain);

        HTaskInfo hTaskInfo = nullptr;
        std::shared_ptr<TaskInformation> task = std::make_shared<TaskInformation>();
        if (task == nullptr) {
            return -1;
        }
        hTaskInfo = task.get();
        hTaskInfo->channelId = 2; // 2 channel id
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 1;
        std::shared_ptr<DaemonUpdater> hdcDamon = std::make_shared<DaemonUpdater>(hTaskInfo);
        if (hdcDamon == nullptr) {
            return -1;
        }
        // cmd: hdc flash partition packagename
        // check
        HdcTransferBase::TransferConfig transferConfig {};
        transferConfig.functionName = "aaaa";
        transferConfig.options = TEST_PARTITION_NAME;
        transferConfig.options += "  ";
        transferConfig.options += TEST_FLASH_IMAGE_NAME;
        std::string localPath = TEST_FLASH_IMAGE_NAME;
        transferConfig.fileSize = 1468006400; // 1468006400 file size
        WRITE_LOG(LOG_DEBUG, "CheckMaster %s", transferConfig.functionName.c_str());
        transferConfig.optionalName = "userdata.img";
        std::string bufString = SerialStruct::SerializeToString(transferConfig);
        const uint64_t realSize = static_cast<uint64_t>(1024 * 1024 * 1024) * 5;
        std::vector<uint8_t> buffer(sizeof(realSize) + bufString.size());
        int ret = memcpy_s(buffer.data(), buffer.size(), &realSize, sizeof(realSize));
        EXPECT_EQ(0, ret);
        ret = memcpy_s(buffer.data() + sizeof(realSize), buffer.size(), bufString.c_str(), bufString.size());
        EXPECT_EQ(0, ret);
        hdcDamon->CommandDispatch(CMD_UPDATER_CHECK, buffer.data(), buffer.size());
        return 0;
    }

    int TestHdcDaemonFlash()
    {
        uv_loop_t loopMain;
        uv_loop_init(&loopMain);

        HTaskInfo hTaskInfo = nullptr;
        std::shared_ptr<TaskInformation> task = std::make_shared<TaskInformation>();
        if (task == nullptr) {
            return -1;
        }
        hTaskInfo = task.get();
        hTaskInfo->channelId = 2; // 2 channel id
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 1;
        std::shared_ptr<DaemonUpdater> hdcDamon = std::make_shared<DaemonUpdater>(hTaskInfo);
        if (hdcDamon == nullptr) {
            return -1;
        }
        // cmd: hdc flash partition packagename
        // check
        HdcTransferBase::TransferConfig transferConfig {};
        transferConfig.functionName = "flash";
        transferConfig.options = TEST_PARTITION_NAME;
        transferConfig.options += "  ";
        transferConfig.options += TEST_FLASH_IMAGE_NAME;
        std::string localPath = TEST_FLASH_IMAGE_NAME;
        transferConfig.fileSize = 1468006400; // 1468006400 file size
        WRITE_LOG(LOG_DEBUG, "CheckMaster %s", transferConfig.functionName.c_str());
        transferConfig.optionalName = "userdata.img";
        std::string bufString = SerialStruct::SerializeToString(transferConfig);
        const uint64_t realSize = transferConfig.fileSize;
        std::vector<uint8_t> buffer(sizeof(realSize) + bufString.size());
        int ret = memcpy_s(buffer.data(), buffer.size(), &realSize, sizeof(realSize));
        EXPECT_EQ(0, ret);
        ret = memcpy_s(buffer.data() + sizeof(realSize), buffer.size(), bufString.c_str(), bufString.size());
        EXPECT_EQ(0, ret);
        hdcDamon->CommandDispatch(CMD_UPDATER_CHECK, buffer.data(), buffer.size());

        // begin
        hdcDamon->CommandDispatch(CMD_UPDATER_BEGIN, NULL, 0);

        HdcTransferBase::TransferPayload payloadHead {};
        for (int i = 0; i < 10; i++) { // 10 send data
            payloadHead.compressType = HdcTransferBase::COMPRESS_NONE;
            payloadHead.uncompressSize = transferConfig.fileSize / 10; // 10 time
            payloadHead.compressSize = transferConfig.fileSize / 10; // 10 time
            payloadHead.index = 0;
            std::string bufData = SerialStruct::SerializeToString(payloadHead);
            hdcDamon->CommandDispatch(CMD_UPDATER_DATA, reinterpret_cast<uint8_t *>(bufData.data()), bufData.size());
        }
        std::string bufData = SerialStruct::SerializeToString(payloadHead);
        hdcDamon->CommandDispatch(CMD_UPDATER_DATA, reinterpret_cast<uint8_t *>(bufData.data()), bufData.size());
        // end
        hdcDamon->DoTransferFinish();
        return 0;
    }

    int TestHdcDaemonErase()
    {
        uv_loop_t loopMain;
        uv_loop_init(&loopMain);

        HTaskInfo hTaskInfo = nullptr;
        std::shared_ptr<TaskInformation> task = std::make_shared<TaskInformation>();
        if (task == nullptr) {
            return -1;
        }
        hTaskInfo = task.get();
        hTaskInfo->channelId = 2; // 2 channel id
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 1;
        std::shared_ptr<DaemonUpdater> hdcDamon = std::make_shared<DaemonUpdater>(hTaskInfo);
        if (hdcDamon == nullptr) {
            return -1;
        }
        // cmd: hdc erase partition
        // check
        std::string bufString = "erase -f ";
        bufString += TEST_PARTITION_NAME;
        hdcDamon->CommandDispatch(CMD_UPDATER_ERASE, reinterpret_cast<uint8_t *>(bufString.data()), bufString.size());
        return 0;
    }

    int TestHdcDaemonFormat()
    {
        uv_loop_t loopMain;
        uv_loop_init(&loopMain);

        HTaskInfo hTaskInfo = nullptr;
        std::shared_ptr<TaskInformation> task = std::make_shared<TaskInformation>();
        if (task == nullptr) {
            return -1;
        }
        hTaskInfo = task.get();
        hTaskInfo->channelId = 2; // 2 channel id
        hTaskInfo->sessionId = 0;
        hTaskInfo->runLoop = &loopMain;
        hTaskInfo->serverOrDaemon = 1;
        std::shared_ptr<DaemonUpdater> hdcDamon = std::make_shared<DaemonUpdater>(hTaskInfo);
        if (hdcDamon == nullptr) {
            return -1;
        }
        // cmd: hdc format partition
        // check
        std::string bufString = "format -f ";
        bufString += TEST_PARTITION_NAME + "  -t ext4";
        hdcDamon->CommandDispatch(CMD_UPDATER_FORMAT, reinterpret_cast<uint8_t *>(bufString.data()), bufString.size());

        return 0;
    }
};

HWTEST_F(FLashServiceUnitTest, FLashServiceUnitTest, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestFindAllDevice());
}

HWTEST_F(FLashServiceUnitTest, TestDaemonUpdater, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestDaemonUpdater());
}

HWTEST_F(FLashServiceUnitTest, TestDoUpdater, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestDoUpdater());
}

HWTEST_F(FLashServiceUnitTest, TestHdcDaemonFlash, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestHdcDaemonFlash());
}

HWTEST_F(FLashServiceUnitTest, TestHdcDaemonInvalid, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestHdcDaemonInvalid());
}

HWTEST_F(FLashServiceUnitTest, TestHdcDaemonErase, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestHdcDaemonErase());
}

HWTEST_F(FLashServiceUnitTest, TestHdcDaemonFormat, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestHdcDaemonFormat());
}

HWTEST_F(FLashServiceUnitTest, TestFindAllDevice, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestFindAllDevice());
}

HWTEST_F(FLashServiceUnitTest, TestDoFlashPartition, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestDoFlashPartition());
}

HWTEST_F(FLashServiceUnitTest, TestDoFormatPartition, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestDoFormatPartition("data", "ext4"));
    EXPECT_EQ(0, test.TestDoFormatPartition("data", "f2fs"));
    EXPECT_EQ(0, test.TestDoFormatPartition("boot", "f2fs"));
}

HWTEST_F(FLashServiceUnitTest, TestDoErasePartition, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestDoErasePartition());
}

HWTEST_F(FLashServiceUnitTest, TestFlashServiceDoResizeParatiton, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestFlashServiceDoResizeParatiton());
}

HWTEST_F(FLashServiceUnitTest, TestFlashServiceDoPrepare, TestSize.Level1)
{
    FLashServiceUnitTest test;
    std::string cmdParam = "update ";
    EXPECT_EQ(0, test.TestFlashServiceDoPrepare(flashd::UPDATEMOD_UPDATE, cmdParam));

    cmdParam = "flash ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    cmdParam += TEST_FLASH_IMAGE_NAME;
    EXPECT_EQ(0, test.TestFlashServiceDoPrepare(flashd::UPDATEMOD_FLASH, cmdParam));

    cmdParam = "erase -f";
    cmdParam += TEST_PARTITION_NAME + "  ";
    EXPECT_EQ(0, test.TestFlashServiceDoPrepare(flashd::UPDATEMOD_ERASE, cmdParam));

    cmdParam = "format -f ";
    cmdParam += TEST_PARTITION_NAME + "  -t ext4";
    EXPECT_EQ(0, test.TestFlashServiceDoPrepare(flashd::UPDATEMOD_FORMAT, cmdParam));

    EXPECT_EQ(0, test.TestFlashServiceDoPrepare(flashd::UPDATEMOD_MAX, cmdParam));
}

HWTEST_F(FLashServiceUnitTest, TestFlashServiceDoFlash, TestSize.Level1)
{
    FLashServiceUnitTest test;
    std::string cmdParam = "update ";
    EXPECT_EQ(0, test.TestFlashServiceDoFlash(flashd::UPDATEMOD_UPDATE, cmdParam));

    cmdParam = "flash ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    cmdParam += TEST_FLASH_IMAGE_NAME;
    EXPECT_EQ(0, test.TestFlashServiceDoFlash(flashd::UPDATEMOD_FLASH, cmdParam));

    cmdParam = "erase -f ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    EXPECT_EQ(0, test.TestFlashServiceDoFlash(flashd::UPDATEMOD_ERASE, cmdParam));

    cmdParam = "format -f -t ext4 ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    EXPECT_EQ(0, test.TestFlashServiceDoFlash(flashd::UPDATEMOD_FORMAT, cmdParam));

    cmdParam = "format -f -t f2fs ";
    cmdParam += TEST_PARTITION_NAME + "  ";
    EXPECT_EQ(0, test.TestFlashServiceDoFlash(flashd::UPDATEMOD_FORMAT, cmdParam));

    EXPECT_EQ(0, test.TestFlashServiceDoFlash(flashd::UPDATEMOD_MAX, cmdParam));
}

HWTEST_F(FLashServiceUnitTest, TestFlashServiceDoFinish, TestSize.Level1)
{
    FLashServiceUnitTest test;
    EXPECT_EQ(0, test.TestFlashServiceDoFinish(flashd::UPDATEMOD_UPDATE));
    EXPECT_EQ(0, test.TestFlashServiceDoFinish(flashd::UPDATEMOD_FLASH));
    EXPECT_EQ(0, test.TestFlashServiceDoFinish(flashd::UPDATEMOD_MAX));
}

HWTEST_F(FLashServiceUnitTest, TestFlashdMain, TestSize.Level1)
{
    const char* argv1[] = {"TestFlashdMain", "-t"};
    flashd_main(sizeof(argv1) / sizeof(char*), const_cast<char**>(argv1));
    const char *argv2[] = {"TestFlashdMain", "-u", " -l5"};
    flashd_main(sizeof(argv2) / sizeof(char*), const_cast<char**>(argv2));
}
} // namespace