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

#include "update_image_block_test.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <libgen.h>
#include <string>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "applypatch/block_set.h"
#include "applypatch/store.h"
#include "fs_manager/mount.h"
#include "log.h"
#include "package/pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "script_utils.h"
#include "unittest_comm.h"
#include "update_image_block.h"
#include "update_processor.h"
#include "utils.h"

using namespace Updater;
using namespace testing::ext;
using namespace Uscript;
using namespace std;
using namespace Hpackage;

namespace UpdaterUt {
void UpdateImageBlockTest::SetUp()
{
    cout << "Updater Unit UpdateBlockUnitTest Begin!" << endl;

    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
}

void UpdateImageBlockTest::TearDown()
{
    cout << "Updater Unit UpdateBlockUnitTest End!" << endl;
}

/* ota update, upd to base */
HWTEST_F(UpdateImageBlockTest, update_image_block_test_001, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/updater_write_miscblock_img.zip";
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    close(fd);
    int32_t ret = ProcessUpdater(false, STDOUT_FILENO, packagePath, GetTestCertName());
    EXPECT_EQ(ret, 0);
}

/* block diff update, hash check ok */
HWTEST_F(UpdateImageBlockTest, update_image_block_test_002, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/updater_write_diff_miscblock_img.zip";
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    close(fd);
    int32_t ret = ProcessUpdater(false, STDOUT_FILENO, packagePath, GetTestCertName());
    EXPECT_EQ(ret, 0);
}

/* block diff update, hash check fail */
HWTEST_F(UpdateImageBlockTest, update_image_block_test_003, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/updater_write_diff_miscblock_img.zip";
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    close(fd);
    int32_t ret = ProcessUpdater(false, STDOUT_FILENO, packagePath, GetTestCertName());
    EXPECT_EQ(ret, USCRIPT_INVALID_PARAM);
}
}
