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

#include "misc_info_unittest.h"
#include <iostream>
#include <unistd.h>
#include "misc_info/misc_info.h"
#include "securec.h"

using namespace testing::ext;
using namespace updater_ut;
using namespace updater;
using namespace std;

namespace updater_ut {
const std::string MISC_FILE = "/data/updater/misc_ut";

void MiscInfoUnitTest::SetUpTestCase(void)
{
    cout << "Updater Unit MiscInfoUnitTest Begin!" << endl;
}

void MiscInfoUnitTest::TearDownTestCase(void)
{
    cout << "Updater Unit MiscInfoUnitTest End!" << endl;
}

TEST(MiscInfoUnitTest, misc_info_test_001)
{
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(MISC_FILE.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);

    UpdateMessage boot {};
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command), "boot_updater", sizeof(boot.command)), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update),
        "--update_package=./updater/xxx.zip\n--retry_count=1", sizeof(boot.update)), 0);
    bool ret = WriteUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);

    ret = ReadUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);
    unlink(MISC_FILE.c_str());
}
} // namespace updater_ut
