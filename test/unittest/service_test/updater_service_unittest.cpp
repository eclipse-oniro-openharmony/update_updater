/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "updater/updater_const.h"
#include "updater/updater.h"
#include "fs_manager/mount.h"
#include "misc_info/misc_info.h"
#include "updater_main.h"

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
constexpr uint32_t MAX_ARG_SIZE = 24;
class UpdaterUtilUnitTest : public testing::Test {
public:
    UpdaterUtilUnitTest()
    {
        std::cout<<"UpdaterUtilUnitTest()";
    }
    ~UpdaterUtilUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(UpdaterUtilUnitTest, DeleteUpdaterPath, TestSize.Level1)
{
    std::string path = "/data/test/test/test";
    bool ret = DeleteUpdaterPath(path);
    EXPECT_EQ(ret, true);

    path = "/data/test";
    ret = DeleteUpdaterPath(path);
    EXPECT_EQ(ret, true);
}

HWTEST_F(UpdaterUtilUnitTest, ClearMisc, TestSize.Level1)
{
    bool ret = ClearMisc();
    EXPECT_EQ(ret, true);
}

HWTEST_F(UpdaterUtilUnitTest, IsSDCardExist, TestSize.Level1)
{
    std::string sdcardStr = "";
    bool ret = IsSDCardExist(sdcardStr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(UpdaterUtilUnitTest, GetBootMode, TestSize.Level1)
{
    int mode = BOOT_UPDATER;
    int ret = GetBootMode(mode);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UpdaterUtilUnitTest, ParseParams, TestSize.Level1)
{
    UpdateMessage boot {};
    std::string commandMsg = "";
    std::string updateMsg = "";
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()), 0);
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    std::string str = "./UpdaterMain";
    EXPECT_EQ(strncpy_s(argv[0], MAX_ARG_SIZE, str.c_str(), str.size()), 0);
    int argc = 1;
    std::vector<std::string> args = ParseParams(argc, argv);
    std::string res = "";
    for (auto s : args) {
        res += s;
    }
    EXPECT_EQ("./UpdaterMain", res);

    commandMsg = "boot_updater";
    updateMsg = "--update_package=updater_full.zip";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()), 0);
    bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);

    args = ParseParams(argc, argv);
    res = "";
    for (auto s : args) {
        res += s;
    }
    EXPECT_EQ("./UpdaterMain--update_package=updater_full.zip", res);
}
}