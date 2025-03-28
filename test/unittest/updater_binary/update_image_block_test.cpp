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
    int pfd[2]; // 2: pipe read, pipe write
    int ret = pipe(pfd);
    EXPECT_GE(ret, 0);
    ret = ProcessUpdater(false, pfd[1], packagePath, GetTestCertName());
    close(pfd[0]);
    EXPECT_EQ(ret, 0);
}

/* block diff update, hash check fail */
HWTEST_F(UpdateImageBlockTest, update_image_block_test_003, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/updater_write_diff_miscblock_img.zip";
    int pfd[2]; // 2: pipe read, pipe write
    int ret = pipe(pfd);
    EXPECT_GE(ret, 0);
    ret = ProcessUpdater(false, pfd[1], packagePath, GetTestCertName());
    close(pfd[0]);
    EXPECT_EQ(ret, USCRIPT_INVALID_PARAM);
}

HWTEST_F(UpdateImageBlockTest, update_image_block_test_004, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/updater_diff_misc_verify_err.zip";
    int pfd[2]; // 2: pipe read, pipe write
    int ret = pipe(pfd);
    EXPECT_GE(ret, 0);
    ret = ProcessUpdater(false, pfd[1], packagePath, GetTestCertName());
    close(pfd[0]);
    EXPECT_NE(ret, 0);
}
}
