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
#include "updater/hwfault_retry.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "utils.h"

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
class HwfaultRetryUnitTest : public testing::Test {
public:
    HwfaultRetryUnitTest()
    {
        InitUpdaterLogger("UPDATER", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    }
    ~HwfaultRetryUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void)
    {
        (void)ClearMisc();
    }
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(HwfaultRetryUnitTest, VerifyFailRetry, TestSize.Level1)
{
    HwFaultRetry::GetInstance().SetFaultInfo(VERIFY_FAILED_REBOOT);
    HwFaultRetry::GetInstance().SetRetryCount(0);
    HwFaultRetry::GetInstance().DoRetryAction();

    bool ret = Utils::CheckFaultInfo(VERIFY_FAILED_REBOOT);
    EXPECT_EQ(ret, true);
}

HWTEST_F(HwfaultRetryUnitTest, IOFailRetry, TestSize.Level1)
{
    HwFaultRetry::GetInstance().SetFaultInfo(IO_FAILED_REBOOT);
    HwFaultRetry::GetInstance().SetRetryCount(0);
    HwFaultRetry::GetInstance().DoRetryAction();

    bool ret = Utils::CheckFaultInfo(IO_FAILED_REBOOT);
    EXPECT_EQ(ret, true);
}

HWTEST_F(HwfaultRetryUnitTest, RetryMoreThanMax, TestSize.Level1)
{
    HwFaultRetry::GetInstance().SetFaultInfo(IO_FAILED_REBOOT);
    HwFaultRetry::GetInstance().SetRetryCount(MAX_RETRY_COUNT);
    HwFaultRetry::GetInstance().DoRetryAction();

    bool ret = Utils::CheckFaultInfo(IO_FAILED_REBOOT);
    EXPECT_EQ(ret, true);
}
}