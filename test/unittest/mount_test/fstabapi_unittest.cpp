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

#include "fstabapi_unittest.h"
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sys/mount.h>
#include <unistd.h>
#include <vector>
#include "fs_manager/fs_manager.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "utils.h"

using namespace testing::ext;
using namespace UpdaterUt;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
void FstabApiUnitTest::SetUp(void)
{
    cout << "Updater Unit MountUnitTest Begin!" << endl;
}

// end
void FstabApiUnitTest::TearDown(void)
{
    cout << "Updater Unit MountUnitTest End!" << endl;
}

// do something at the each function begining
void FstabApiUnitTest::SetUpTestCase(void) {}

// do something at the each function end
void FstabApiUnitTest::TearDownTestCase(void) {}

HWTEST_F(FstabApiUnitTest, ReadFstabFromFile_unitest, TestSize.Level1)
{
    Fstab *fstab = nullptr;
    const std::string fstabFile1 = "/data/fstab.updater1";
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile2 = "/data/updater/mount_unitest/ReadFstabFromFile1.fstable";
    fstab = ReadFstabFromFile(fstabFile2.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile3 = "/data/updater/mount_unitest/ReadFstabFromFile2.fstable";
    fstab = ReadFstabFromFile(fstabFile3.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile4 = "/data/updater/mount_unitest/ReadFstabFromFile3.fstable";
    fstab = ReadFstabFromFile(fstabFile4.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile5 = "/data/updater/mount_unitest/ReadFstabFromFile4.fstable";
    fstab = ReadFstabFromFile(fstabFile5.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile6 = "/data/updater/mount_unitest/ReadFstabFromFile5.fstable";
    fstab = ReadFstabFromFile(fstabFile6.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    ReleaseFstab(fstab);
}

HWTEST_F(FstabApiUnitTest, FindFstabItemForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/FindFstabItemForPath1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    FstabItem* item = nullptr;
    const std::string path1 = "";
    item = FindFstabItemForPath(*fstab, path1.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path2 = "/data";
    item = FindFstabItemForPath(*fstab, path2.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    const std::string path3 = "/data2";
    item = FindFstabItemForPath(*fstab, path3.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path4 = "/data2/test";
    item = FindFstabItemForPath(*fstab, path4.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    ReleaseFstab(fstab);
    fstab = nullptr;
}

HWTEST_F(FstabApiUnitTest, FindFstabItemForMountPoint_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/FindFstabItemForMountPoint1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    FstabItem* item = nullptr;
    const std::string mp1 = "/data";
    const std::string mp2 = "/data2";
    item = FindFstabItemForMountPoint(*fstab, mp2.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string mp3 = "/data";
    item = FindFstabItemForMountPoint(*fstab, mp3.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    ReleaseFstab(fstab);
    fstab = nullptr;
}

HWTEST_F(FstabApiUnitTest, GetMountFlags_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/GetMountFlags1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    struct FstabItem* item = nullptr;
    const std::string mp = "/hos";
    item = FindFstabItemForMountPoint(*fstab, mp.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const int bufferSize = 512;
    char fsSpecificOptions[bufferSize] = {0};
    unsigned long flags = GetMountFlags(item->mountOptions, fsSpecificOptions, bufferSize, mp.c_str());
    EXPECT_EQ(flags, static_cast<unsigned long>(MS_NOSUID | MS_NODEV | MS_NOATIME));
    ReleaseFstab(fstab);
    fstab = nullptr;
}
} // namespace updater_ut
