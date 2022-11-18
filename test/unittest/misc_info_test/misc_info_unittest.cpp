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
using namespace UpdaterUt;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
const std::string MISC_FILE = "/data/updater/misc_ut";

void MiscInfoUnitTest::SetUpTestCase(void)
{
    cout << "Updater Unit MiscInfoUnitTest Begin!" << endl;
}

void MiscInfoUnitTest::TearDownTestCase(void)
{
    cout << "Updater Unit MiscInfoUnitTest End!" << endl;
}

HWTEST_F(MiscInfoUnitTest, misc_info_test_001, TestSize.Level1)
{
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(MISC_FILE.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);

    UpdateMessage boot {};
    const std::string command1 = "boot_updater";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, command1.c_str(), command1.size()), 0);
    const std::string command2 = "--update_package=./updater/xxx.zip\n--retry_count=1";
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, command2.c_str(), command2.size()), 0);
    std::string path = "";
    bool ret = WriteUpdaterMessage(path, boot);
    EXPECT_EQ(ret, false);

    path = MISC_FILE;
    ret = WriteUpdaterMessage(path, boot);
    EXPECT_EQ(ret, true);

    path = "";
    ret = ReadUpdaterMessage(path, boot);
    EXPECT_EQ(ret, false);
    unlink(path.c_str());

    path = MISC_FILE;
    ret = ReadUpdaterMessage(path, boot);
    EXPECT_EQ(ret, true);

    ret = WriteUpdaterMiscMsg(boot);
    EXPECT_EQ(ret, true);

    ret = ReadUpdaterMiscMsg(boot);
    EXPECT_EQ(ret, true);
}
} // namespace updater_ut
