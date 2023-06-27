/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <thread>

#include "update_bin/bin_flow_update.h"
#include "update_bin/bin_process.h"
#include "log.h"

using namespace testing::ext;
using namespace Hpackage;
using namespace Updater;

namespace OHOS {
constexpr const char *PKG_PATH = "/data/updater/package/update.bin";
constexpr  int MAX_LOG_BUF_SIZE = 1 * 1024 * 1024;
constexpr  int BUF_SIZE = 4 * 1024 * 1024;
class BinFlowUpdateTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp()
    {
        InitUpdaterLogger("UPDATER", "updater_log.log", "updater_status.log", "error_code.log");
    }
    void TearDown() {}
    void TestBody() {}

    int TestBinFlowUpdater()
    {
        LOG(INFO) << "TestBinFlowUpdater start";
        std::string packagePath = "/data/updater/package/updater_flow.zip";
        PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
        if (pkgManager == nullptr) {
            LOG(ERROR) << "pkgManager is nullptr";
            return -1;
        }

        std::vector<std::string> components;
        int32_t ret = pkgManager->LoadPackage(packagePath, Utils::GetCertName(), components);
        if (ret != PKG_SUCCESS) {
            LOG(ERROR) << "Fail to load package";
            PkgManager::ReleasePackageInstance(pkgManager);
            return -1;
        }

        ret = Updater::ExecUpdate(pkgManager, false, packagePath,
            [](const char *cmd, const char *content) {
                LOG(INFO) << "pip msg, " << cmd << ":" << content;
            });
        PkgManager::ReleasePackageInstance(pkgManager);
        return ret;
    }
};

HWTEST_F(BinFlowUpdateTest, binFlowUpdateTest01, TestSize.Level0)
{
    std::cout << "binFlowUpdateTest01 start\n";
    FILE* fp = fopen(PKG_PATH, "rb");
    if (fp == nullptr) {
        std::cout << "fopen /data/updater/package/update.bin failed" << " : " << strerror(errno);
    }
    EXPECT_NE(fp, nullptr);

    uint8_t buf[MAX_LOG_BUF_SIZE] {};
    size_t len;
    int ret = 0;
    BinFlowUpdate binFlowUpdate(BUF_SIZE);
    while ((len = fread(buf, 1, sizeof(buf), fp)) != 0) {
        ret = binFlowUpdate.StartBinFlowUpdate(buf, len);
        if (ret != 0) {
            break;
        }
    }

    EXPECT_EQ(ret, 0);
    std::cout << "binFlowUpdateTest01 end\n";
}

HWTEST_F(BinFlowUpdateTest, TestBinFlowUpdater, TestSize.Level0)
{
    BinFlowUpdateTest test;
    EXPECT_EQ(0, test.TestBinFlowUpdater());
}
}  // namespace OHOS
