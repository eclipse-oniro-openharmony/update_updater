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
    unsigned long mountFlag = MS_REMOUNT;
    std::string tmpPath = "/tmp";
    // mount rootfs to read-write.
    std::string rootSource = "/dev/root";
    if (mount(rootSource.c_str(), "/", "ext4", mountFlag, nullptr) != 0) {
        std::cout << "Cannot re-mount rootfs\n";
    }
    auto ret = mkdir(tmpPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret != 0) {
        std::cout << "Cannot create \"/tmp\" directory: " << errno << "\n";
    }

    // Load specific fstab for testing.
    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
    cout << "SetUpTestCase" << endl;
}

void UpdateImageBlockTest::TearDown()
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(UpdateImageBlockTest, update_image_block_test_001, TestSize.Level1)
{
    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
    string devPath = GetBlockDeviceByMountPoint("/vendortest1");
    size_t bufferSize = 4096;
    std::vector<uint8_t> buffer(bufferSize, 0);
    auto ret = Store::WriteDataToStore("/", devPath, buffer, bufferSize);
    string packagePath = "/data/updater/updater/updater_diff_1.zip";
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    std::vector<std::string> components;
    ret = pkgManager->LoadPackage(packagePath, GetTestCertName(), components);
    printf("load package's ret : %d\n", ret);
    UpdaterEnv* env = new UpdaterEnv(pkgManager, nullptr, false); // retry
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env);
    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        printf(" execute ret : %d\n", ret);
    }
    delete env;
    ScriptManager::ReleaseScriptManager();
    PkgManager::ReleasePackageInstance(pkgManager);
}

HWTEST_F(UpdateImageBlockTest, update_image_block_test_002, TestSize.Level1)
{
    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
    string devPath = GetBlockDeviceByMountPoint("/vendortest");
    size_t bufferSize = 4096;
    printf("dev path is %s\n", devPath.c_str());
    std::vector<uint8_t> buffer(bufferSize, 0);
    auto ret = Store::WriteDataToStore("/", devPath, buffer, bufferSize);
    printf("WriteDataToStore's ret: %d\n", ret);

    string oldName = "/data/updater/updater/retry_flag";
    string newName = "/data/updater/update_tmp/retry_flag";
    string newDir = "/data/updater/update_tmp";
    ret = Store::CreateNewSpace(newDir, false);
    printf("CreateNewSpace's ret: %d\n", ret);
    ret = rename(oldName.c_str(), newName.c_str());
    printf("rename's ret : %d\n", ret);

    string packagePath = "/data/updater/updater/updater_diff_2.zip";
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    std::vector<std::string> components;
    ret = pkgManager->LoadPackage(packagePath, GetTestCertName(), components);
    printf("load package's ret : %d\n", ret);
    UpdaterEnv* env = new UpdaterEnv(pkgManager, nullptr, true);
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env);
    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        printf(" execute ret : %d\n", ret);
    }
    delete env;
    ScriptManager::ReleaseScriptManager();
    PkgManager::ReleasePackageInstance(pkgManager);
}
}
