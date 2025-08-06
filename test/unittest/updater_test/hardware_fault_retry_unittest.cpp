/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <gtest/gtest.h>
#include <memory>
#include <sys/ioctl.h>
#include "log/log.h"
#include "securec.h"
#include "updater/hardware_fault_retry.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "utils.h"

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
class HardwareFaultRetryUnitTest : public testing::Test {
public:
    HardwareFaultRetryUnitTest()
    {
        InitUpdaterLogger("UPDATER", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    }
    ~HardwareFaultRetryUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown()
    {
        (void)ClearMisc();
    }
    void TestBody() {}
};

HWTEST_F(HardwareFaultRetryUnitTest, VerifyFailRetry, TestSize.Level1)
{
    HardwareFaultRetry::GetInstance().SetFaultInfo(VERIFY_FAILED_REBOOT);
    HardwareFaultRetry::GetInstance().SetRetryCount(0);
    HardwareFaultRetry::GetInstance().DoRetryAction();

    bool ret = Utils::CheckFaultInfo(VERIFY_FAILED_REBOOT);
    EXPECT_EQ(ret, true);
}

HWTEST_F(HardwareFaultRetryUnitTest, IOFailRetry, TestSize.Level1)
{
    HardwareFaultRetry::GetInstance().SetFaultInfo(IO_FAILED_REBOOT);
    HardwareFaultRetry::GetInstance().SetRetryCount(0);
    HardwareFaultRetry::GetInstance().DoRetryAction();

    bool ret = Utils::CheckFaultInfo(IO_FAILED_REBOOT);
    EXPECT_EQ(ret, true);
}

HWTEST_F(HardwareFaultRetryUnitTest, RetryMoreThanMax, TestSize.Level1)
{
    HardwareFaultRetry::GetInstance().SetFaultInfo(IO_FAILED_REBOOT);
    HardwareFaultRetry::GetInstance().SetRetryCount(MAX_RETRY_COUNT);
    HardwareFaultRetry::GetInstance().DoRetryAction();

    bool ret = Utils::CheckFaultInfo(IO_FAILED_REBOOT);
    EXPECT_EQ(ret, false);
}

HWTEST_F(HardwareFaultRetryUnitTest, SetEffectiveValueTest, TestSize.Level1)
{
    HardwareFaultRetry::GetInstance().SetEffectiveValue(false);
    HardwareFaultRetry::GetInstance().SetFaultInfo(VERIFY_FAILED_REBOOT);
    HardwareFaultRetry::GetInstance().SetRetryCount(0);
    HardwareFaultRetry::GetInstance().DoRetryAction();
 
    EXPECT_FALSE(Utils::CheckFaultInfo(VERIFY_FAILED_REBOOT));
}

HWTEST_F(HardwareFaultRetryUnitTest, RegisterAndRemoveTest, TestSize.Level1)
{
    HardwareFaultRetry::GetInstance().SetEffectiveValue(true);
    HardwareFaultRetry::GetInstance().RegisterDefaultFunc(PROCESS_BIN_FAIL_RETRY);
    HardwareFaultRetry::GetInstance().SetFaultInfo(PROCESS_BIN_FAIL_RETRY);
    HardwareFaultRetry::GetInstance().SetRetryCount(0);
    HardwareFaultRetry::GetInstance().DoRetryAction();
    EXPECT_EQ(HardwareFaultRetry::GetInstance().IsRetry(), true);

    EXPECT_EQ(Utils::CheckFaultInfo(PROCESS_BIN_FAIL_RETRY), true);

    (void)ClearMisc();
    HardwareFaultRetry::GetInstance().RemoveFunc(PROCESS_BIN_FAIL_RETRY);
    HardwareFaultRetry::GetInstance().SetFaultInfo(PROCESS_BIN_FAIL_RETRY);
    HardwareFaultRetry::GetInstance().SetRetryCount(0);
    HardwareFaultRetry::GetInstance().DoRetryAction();

    EXPECT_EQ(Utils::CheckFaultInfo(PROCESS_BIN_FAIL_RETRY), false);
}
}