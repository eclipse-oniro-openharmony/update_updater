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

#include "updaterkits_unittest.h"
#include <iostream>
#include <unistd.h>
#include "securec.h"
#include "updaterkits/updaterkits.h"

using namespace testing::ext;
using namespace UpdaterUt;
using namespace std;

namespace UpdaterUt {
const std::string MISC_FILE = "/data/updater/misc_ut";

void UpdaterKitsUnitTest::SetUpTestCase(void)
{
    cout << "Updater Unit UpdaterKitsUnitTest Begin!" << endl;
}

void UpdaterKitsUnitTest::TearDownTestCase(void)
{
    cout << "Updater Unit UpdaterKitsUnitTest End!" << endl;
}

HWTEST_F(UpdaterKitsUnitTest, updater_kits_test01, TestSize.Level1)
{
    const std::vector<std::string> packageName1 = {""};
    bool ret = RebootAndInstallUpgradePackage(MISC_FILE, packageName1);
    EXPECT_EQ(ret, false);

    const std::vector<std::string> packageName2 = {"/data/updater/updater/updater_without_updater_binary.zip"};
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(MISC_FILE.c_str(), "wb+"), fclose);
    EXPECT_NE(fp, nullptr);
    ret = RebootAndInstallUpgradePackage(MISC_FILE, packageName2);
    EXPECT_EQ(ret, true);
    unlink(MISC_FILE.c_str());
}

HWTEST_F(UpdaterKitsUnitTest, updater_kits_test02, TestSize.Level1)
{
    const std::string cmd1 = "";
    bool ret = RebootAndCleanUserData(MISC_FILE, cmd1);
    EXPECT_EQ(ret, false);

    const std::string cmd2 = "--user_wipe_data";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(MISC_FILE.c_str(), "wb+"), fclose);
    EXPECT_NE(fp, nullptr);
    ret = RebootAndCleanUserData(MISC_FILE, cmd2);
    EXPECT_EQ(ret, true);
    unlink(MISC_FILE.c_str());
}
} // namespace updater_ut
