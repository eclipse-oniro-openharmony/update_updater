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
#include "applypatch/partition_record.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "package/pkg_manager.h"
#include "updater/updater.h"

using namespace testing::ext;
using namespace updater;
using namespace std;
namespace updater_ut {
class PartitionUpdateRecordUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
};

void PartitionUpdateRecordUnitTest::SetUpTestCase()
{
    LoadSpecificFstab("/data/updater/applypatch/etc/partition.tab");
}

void PartitionUpdateRecordUnitTest::SetUp()
{
    PartitionRecord::GetInstance().ClearRecordPartitionOffset();
}

void PartitionUpdateRecordUnitTest::TearDown()
{
    PartitionRecord::GetInstance().ClearRecordPartitionOffset();
}

HWTEST_F(PartitionUpdateRecordUnitTest, partition_record_test_001, TestSize.Level1)
{
    const std::string partitionName = "ut_partition";
    bool ret = PartitionRecord::GetInstance().RecordPartitionUpdateStatus(partitionName, true);
    EXPECT_EQ(ret, true);

    ret = PartitionRecord::GetInstance().IsPartitionUpdated(partitionName);
    EXPECT_EQ(ret, true);
}

HWTEST_F(PartitionUpdateRecordUnitTest, partition_record_test_002, TestSize.Level1)
{
    const std::string partitionName = "ut_partition1";
    bool ret = PartitionRecord::GetInstance().IsPartitionUpdated(partitionName);
    EXPECT_EQ(ret, false);
}

HWTEST_F(PartitionUpdateRecordUnitTest, partition_record_test_003, TestSize.Level1)
{
    string partitionName = "partitionName";
    for (int i = 0; i < MAX_PARTITION_NUM; i++) {
        bool ret = PartitionRecord::GetInstance().RecordPartitionUpdateStatus(partitionName, true);
        EXPECT_EQ(ret, true);
        ret = PartitionRecord::GetInstance().IsPartitionUpdated(partitionName);
        EXPECT_EQ(ret, true);
        partitionName += "a";
    }
    bool ret = PartitionRecord::GetInstance().RecordPartitionUpdateStatus(partitionName, true);
    EXPECT_EQ(ret, false);
}
} // updater_ut
