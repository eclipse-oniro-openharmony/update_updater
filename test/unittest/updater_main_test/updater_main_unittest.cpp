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

#include "updater_main_unittest.h"
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include "fs_manager/mount.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "securec.h"
#include "updater_main.h"
#include "updater_ui.h"
#include "updater/updater.h"
#include "utils.h"

using namespace Updater;
using namespace testing::ext;
using namespace std;
using namespace Updater::Utils;

namespace UpdaterUt {
constexpr uint32_t MAX_ARG_SIZE = 10;
const std::string MISC_FILE = "/dev/block/platform/soc/10100000.himci.eMMC/by-name/misc";
// do something at the each function begining
void UpdaterMainUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdaterMainUnitTest Begin!" << endl;
}

// do something at the each function end
void UpdaterMainUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdaterMainUnitTest End!" << endl;
}

// init
void UpdaterMainUnitTest::SetUpTestCase(void)
{
    cout << "SetUpTestCase" << endl;
    Updater::Utils::MkdirRecursive("/data/sdcard/updater", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    LoadSpecificFstab("/data/updater/main_data/updater.tab");
    if (MountForPath("/data") != 0) {
        cout << "MountForPath failed" << endl;
    }
    PostUpdater(true);
}

// end
void UpdaterMainUnitTest::TearDownTestCase(void)
{
    cout << "TearDownTestCase" << endl;
    unlink("/data/updater.log");
    unlink("/data/updater_stage.log");
}

HWTEST_F(UpdaterMainUnitTest, updater_main_test_001, TestSize.Level1)
{
    UpdateMessage boot {};
    if (access("/data/updater/", 0)) {
        int ret = mkdir("/data/updater/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        ASSERT_EQ(ret, 0);
    }
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);
    const std::string commandMsg = "boot_updater";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update), "", sizeof(boot.update)), 0);
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    EXPECT_EQ(strncpy_s(argv[0], MAX_ARG_SIZE, "./main", MAX_ARG_SIZE), 0);
    int argc = 1;
    std::vector<std::string> args = ParseParams(argc, argv);

    EXPECT_EQ(args.size(), static_cast<unsigned int>(argc));
    PostUpdater(true);
    delete argv[0];
    delete []argv;
}

HWTEST_F(UpdaterMainUnitTest, updater_main_test_002, TestSize.Level1)
{
    UpdateMessage boot {};
    const std::string command1 = "boot_updater";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, command1.c_str(), command1.size()), 0);
    const std::string command2 = "--user_wipe_data";
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, command2.c_str(), command2.size()), 0);
    bool ret = WriteUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);

    int fRet = FactoryReset(USER_WIPE_DATA, "/misc/factory_test");
    EXPECT_EQ(fRet, 0);
    PostUpdater(true);
}

HWTEST_F(UpdaterMainUnitTest, updater_main_test_003, TestSize.Level1)
{
    const std::string sLog = "/data/updater/main_data/updater.tab";
    const std::string dLog = "/data/updater/main_data/ut_dLog.txt";
    bool ret = CopyUpdaterLogs(sLog, dLog);
    EXPECT_EQ(ret, true);
    unlink(dLog.c_str());
}

HWTEST_F(UpdaterMainUnitTest, updater_main_test_004, TestSize.Level1)
{
    UpdaterUiInit();
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen("/data/updater/updater.zip", "wb"), fclose);
    EXPECT_NE(fp, nullptr);

    UpdateMessage boot {};
    const std::string command1 = "boot_updater";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, command1.c_str(), command1.size()), 0);
    const std::string command2 = "--update_package=/data/updater/updater.zip\n--retry_count=0";
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, command2.c_str(), command2.size()), 0);
    bool ret = WriteUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);

    char **argv = new char*[MAX_ARG_SIZE];
    argv[0] = new char[10];
    EXPECT_EQ(strncpy_s(argv[0], MAX_ARG_SIZE, "./main", MAX_ARG_SIZE), 0);
    int argc = 1;
    int lRet = UpdaterMain(argc, argv);
    EXPECT_EQ(lRet, 0);

    EXPECT_EQ(memset_s(boot.update, sizeof(boot.update), 0, sizeof(boot.update)), 0);
    const std::string command3 = "--user_wipe_data";
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, command3.c_str(), command3.size()), 0);
    ret = WriteUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);
    lRet = UpdaterMain(argc, argv);
    EXPECT_EQ(lRet, 0);

    EXPECT_EQ(memset_s(boot.update, sizeof(boot.update), 0, sizeof(boot.update)), 0);
    const std::string command4 = "--factory_wipe_data";
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, command4.c_str(), command4.size()), 0);
    ret = WriteUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);
    lRet = UpdaterMain(argc, argv);
    EXPECT_EQ(lRet, 0);

    ret = ReadUpdaterMessage(MISC_FILE, boot);
    EXPECT_EQ(ret, true);
    EXPECT_STREQ(boot.update, "");
    delete argv[0];
    delete []argv;
    DeleteView();
}

HWTEST_F(UpdaterMainUnitTest, updater_main_test_compress, TestSize.Level1)
{
    const std::string testFile = "/data/sdcard/updater/test_compress.txt";
    FILE *fp = fopen(testFile.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fclose(fp);
    CompressLogs(testFile);
}
} // namespace updater_ut
