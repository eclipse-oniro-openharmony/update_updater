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

#include "update_processor_unittest.h"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <sys/mount.h>
#include <unistd.h>
#include "fs_manager/mount.h"
#include "log/log.h"
#include "package/pkg_manager.h"
#include "unittest_comm.h"
#include "update_processor.h"
#include "updater_main.h"
#include "updater/updater.h"
#include "utils.h"

using namespace updater;
using namespace testing::ext;
using namespace uscript;
using namespace std;
using namespace hpackage;

namespace updater_ut {
using namespace testing::ext;
using namespace updater_ut;
using namespace testing;

void UpdateProcessorUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdatePartitionsUnitTest Begin!" << endl;
}

void UpdateProcessorUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdatePartitionsUnitTest End!" << endl;
}

// do something at the each function begining
void UpdateProcessorUnitTest::SetUpTestCase(void) {}

// do something at the each function end
void UpdateProcessorUnitTest::TearDownTestCase(void) {}

HWTEST_F(UpdateProcessorUnitTest, UpdateProcessor_001, TestSize.Level1)
{
    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
    const string packagePath = "/data/updater/updater/raw_image_write.zip";
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    std::vector<std::string> components;
    int32_t ret = pkgManager->LoadPackage(packagePath, GetTestCertName(), components);
    printf("load package's ret : %d\n", ret);
    UpdaterEnv* env = new UpdaterEnv(pkgManager, nullptr, false); // retry
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env);

    const string partitionName = "/rawwriter";
    const string devPath = GetBlockDeviceByMountPoint(partitionName);
    const string devDir = "/data/updater/ut/datawriter/";
    updater::utils::MkdirRecursive(devDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    int fd = open(devPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0664);
    printf("@@@ devPath = %s, fd=%d\n", devPath.c_str(), fd);
    close(fd);

    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        EXPECT_EQ(0, ret);
        printf(" execute ret : %d\n", ret);
    }
    delete env;
    ScriptManager::ReleaseScriptManager();
    PkgManager::ReleasePackageInstance(pkgManager);
}
} // namespace updater_ut
