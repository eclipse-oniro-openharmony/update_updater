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
    int TestInvalidCreatePackage()
    {
        ComponentInfoExt compInfo;
        uint8_t pkgType = PkgPackType::PKG_PACK_TYPE_UPGRADE;
        int ret = CreatePackage(nullptr, &compInfo, nullptr, GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        UpgradePkgInfoExt pkgInfoExt;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, nullptr, GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        constexpr uint32_t digestLen = 32;
        ret = VerifyPackage(nullptr, GetTestCertName(0).c_str(), nullptr, nullptr, digestLen);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        std::string packagePath = TEST_PATH_TO + testPackageName;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, packagePath.c_str(), GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        pkgType = PkgPackType::PKG_PACK_TYPE_ZIP;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, packagePath.c_str(), GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        pkgType = PkgPackType::PKG_PACK_TYPE_LZ4;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, packagePath.c_str(), GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        pkgType = PkgPackType::PKG_PACK_TYPE_GZIP;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, packagePath.c_str(), GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        pkgType = PkgPackType::PKG_PACK_TYPE_NONE;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, packagePath.c_str(), GetTestPrivateKeyName(0).c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);
        return 0;
    }

    int TestPackagePack(int type = PKG_DIGEST_TYPE_SHA256)
    {
        int32_t ret;
        uint32_t updateFileVersion = 1000;
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.digestMethod = type;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_UPGRADE;
        pkgInfo.updateFileVersion = updateFileVersion;
        std::string filePath;
        uint32_t componentIdBase = 100;
        uint8_t componentFlags = 22;
        ComponentInfoExt comp[testFileNames_.size()];
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            comp[i].componentAddr = strdup(testFileNames_[i].c_str());
            filePath = TEST_PATH_FROM;
            filePath += testFileNames_[i].c_str();
            comp[i].filePath = strdup(filePath.c_str());
            comp[i].version = strdup("55555555");
            ret = BuildFileDigest(*comp[i].digest, sizeof(comp[i].digest), filePath);
            EXPECT_EQ(ret, PKG_SUCCESS);
            comp[i].size = GetFileSize(filePath);
            comp[i].originalSize = comp[i].size;
            comp[i].id = i + componentIdBase;
            comp[i].resType = 1;
            comp[i].type = 1;
            comp[i].flags = componentFlags;
            filePath.clear();
        }
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(),
            GetTestPrivateKeyName(pkgInfo.digestMethod).c_str());
        EXPECT_EQ(ret, PKG_SUCCESS);
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
            free(comp[i].version);
        }
        free(pkgInfo.softwareVersion);
        free(pkgInfo.date);
        free(pkgInfo.time);
        free(pkgInfo.productUpdateId);
        return ret;
    }

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

    int TestGZipPkgCompress()
    {
        int ret = TestPackagePack();
        EXPECT_EQ(ret, PKG_SUCCESS);
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::pair<std::string, ZipFileInfo>> files;
        ZipFileInfo file;
        file.fileInfo.identity = testPackageName;
        file.fileInfo.packMethod = PKG_COMPRESS_METHOD_GZIP;
        file.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        std::string fileName = TEST_PATH_TO + testPackageName;
        files.push_back(std::pair<std::string, ZipFileInfo>(fileName, file));

        PkgInfo pkgInfo;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.digestMethod  = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.pkgType = PKG_PACK_TYPE_GZIP;
        return pkgManager_->CreatePackage(TEST_PATH_TO + testGZipPackageName,
            GetTestPrivateKeyName(pkgInfo.digestMethod), &pkgInfo, files);
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

    int TestVerifyPackageWithCallback()
    {
        std::string path = "/data/updater/package/test_package.zip";
        int ret = VerifyPackageWithCallback(path.c_str(), GetTestCertName(0).c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("current progress: %u\n", percent); });
        EXPECT_EQ(0, ret);

        std::string keyPath = "";
        ret = VerifyPackageWithCallback(path.c_str(), keyPath.c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("current progress: %u\n", percent); });
        EXPECT_EQ(PKG_INVALID_PARAM, ret);

        std::function<void(int32_t result, uint32_t percent)> cb = nullptr;
        ret = VerifyPackageWithCallback(path.c_str(), GetTestCertName(0).c_str(), cb);
        EXPECT_EQ(PKG_INVALID_PARAM, ret);

        path = "";
        ret = VerifyPackageWithCallback(path.c_str(), GetTestCertName(0).c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("current progress: %u\n", percent); });
        EXPECT_EQ(PKG_INVALID_PARAM, ret);
        return 0;
    }

    int TestLz4PkgCompress()
    {
        int ret = TestPackagePack();
        EXPECT_EQ(ret, PKG_SUCCESS);
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::pair<std::string, Lz4FileInfo>> files;
        Lz4FileInfo file;
        int8_t compressionLevel = 14;
        file.fileInfo.identity = testPackageName;
        file.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
        file.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        file.compressionLevel = compressionLevel;
        file.blockSizeID = 0;
        file.contentChecksumFlag = 0;
        file.blockIndependence = 0;
        std::string fileName = TEST_PATH_TO + testPackageName;
        files.push_back(std::pair<std::string, Lz4FileInfo>(fileName, file));

        PkgInfo pkgInfo;
        pkgInfo.pkgType = PKG_PACK_TYPE_LZ4;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.digestMethod  = PKG_DIGEST_TYPE_SHA256;
        return pkgManager_->CreatePackage(TEST_PATH_TO + testLz4PackageName,
            GetTestPrivateKeyName(pkgInfo.digestMethod), &pkgInfo, files);
    }
};

HWTEST_F(PackageUnitTest, TestLz4Package, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestLz4PkgCompress());
}

HWTEST_F(PackageUnitTest, TestInvalidCreatePackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestInvalidCreatePackage());
}

HWTEST_F(PackageUnitTest, TestVerifyUpgradePackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyUpgradePackage());
}

HWTEST_F(PackageUnitTest, TestVerifyPackageWithCallback, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyPackageWithCallback());
}

HWTEST_F(PackageUnitTest, TestPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestPackagePack(PKG_DIGEST_TYPE_SHA256));
    EXPECT_EQ(0, test.TestPackageUnpack(PKG_DIGEST_TYPE_SHA256));
}

HWTEST_F(PackageUnitTest, TestZipPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestZipPkgCompress(PKG_DIGEST_TYPE_SHA256));
    EXPECT_EQ(0, test.TestZipPkgDecompress(PKG_DIGEST_TYPE_SHA256));
}

HWTEST_F(PackageUnitTest, TestGZipPkg, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestGZipPkgCompress());
}
}
