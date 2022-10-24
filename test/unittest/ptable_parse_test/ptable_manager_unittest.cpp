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
        ASSERT_EQ(ret, -1);
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
}