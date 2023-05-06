/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "update_processor_unittest.h"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <sys/mount.h>
#include <unistd.h>
#include "mount.h"
#include "store.h"
#include "unittest_comm.h"
#include "update_processor.h"
#include "script_manager.h"
#include "pkg_manager.h"
#include "ring_buffer.h"

using namespace Updater;
using namespace std;
using namespace Hpackage;
using namespace testing::ext;
using namespace Uscript;

namespace UpdaterUt {
constexpr const char *UT_MISC_PARTITION_NAME = "/misc";
constexpr const uint32_t UT_MISC_BUFFER_SIZE = 2048;
constexpr const uint32_t BUFFER_PUSH_TIMES = 3;
constexpr const uint32_t BUFFER_SIZE = 1024 * 1024 * 2;
void UpdateProcessorUnitTest::SetUp(void)

{
    cout << "Updater Unit UpdateProcessorUnitTest Begin!" << endl;

    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");

    /* create 2k size test file */
    string devPath = GetBlockDeviceByMountPoint(UT_MISC_PARTITION_NAME);
    vector<uint8_t> buffer(UT_MISC_BUFFER_SIZE, 0);
    auto ret = Store::WriteDataToStore("/", devPath, buffer, UT_MISC_BUFFER_SIZE);
    cout << "WriteDataToStore ret: " << ret << endl;
}

void UpdateProcessorUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdateProcessorUnitTest End!" << endl;

    /* delete 2k size test file */
    string devPath = GetBlockDeviceByMountPoint(UT_MISC_PARTITION_NAME);
    auto ret = Store::FreeStore("/", devPath);
    cout << "FreeStore ret: " << ret << endl;
}

// do something at the each function begining
void UpdateProcessorUnitTest::SetUpTestCase(void) {}

// do something at the each function end
void UpdateProcessorUnitTest::TearDownTestCase(void) {}

/* ota update, zip has 2k size misc.img */
HWTEST_F(UpdateProcessorUnitTest, UpdateProcessor_001, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/updater_write_misc_img.zip";
    int32_t ret = ProcessUpdater(false, STDOUT_FILENO, packagePath, GetTestCertName());
    EXPECT_EQ(ret, 0);
}

/* image diff update, zip has 2k size misc.img, base is zero, dst is urandom */
HWTEST_F(UpdateProcessorUnitTest, UpdateProcessor_002, TestSize.Level1)
{
    vector<uint8_t> buffer(UT_MISC_BUFFER_SIZE, 0);
    int32_t ret = Store::WriteDataToStore("/", GetBlockDeviceByMountPoint(UT_MISC_PARTITION_NAME),
        buffer, UT_MISC_BUFFER_SIZE);
    EXPECT_EQ(ret, 0);

    const string packagePath = "/data/updater/updater/updater_write_diff_misc_img.zip";
    ret = ProcessUpdater(false, STDOUT_FILENO, packagePath, GetTestCertName());
    EXPECT_EQ(ret, 0);
}

/* image diff update, zip has 2k size misc.img, base is zero, dst is urandom, hash check fail */
HWTEST_F(UpdateProcessorUnitTest, UpdateProcessor_003, TestSize.Level1)
{
    vector<uint8_t> buffer(UT_MISC_BUFFER_SIZE, 1);
    int32_t ret = Store::WriteDataToStore("/", GetBlockDeviceByMountPoint(UT_MISC_PARTITION_NAME),
        buffer, UT_MISC_BUFFER_SIZE);
    EXPECT_EQ(ret, 0);

    const string packagePath = "/data/updater/updater/updater_write_diff_misc_img.zip";
    ret = ProcessUpdater(false, STDOUT_FILENO, packagePath, GetTestCertName());
    EXPECT_EQ(ret, USCRIPT_INVALID_PARAM);
}

HWTEST_F(UpdateProcessorUnitTest, UpdateProcessor_004, TestSize.Level1)
{
    PkgBuffer buffer(BUFFER_SIZE);
    RingBuffer ringBuffer;
    bool ret = ringBuffer.Init(UScriptInstructionUpdateFromBin::STASH_BUFFER_SIZE, BUFFER_PUSH_TIMES);
    EXPECT_TRUE(ret);
    for (uint32_t i = 0; i < BUFFER_PUSH_TIMES; i++) {
        EXPECT_EQ(UScriptInstructionUpdateFromBin::UnCompressDataProducer(buffer,
            BUFFER_SIZE, 0, false, &ringBuffer), 0);
    }
    PkgBuffer emptyBuffer = {};
    EXPECT_EQ(UScriptInstructionUpdateFromBin::UnCompressDataProducer(emptyBuffer, 0, 0, true, &ringBuffer), 0);
    uint8_t recvBuffer[BUFFER_SIZE] {};
    uint32_t len = 0;
    ringBuffer.Pop(recvBuffer, BUFFER_SIZE, len);
    EXPECT_EQ(len, UScriptInstructionUpdateFromBin::STASH_BUFFER_SIZE);
    ringBuffer.Pop(recvBuffer, BUFFER_SIZE, len);
    EXPECT_EQ(len, BUFFER_SIZE);
}
} // namespace updater_ut
