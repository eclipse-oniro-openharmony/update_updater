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

#include "update_partitions_unittest.h"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include "log.h"
#include "package/pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "script_utils.h"
#include "unittest_comm.h"
#include "update_processor.h"
#include "utils.h"

using namespace Updater;
using namespace testing::ext;
using namespace Uscript;
using namespace std;
using namespace Hpackage;

namespace UpdaterUt {
void UpdatePartitionsUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdatePartitionsUnitTest Begin!" << endl;
}

void UpdatePartitionsUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdatePartitionsUnitTest End!" << endl;
}

// do something at the each function begining
void UpdatePartitionsUnitTest::SetUpTestCase(void) {}

// do something at the each function end
void UpdatePartitionsUnitTest::TearDownTestCase(void) {}

HWTEST_F(UpdatePartitionsUnitTest, UpdatePartitions_Unitest01, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/parts/updaterpart01.zip";
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    std::vector<std::string> components;
    int partRet = -1;
    int ret = pkgManager->LoadPackage(packagePath, GetTestCertName(), components);
    cout << "load package's ret:" << ret << endl;
    UpdaterEnv* env = new UpdaterEnv(pkgManager, nullptr, false);
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env);
    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        cout << " execute ret:" << ret << endl;
        if (i == 1) { // only run update_partitions script
            partRet = ret;
        }
    }
    delete env;
    ScriptManager::ReleaseScriptManager();
    PkgManager::ReleasePackageInstance(pkgManager);
    EXPECT_EQ(partRet, USCRIPT_SUCCESS);
}

HWTEST_F(UpdatePartitionsUnitTest, UpdatePartitions_Unitest02, TestSize.Level1)
{
    const string packagePath = "/data/updater/updater/parts/updaterpart02.zip";
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    std::vector<std::string> components;
    int partRet = -1;
    int ret = pkgManager->LoadPackage(packagePath, GetTestCertName(), components);
    cout << "load package's ret:" << ret << endl;
    UpdaterEnv* env = new UpdaterEnv(pkgManager, nullptr, false);
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env);
    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        cout << " execute ret:" << ret << endl;
        if (i == 1) { // only run update_partitions script
            partRet = ret;
        }
    }
    delete env;
    ScriptManager::ReleaseScriptManager();
    PkgManager::ReleasePackageInstance(pkgManager);
    EXPECT_EQ(partRet, USCRIPT_ERROR_EXECUTE);
}
} // namespace updater_ut
