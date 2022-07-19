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
#include "cmp_partition.h"
#include "log/log.h"
#include "mount.h"
#include "partition_const.h"
#include "partitions.h"
#include "securec.h"

using namespace Updater;
using namespace testing::ext;
using namespace std;

namespace UpdaterUt {
constexpr int PARTITION_NUMBER = 9;
constexpr size_t BOOT_PARTITION_LEN = 2048;
constexpr size_t KERNEL_PARTITION_LEN = 30720;
constexpr size_t UPDATER_PARTITION_LEN = 40960;
constexpr size_t MISC_PARTITION_LEN = 2048;
constexpr size_t SYSTEM_PARTITION_LEN = 3627008;
constexpr size_t HOS_PARTITION_LEN = 3133440;
constexpr size_t VENDOR_PARTITION_LEN = 3133440;
constexpr size_t DATA_PARTITION_LEN = 3133440;
constexpr size_t XXX_PARTITION_LEN = 2998272;
constexpr size_t BUFFER_SIZE = 100;

class DoPartitionUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
};

void DoPartitionUnitTest::SetUpTestCase()
{
    cout << "Updater Unit allCmdUnitTest Setup!" << endl;
}

void DoPartitionUnitTest::SetUp()
{
    cout << "Updater Unit allCmdUnitTest Begin!" << endl;
}

void DoPartitionUnitTest::TearDown()
{
    cout << "Updater Unit allCmdUnitTest End!" << endl;
}

static void InitEmmcPartition(struct Partition &part, const std::string &partName, size_t start, size_t length)
{
    part.partName = partName;
    part.start = start;
    part.length = length;
    // Paramters below just give a random values, DoPartition will ignore the values.
    part.devName = "mmcblk0px";
    part.fsType = "emmc";
}

HWTEST_F(DoPartitionUnitTest, do_partition_test_001, TestSize.Level1)
{
    PartitonList nList;
    int partitionIndex = 0;
    struct Partition myPaty[PARTITION_NUMBER];
    EXPECT_EQ(memset_s(myPaty, sizeof(struct Partition) * PARTITION_NUMBER, 0,
        sizeof(struct Partition) * PARTITION_NUMBER), 0);
    size_t bootPartitionStart = 0;
    InitEmmcPartition(myPaty[partitionIndex++], "boot", bootPartitionStart, BOOT_PARTITION_LEN);

    size_t kernelPartitionStart = bootPartitionStart + BOOT_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "kernel", bootPartitionStart, BOOT_PARTITION_LEN);

    size_t updaterPartitionStart = kernelPartitionStart + KERNEL_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "updater", updaterPartitionStart, UPDATER_PARTITION_LEN);

    size_t miscPartitionStart = updaterPartitionStart + UPDATER_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "misc", miscPartitionStart, MISC_PARTITION_LEN);

    size_t systemPartitionStart = miscPartitionStart + MISC_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "system", systemPartitionStart, SYSTEM_PARTITION_LEN);

    size_t hosPartitionStart = systemPartitionStart + SYSTEM_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "hos", hosPartitionStart, HOS_PARTITION_LEN);

    size_t vendorPartitionStart = hosPartitionStart + HOS_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "vendor", vendorPartitionStart, VENDOR_PARTITION_LEN);

    size_t dataPartitionStart = vendorPartitionStart + VENDOR_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex++], "userdata", dataPartitionStart, DATA_PARTITION_LEN);

    for (int i = 0; i < partitionIndex; i++) {
        nList.push_back(&myPaty[i]);
    }

    std::string fstabPath = "/data/updater/updater/fstab.updater";
    LoadSpecificFstab(fstabPath);
    int ret = DoPartitions(nList);
    ASSERT_GT(ret, 0);

    PartitonList olist;
    size_t xxxPartitionStart = dataPartitionStart + XXX_PARTITION_LEN;
    InitEmmcPartition(myPaty[partitionIndex], "xxxxxx", xxxPartitionStart, XXX_PARTITION_LEN);
    olist.push_back(&myPaty[partitionIndex]);
    int ret1 = RegisterUpdaterPartitionList(nList, olist);
    ASSERT_EQ(ret1, 1);

    char aaa[BUFFER_SIZE] = {0};
    BlockDevice myDev {};
    myDev.devPath = "xxxxxx";
    myDev.specific = (void *)aaa;
    SetBlockDeviceMode(myDev);
}
} // updater_ut
