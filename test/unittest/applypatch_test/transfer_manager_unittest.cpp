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

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include "applypatch/transfer_manager.h"
#include "log/log.h"

using namespace testing::ext;
using namespace Updater;
using namespace std;
namespace UpdaterUt {
class TransferManagerUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
};

void TransferManagerUnitTest::SetUpTestCase()
{
    cout << "Updater Unit TransferManagerUnitTest Setup!" << endl;
}

void TransferManagerUnitTest::SetUp()
{
    cout << "Updater Unit TransferManagerUnitTest Begin!" << endl;
}

void TransferManagerUnitTest::TearDown()
{
    cout << "Updater Unit TransferManagerUnitTest End!" << endl;
}

HWTEST_F(TransferManagerUnitTest, transfer_manager_test_001, TestSize.Level1)
{
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    std::string cmd = "zero 2,0,1";
    tm->CheckResult(CommandResult::NEED_RETRY, cmd, CommandType::ZERO);
    TransferManager::ReleaseTransferManagerInstance(tm);
}

HWTEST_F(TransferManagerUnitTest, transfer_manager_test_002, TestSize.Level1)
{
    TransferManagerPtr tm = TransferManager::GetTransferManagerInstance();
    tm->ReloadForRetry();
    TransferManager::ReleaseTransferManagerInstance(tm);
}
} // updater_ut
