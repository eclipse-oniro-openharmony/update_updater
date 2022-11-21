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
    EXPECT_EQ(ret, true);
}

HWTEST_F(UpdaterUtilUnitTest, GetBootMode, TestSize.Level1)
{
    int mode = BOOT_UPDATER;
    int ret = GetBootMode(mode);
    EXPECT_EQ(ret, 0);
}
}