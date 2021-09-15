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
#include "fs_manager/fstab.h"
#include "fs_manager/fstab_api.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "utils.h"

using namespace testing::ext;
using namespace updater_ut;
using namespace updater;
using namespace std;

namespace updater_ut {
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
    Fstab fstab;
    const std::string fstabFile1 = "/data/fstab.updater1";
    bool ret = ReadFstabFromFile(fstabFile1, fstab);
    EXPECT_FALSE(ret);
    const std::string fstabFile2 = "/data/updater/mount_unitest/ReadFstabFromFile1.fstable";
    ret = ReadFstabFromFile(fstabFile2, fstab);
    EXPECT_FALSE(ret);
    const std::string fstabFile3 = "/data/updater/mount_unitest/ReadFstabFromFile2.fstable";
    ret = ReadFstabFromFile(fstabFile3, fstab);
    EXPECT_FALSE(ret);
    const std::string fstabFile4 = "/data/updater/mount_unitest/ReadFstabFromFile3.fstable";
    ret = ReadFstabFromFile(fstabFile4, fstab);
    EXPECT_FALSE(ret);
    const std::string fstabFile5 = "/data/updater/mount_unitest/ReadFstabFromFile4.fstable";
    ret = ReadFstabFromFile(fstabFile5, fstab);
    EXPECT_FALSE(ret);
    const std::string fstabFile6 = "/data/updater/mount_unitest/ReadFstabFromFile5.fstable";
    ret = ReadFstabFromFile(fstabFile6, fstab);
    EXPECT_TRUE(ret);
}

HWTEST_F(FstabApiUnitTest, FindFstabItemForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/FindFstabItemForPath1.fstable";
    Fstab fstab1;
    ReadFstabFromFile(fstabFile1, fstab1);
    struct FstabItem* item = nullptr;
    const std::string path1 = "";
    item = FindFstabItemForPath(fstab1, path1);
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path2 = "/data";
    item = FindFstabItemForPath(fstab1, path2);
    if (item != nullptr) {
        SUCCEED();
    }
    const std::string path3 = "/data2";
    item = FindFstabItemForPath(fstab1, path3);
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path4 = "/data2/test";
    item = FindFstabItemForPath(fstab1, path4);
    if (item != nullptr) {
        SUCCEED();
    }
}

HWTEST_F(FstabApiUnitTest, FindFstabItemForMountPoint_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/FindFstabItemForMountPoint1.fstable";
    Fstab fstab1;
    ReadFstabFromFile(fstabFile1, fstab1);
    struct FstabItem* item = nullptr;
    const std::string mp1 = "/data";
    const std::string mp2 = "/data2";
    item = FindFstabItemForMountPoint(fstab1, mp2);
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string mp3 = "/data";
    item = FindFstabItemForMountPoint(fstab1, mp3);
    if (item != nullptr) {
        SUCCEED();
    }
}

HWTEST_F(FstabApiUnitTest, GetMountFlags_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/GetMountFlags1.fstable";
    int fd = open(fstabFile1.c_str(), O_RDONLY);
    if (fd < 0) {
        cout << "fstabFile open failed, fd = " << fd << endl;
        FAIL();
        return;
    } else {
        close(fd);
    }
    Fstab fstab1;
    ReadFstabFromFile(fstabFile1, fstab1);
    struct FstabItem* item = nullptr;
    const std::string mp = "/hos";
    item = FindFstabItemForMountPoint(fstab1, mp);
    if (item == nullptr) {
        SUCCEED();
    }
    std::string fsSpecificOptions;
    unsigned long flags = GetMountFlags(item->mountOptions, fsSpecificOptions);
    EXPECT_EQ(flags, static_cast<unsigned long>(MS_NOSUID | MS_NODEV | MS_NOATIME));
}
} // namespace updater_ut
