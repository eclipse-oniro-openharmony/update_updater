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

#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_test.h"
#include "pkg_utils.h"

using namespace std;
using namespace Hpackage;
using namespace Updater;
using namespace testing::ext;

namespace UpdaterUt {
class PackageUnitTest : public PkgTest {
public:
    PackageUnitTest() {}
    ~PackageUnitTest() override {}
public:
    int TestPackageUnpack(int type)
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::string> components;
        // 使用上面打包的包进行解析
        int32_t ret = pkgManager_->LoadPackage(
            "/data/updater/package/test_package.zip", GetTestCertName(type), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("comp [%zu] file name: %s \r\n", i, (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        return PKG_SUCCESS;
    }

    int TestZipPkgCompress(int digestMethod)
    {
        return CreateZipPackage(testFileNames_, TEST_PATH_TO + testZipPackageName, TEST_PATH_FROM, digestMethod);
    }

    int TestZipPkgDecompress(int digestMethod)
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::string> components;
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testZipPackageName,
            GetTestCertName(digestMethod), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("file name: %s \r\n", (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        return ret;
    }

    int TestVerifyUpgradePackage()
    {
        constexpr size_t digestSize = 32;
        std::vector<uint8_t> digest(digestSize);
        std::string path = "/data/updater/package/test_package.zip";
        BuildFileDigest(*digest.data(), digest.capacity(), path.c_str());
        int ret = VerifyPackage(path.c_str(), GetTestCertName(0).c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(0, ret);
        ret = VerifyPackage(nullptr, nullptr, nullptr, nullptr, digest.capacity());
        EXPECT_EQ(PKG_INVALID_PARAM, ret);
        return 0;
    }
};

HWTEST_F(PackageUnitTest, TestVerifyUpgradePackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyUpgradePackage());
}

HWTEST_F(PackageUnitTest, TestPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestPackageUnpack(PKG_DIGEST_TYPE_SHA256));
}

HWTEST_F(PackageUnitTest, TestZipPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestZipPkgCompress(PKG_DIGEST_TYPE_SHA256));
    EXPECT_EQ(0, test.TestZipPkgDecompress(PKG_DIGEST_TYPE_SHA256));
}
}
