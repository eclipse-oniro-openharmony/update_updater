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

using namespace Updater;
using namespace std;
using namespace Hpackage;
using namespace testing::ext;
using namespace Uscript;

namespace UpdaterUt {
constexpr const char *UT_MISC_PARTITION_NAME = "/misc";
constexpr const uint32_t UT_MISC_BUFFER_SIZE = 2048;

void UpdateProcessorUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdateProcessorUnitTest Begin!" << endl;

    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");

    /* create 2k size test file */
    string devPath = GetBlockDeviceByMountPoint(UT_MISC_PARTITION_NAME);
    vector<uint8_t> buffer(UT_MISC_BUFFER_SIZE, 0);
    auto ret = Store::WriteDataToStore("/", devPath, buffer, UT_MISC_BUFFER_SIZE);
    printf("WriteDataToStore ret: %d\n", ret);
}

void UpdateProcessorUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdateProcessorUnitTest End!" << endl;

    /* delete 2k size test file */
    string devPath = GetBlockDeviceByMountPoint(UT_MISC_PARTITION_NAME);
    auto ret = Store::FreeStore("/", devPath);
    printf("FreeStore ret: %d\n", ret);
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

/* diff update, zip has 2k size misc.img, base is zero, dst is urandom */
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

/* diff update, zip has 2k size misc.img, base is zero, dst is urandom, hash check fail */
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
} // namespace updater_ut
