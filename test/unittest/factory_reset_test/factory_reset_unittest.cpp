/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#define private public
#include "factory_reset.h"
#undef private

using namespace Updater;
using namespace testing::ext;

namespace {
class FactoryResetUnitTest : public testing::Test {
public:
    FactoryResetUnitTest()
    {
        std::cout<<"FactoryResetUnitTest()";
    }
    ~FactoryResetUnitTest() {}
    FactoryResetProcess* factoryResetProcess;
    std::function<int(FactoryResetMode)> CommonResetPostFunc_;

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp()
    {
        factoryResetProcess = new FactoryResetProcess();
        factoryResetProcess->CommonResetPostFunc_ = CommonResetPostFunc_;
    }
    void TearDown()
    {
        delete factoryResetProcess;
        factoryResetProcess = nullptr;
    }
    void TestBody() {}
};

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc01, TestSize.Level0)
{
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::INVALID_MODE, "/data");
    EXPECT_EQ(ret, 1);
}

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc02, TestSize.Level0)
{
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::USER_WIPE_DATA, "/data");
    EXPECT_EQ(ret, 1);
}

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc03, TestSize.Level0)
{
    CommonResetPostFunc_ = [](FactoryResetMode) { return 0; };
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::USER_WIPE_DATA, "/data");
    EXPECT_EQ(ret, 1);
}

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc04, TestSize.Level0)
{
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::FACTORY_WIPE_DATA, "/data");
    EXPECT_EQ(ret, 1);
}

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc05, TestSize.Level0)
{
    CommonResetPostFunc_ = [](FactoryResetMode) { return 0; };
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::FACTORY_WIPE_DATA, "/data");
    EXPECT_EQ(ret, 1);
}

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc06, TestSize.Level0)
{
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::MENU_WIPE_DATA, "/data");
    EXPECT_EQ(ret, 1);
}

HWTEST_F(FactoryResetUnitTest, FactoryResetFunc07, TestSize.Level0)
{
    CommonResetPostFunc_ = [](FactoryResetMode) { return 0; };
    int ret = factoryResetProcess->GetInstance().DoFactoryReset(FactoryResetMode::MENU_WIPE_DATA, "/data");
    EXPECT_EQ(ret, 1);
}
}