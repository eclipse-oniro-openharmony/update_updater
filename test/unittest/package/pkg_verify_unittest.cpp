/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <unistd.h>
#include "log.h"
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_test.h"
#include "pkg_utils.h"

#include "package.h"
#include "cert_verify.h"
#include "pkcs7_signed_data.h"

using namespace std;
using namespace Hpackage;
using namespace Updater;
using namespace testing::ext;

namespace UpdaterUt {
class PackageVerifyTest : public PkgTest {
public:
    PackageVerifyTest() {}
    ~PackageVerifyTest() override {}
public:
    int TestGetFileSize(const std::string &testfileName)
    {
        int32_t ret = GetFileSize(testfileName);
        return ret;
    }

    int TestExtraPackageFile()
    {
        int32_t ret = ExtraPackageFile(nullptr, nullptr, nullptr);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        std::string packagePath = "invalid_path";
        std::string file = "invalid_file";
        std::string outPath = "invalid_path";
        ret = ExtraPackageFile(packagePath.c_str(), file.c_str(), outPath.c_str());
        EXPECT_EQ(ret, PKG_INVALID_FILE);

        packagePath = "/data/updater/package/test_package.zip";
        file = "updater.bin";
        outPath = "/data/updater/package/";
        ret = ExtraPackageFile(packagePath.c_str(), file.c_str(), outPath.c_str());
        EXPECT_EQ(ret, PKG_SUCCESS);
        return 0;
    }

    int TestExtraPackageDir()
    {
        int32_t ret = ExtraPackageDir(nullptr, nullptr, nullptr);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        std::string packagePath = "invalid_path";
        std::string outPath = "invalid_path";
        ret = ExtraPackageDir(packagePath.c_str(), nullptr, outPath.c_str());
        EXPECT_EQ(ret, PKG_INVALID_FILE);

        packagePath = "/data/updater/package/test_package.zip";
        outPath = "/data/updater/package/";
        ret = ExtraPackageDir(packagePath.c_str(), nullptr, outPath.c_str());
        EXPECT_EQ(ret, PKG_SUCCESS);
        return 0;
    }
};

HWTEST_F(PackageVerifyTest, TestExtraPackageDir, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestExtraPackageDir());
}

HWTEST_F(PackageVerifyTest, TestExtraPackageFile, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestExtraPackageFile());
}

HWTEST_F(PackageVerifyTest, TestGetFileSize, TestSize.Level1)
{
    PackageVerifyTest test;
    std::string testFileName = "invalid_path";
    EXPECT_EQ(0, test.TestGetFileSize(testFileName));
    testFileName = "/data/updater/package/test_package.zip";
    EXPECT_EQ(1368949, test.TestGetFileSize(testFileName));
}
}
