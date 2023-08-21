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

#include <gtest/gtest.h>
#include "ptable_manager.h"
#include "ptable.h"

using namespace Updater;
using namespace testing;
using namespace testing::ext;

namespace {
class PtableManagerTest : public PtableManager {
public:
    PtableManagerTest() {}

    ~PtableManagerTest() {}

    void LoadPartitionInfo([[maybe_unused]] Hpackage::PkgManager *pkgManager = nullptr) override {}

    int32_t TestGetPartitionInfoIndexByName(const std::vector<Ptable::PtnInfo> &ptnInfo, const std::string &name)
    {
        return GetPartitionInfoIndexByName(ptnInfo, name);
    }

    bool TestIsPtableChanged(const std::vector<Ptable::PtnInfo> &devicePtnInfo,
        const std::vector<Ptable::PtnInfo> &pkgPtnInfo)
    {
        return IsPtableChanged(devicePtnInfo, pkgPtnInfo);
    }

    bool TestInitPtableManager()
    {
        return InitPtableManager();
    }

    void TestSetDeviceStorageType()
    {
        SetDeviceStorageType();
    }
    bool TestIsPartitionChanged(const std::vector<Ptable::PtnInfo> &devicePtnInfo,
        const std::vector<Ptable::PtnInfo> &pkgPtnInfo, const std::string &partitionName)
    {
        return IsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
    }
};

class UTestPtableManager : public ::testing::Test {
public:
    UTestPtableManager() {}

    ~UTestPtableManager() = default;

    void TestGetPartitionInfoIndexByName()
    {
        PtableManagerTest context {};
        std::vector<Ptable::PtnInfo> ptnInfo;
        Ptable::PtnInfo tmp;
        std::string name;
        int32_t ret = context.TestGetPartitionInfoIndexByName(ptnInfo, name);
        ASSERT_EQ(ret, -1);
        name = "TestGetPartitionInfoIndexByName";
        ret = context.TestGetPartitionInfoIndexByName(ptnInfo, name);
        ASSERT_EQ(ret, -1);
        tmp.dispName = name;
        ptnInfo.push_back(tmp);
        name = "";
        ret = context.TestGetPartitionInfoIndexByName(ptnInfo, name);
        ASSERT_EQ(ret, -1);
        name = "TestGetPartitionInfoIndexByName1";
        ret = context.TestGetPartitionInfoIndexByName(ptnInfo, name);
        ASSERT_EQ(ret, -1);
        name = "TestGetPartitionInfoIndexByName";
        ret = context.TestGetPartitionInfoIndexByName(ptnInfo, name);
        ASSERT_NE(ret, -1);
    }

    void TestIsPtableChanged()
    {
        PtableManagerTest context {};
        std::vector<Ptable::PtnInfo> devicePtnInfo;
        std::vector<Ptable::PtnInfo> pkgPtnInfo;
        bool ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, false);
        Ptable::PtnInfo tmp = {1, 1, {1}, 1, "TestIsPtableChanged"};
        pkgPtnInfo.push_back(tmp);
        ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, true);
        devicePtnInfo.push_back(tmp);
        ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, false);
        devicePtnInfo.push_back(tmp);
        ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, true);
        devicePtnInfo.pop_back();
        devicePtnInfo[0].startAddr = 0;
        ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, true);
        devicePtnInfo[0].startAddr = 1;
        devicePtnInfo[0].partitionSize = 0;
        ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, true);
        devicePtnInfo[0].partitionSize = 1;
        devicePtnInfo[0].dispName = "TestIsPtableChanged1";
        ret = context.TestIsPtableChanged(devicePtnInfo, pkgPtnInfo);
        ASSERT_EQ(ret, true);
        ret = context.WritePtableToDevice();
        ASSERT_EQ(ret, false);
    }

    void TestInitPtableManagerAndSetDeviceStorageType()
    {
        PtableManagerTest context {};
        bool ret = context.TestInitPtableManager();
        ASSERT_EQ(ret, false);
        context.TestSetDeviceStorageType();
        ASSERT_EQ(context.storage_, PtableManagerTest::StorageType::STORAGE_UFS);
        context.storage_ = PtableManagerTest::StorageType::STORAGE_EMMC;
        context.TestSetDeviceStorageType();
        ASSERT_EQ(context.storage_, PtableManagerTest::StorageType::STORAGE_EMMC);
    }

    void TestGetPartionInfoByName()
    {
        PtableManagerTest context {};
        std::string partitionName = "";
        Ptable::PtnInfo ptnInfo;
        context.pPtable_ = nullptr;
        bool ret = context.GetPartionInfoByName(partitionName, ptnInfo);
        ASSERT_EQ(ret, false);
        context.pPtable_ = std::make_unique<UfsPtable>();
        ret = context.GetPartionInfoByName(partitionName, ptnInfo);
        ASSERT_EQ(ret, false);
        ptnInfo.dispName = "testPartition";
        ret = context.GetPartionInfoByName(partitionName, ptnInfo);
        ASSERT_EQ(ret, false);
        partitionName = "testPartition";
        ret = context.GetPartionInfoByName(partitionName, ptnInfo);
        ASSERT_EQ(ret, false);
        ret = context.WritePartitionTable();
        ASSERT_EQ(ret, false);
    }

    void TestIsPartitionChanged()
    {
        PtableManagerTest context {};
        std::vector<Ptable::PtnInfo> devicePtnInfo;
        std::vector<Ptable::PtnInfo> pkgPtnInfo;
        std::string partitionName;
        bool ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, false);
        Ptable::PtnInfo ptnInfo = {0, 0, {0}, 0, "TestIsPartitionChangedForCheck"};
        pkgPtnInfo.push_back(ptnInfo);
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, true);
        devicePtnInfo.push_back(ptnInfo);
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, true);
        partitionName = "TestIsPartitionChanged";
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, true);
        ptnInfo.dispName = "TestIsPartitionChanged";
        devicePtnInfo.push_back(ptnInfo);
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, true);
        pkgPtnInfo.push_back(ptnInfo);
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, false);
        devicePtnInfo[1].startAddr = 1;
        devicePtnInfo[1].partitionSize = 1;
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, true);
        devicePtnInfo[1].startAddr = 0;
        ret = context.TestIsPartitionChanged(devicePtnInfo, pkgPtnInfo, partitionName);
        ASSERT_EQ(ret, true);
    }
protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(UTestPtableManager, TestGetPartitionInfoIndexByName, TestSize.Level1)
{
    UTestPtableManager {}.TestGetPartitionInfoIndexByName();
}

HWTEST_F(UTestPtableManager, TestIsPtableChanged, TestSize.Level1)
{
    UTestPtableManager {}.TestIsPtableChanged();
}

HWTEST_F(UTestPtableManager, TestInitPtableManagerAndSetDeviceStorageType, TestSize.Level1)
{
    UTestPtableManager {}.TestInitPtableManagerAndSetDeviceStorageType();
}

HWTEST_F(UTestPtableManager, TestGetPartionInfoByName, TestSize.Level1)
{
    UTestPtableManager {}.TestGetPartionInfoByName();
}

HWTEST_F(UTestPtableManager, TestIsPartitionChanged, TestSize.Level1)
{
    UTestPtableManager {}.TestIsPartitionChanged();
}
}