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

#include "mount_unittest.h"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
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
void MountUnitTest::SetUp(void)
{
    cout << "Updater Unit MountUnitTest Begin!" << endl;
}

// end
void MountUnitTest::TearDown(void)
{
    cout << "Updater Unit MountUnitTest End!" << endl;
}

// do something at the each function begining
void MountUnitTest::SetUpTestCase(void)
{
    mkdir("/misc", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

// do something at the each function end
void MountUnitTest::TearDownTestCase(void)
{
    rmdir("/misc");
}

HWTEST_F(MountUnitTest, FormatPartition_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/FormatPartition1.fstable";
    LoadSpecificFstab(fstabFile1);
    const std::string path1 = "";
    int ret = FormatPartition(path1);
    EXPECT_EQ(ret, -1);
    const std::string path2 = "/";
    ret = FormatPartition(path2);
    EXPECT_EQ(ret, 0);
    const std::string path3 = "/data";
    ret = FormatPartition(path3);
    EXPECT_EQ(ret, -1);
    const std::string path4 = "/misc";
    ret = FormatPartition(path4);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(MountUnitTest, MountForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/MountForPath1.fstable";
    LoadSpecificFstab(fstabFile1);
    const std::string path1 = "";
    int ret = MountForPath(path1);
    EXPECT_EQ(ret, -1);
    const std::string path2 = "/vendor";
    ret = MountForPath(path2);
    EXPECT_EQ(ret, 0);
    ret = FormatPartition("/misc");
    EXPECT_EQ(ret, 0);
    const std::string path3 = "/misc";
    ret = MountForPath(path3);
    EXPECT_EQ(ret, 0);

    const std::string path4 = "/data1";
    ret = MountForPath(path4);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(MountUnitTest, UmountForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/UmountForPath1.fstable";
    LoadSpecificFstab(fstabFile1);
    const std::string path1 = "";
    int ret = UmountForPath(path1);
    EXPECT_EQ(ret, -1);
    const std::string path2 = "/misc/mount2";
    ret = UmountForPath(path2);
    EXPECT_EQ(ret, 0);
    const std::string path3 = "/misc";
    ret = UmountForPath(path3);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(MountUnitTest, GetMountStatusForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/GetMountStatusForPath1.fstable";
    LoadSpecificFstab(fstabFile1);
    const std::string path1 = "";
    MountStatus ret = GetMountStatusForPath(path1);
    EXPECT_EQ(ret, MountStatus::MOUNT_ERROR);
    const std::string path2 = "/vendor";
    ret = GetMountStatusForPath(path2);
    EXPECT_EQ(ret, MountStatus::MOUNT_MOUNTED);
    const std::string path3 = "/data/mount2";
    ret = GetMountStatusForPath(path3);
    EXPECT_EQ(ret, MountStatus::MOUNT_UMOUNTED);
}

HWTEST_F(MountUnitTest, SetupPartitions_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/updater/mount_unitest/SetupPartitions1.fstable";
    LoadSpecificFstab(fstabFile1);
    int ret = SetupPartitions();
    EXPECT_EQ(ret, -1);
    const std::string fstabFile2 = "/data/updater/mount_unitest/SetupPartitions2.fstable";
    LoadSpecificFstab(fstabFile2);
    ret = SetupPartitions();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(MountUnitTest, GetBlockDeviceByMountPoint_unitest, TestSize.Level1)
{
    LoadFstab();
    const std::string fstabFile1 = "/data/updater/mount_unitest/GetBlockDeviceByMountPoint1.fstable";
    LoadSpecificFstab(fstabFile1);
    unsigned long mountFlag = MS_REMOUNT;
    const std::string tmpPath = "/vendor/etc/fstab.updater";
    std::string vendorSource = GetBlockDeviceByMountPoint(tmpPath);
    // mount rootfs to read-write.
    if (mount(vendorSource.c_str(), "/vendor", "ext4", mountFlag, nullptr) != 0) {
        std::cout << "Cannot re-mount vendor\n";
    }
    auto ret = open(tmpPath.c_str(), O_RDONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret < 0) {
        std::cout << "Cannot open \"/vendor/etc/fstab.updater\" file: " << errno << "\n";
        return;
    }
    close(ret);
    LoadFstab();
    unlink(tmpPath.c_str());

    LoadSpecificFstab(fstabFile1);
    const std::string mountPoint1 = "";
    const std::string str1 = GetBlockDeviceByMountPoint(mountPoint1);
    EXPECT_TRUE(str1.empty());
    const std::string mountPoint2 = "/data2";
    const std::string str2 = GetBlockDeviceByMountPoint(mountPoint2);
    EXPECT_TRUE(str2.empty());
    const std::string mountPoint3 = "/data";
    const std::string str3 = GetBlockDeviceByMountPoint(mountPoint3);
    EXPECT_FALSE(str3.empty());
}
} // namespace updater_ut
