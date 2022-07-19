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

#include "updater_unittest.h"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <sys/mount.h>
#include <unistd.h>
#include "fs_manager/mount.h"
#include "log/log.h"
#include "package/pkg_manager.h"
#include "unittest_comm.h"
#include "updater/updater.h"
#include "updater_main.h"
#include "updater_ui.h"
#include "utils.h"

namespace UpdaterUt {
using namespace testing::ext;
using namespace UpdaterUt;
using namespace Updater;
using namespace std;
using namespace Hpackage;
using namespace testing;

void UpdaterUnitTest::SetUp()
{
    unsigned long mountFlag = MS_REMOUNT;
    std::string tmpPath = "/tmp";
    // mount rootfs to read-write.
    std::string rootSource = "/dev/root";
    if (mount(rootSource.c_str(), "/", "ext4", mountFlag, nullptr) != 0) {
        std::cout << "Cannot re-mount rootfs\n";
    }
    mode_t mode = (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    auto ret = mkdir(tmpPath.c_str(), mode);
    if (ret != 0 && errno != EEXIST) {
        std::cout << "Cannot create \"/tmp\" directory: " << errno << "\n";
    }

    // Load specific fstab for testing.
    LoadSpecificFstab("/data/updater/updater/etc/fstab.ut.updater");
}

void UpdaterUnitTest::TearDown() {}

void UpdaterUnitTest::SetUpTestCase()
{
    UpdaterUiInit();
}

void UpdaterUnitTest::TearDownTestCase()
{
    DeleteView();
}

HWTEST_F(UpdaterUnitTest, updater_StartUpdaterProc, TestSize.Level1)
{
    std::string packagePath = "/data/updater/updater/updater_without_updater_binary.zip";
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    int maxTemperature;
    UpdaterStatus status;
    status = StartUpdaterProc(pkgManager, packagePath, 0, maxTemperature);
    EXPECT_EQ(status, UPDATE_CORRUPT);

    packagePath = "/data/updater/updater/updater_with_incorrect_binary.zip";
    status = StartUpdaterProc(pkgManager, packagePath, 0, maxTemperature);
    EXPECT_EQ(status, UPDATE_CORRUPT);

    packagePath = "/data/updater/updater/updater.zip";
    std::vector<std::string> components;
    int32_t ret = pkgManager->LoadPackage(packagePath, GetTestCertName(), components);
    EXPECT_EQ(ret, 0);
    status = StartUpdaterProc(pkgManager, packagePath, 0, maxTemperature);
    EXPECT_EQ(status, UPDATE_SUCCESS);

    // retrycount is greater than 0.
    status = StartUpdaterProc(pkgManager, packagePath, 1, maxTemperature);
    EXPECT_EQ(status, UPDATE_RETRY);

    packagePath = "/data/updater/updater/updater_binary_abnormal.zip";
    status = StartUpdaterProc(pkgManager, packagePath, 1, maxTemperature);
    PkgManager::ReleasePackageInstance(pkgManager);
    EXPECT_EQ(status, UPDATE_ERROR);
}

HWTEST_F(UpdaterUnitTest, updater_GetUpdatePackageInfo, TestSize.Level1)
{
    // Non-exist file.
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    std::string nonExistPackagePath = "/data/non_exist";
    int ret = GetUpdatePackageInfo(pkgManager, nonExistPackagePath);
    EXPECT_EQ(ret, static_cast<int>(PKG_INVALID_FILE));

    // valid  updater package.
    std::string validUpdaterPackage = "/data/updater/updater/updater.zip";

    ret = GetUpdatePackageInfo(pkgManager, validUpdaterPackage);
    PkgManager::ReleasePackageInstance(pkgManager);
    EXPECT_EQ(ret, static_cast<int>(PKG_SUCCESS));
}

HWTEST_F(UpdaterUnitTest, updater_UpdateSdcard, TestSize.Level1)
{
    UpdaterStatus status;
    status = UpdaterFromSdcard();
    EXPECT_EQ(status, UPDATE_SUCCESS);
}
} // namespace updater_ut
