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
class PtableTest : public Ptable {
public:
    PtableTest() {}

    ~PtableTest() {}

    bool ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize) override
    {
        return true;
    }

    bool LoadPtableFromDevice() override
    {
        return true;
    }

    bool WritePartitionTable() override
    {
        return true;
    }

    bool TestGetPartionInfoByName(const std::string &partitionName, PtnInfo &ptnInfo, int32_t &index)
    {
        return GetPartionInfoByName(partitionName, ptnInfo, index);
    }

    int TestCalculateCrc32(const uint8_t *buffer, const uint32_t len)
    {
        return static_cast<int>(CalculateCrc32(buffer, len));
    }

    void ChangePartitionInfo(std::vector<PtnInfo> partitionInfo)
    {
        partitionInfo_ = partitionInfo;
    }
};

class UTestPtable : public ::testing::Test {
public:
    UTestPtable() {}

    ~UTestPtable() = default;

    void TestGetPartionInfoByName()
    {
        PtableTest ptableTest {};
        std::string partionName = "";
        int32_t index = 0;
        PtableTest::PtnInfo ptnInfo;
        bool ret = ptableTest.TestGetPartionInfoByName(partionName, ptnInfo, index);
        ASSERT_EQ(ret, false);
        std::vector<PtableTest::PtnInfo> partionInfo;
        ptnInfo.dispName = "updater";
        partionInfo.push_back(ptnInfo);
        ptableTest.ChangePartitionInfo(partionInfo);
        ret = ptableTest.TestGetPartionInfoByName(partionName, ptnInfo, index);
        ASSERT_EQ(ret, false);
        partionName = "updater";
        ret = ptableTest.TestGetPartionInfoByName(partionName, ptnInfo, index);
        ASSERT_NE(ret, false);
    }

    void TestCalculateCrc32()
    {
        PtableTest ptableTest {};
        uint8_t buffer[128] = {1}; // 128 : set buffer size
        uint8_t *nullBuffer = nullptr;
        uint32_t len = 0;
        int ret = ptableTest.TestCalculateCrc32(buffer, len);
        EXPECT_EQ(ret, 0);
        ret = ptableTest.TestCalculateCrc32(nullBuffer, len);
        EXPECT_EQ(ret, 0);
        len = 1;
        ret = ptableTest.TestCalculateCrc32(nullBuffer, len);
        EXPECT_EQ(ret, 0);
        ret = ptableTest.TestCalculateCrc32(buffer, len);
        EXPECT_EQ(ret, -1526341861); // -1526341861 : CalculateCrc32's result;
    }
protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(UTestPtable, TestGetPartionInfoByName, TestSize.Level1)
{
    UTestPtable {}.TestGetPartionInfoByName();
}

HWTEST_F(UTestPtable, TestCalculateCrc32, TestSize.Level1)
{
    UTestPtable {}.TestCalculateCrc32();
}
}