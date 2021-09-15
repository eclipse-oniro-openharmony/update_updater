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
using namespace hpackage;
using namespace updater;
using namespace testing::ext;

namespace {
class PackageUnitTest : public PkgTest {
public:
    PackageUnitTest() {}
    ~PackageUnitTest() override {}
public:
    int TestPackagePack()
    {
        int32_t ret;
        uint32_t updateFileVersion = 1000;
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
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
        ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(), GetTestPrivateKeyName().c_str());
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

    int TestPackageUnpack()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::string> components;
        // 使用上面打包的包进行解析
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testPackageName, GetTestCertName(), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("comp [%zu] file name: %s \r\n", i, (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        return PKG_SUCCESS;
    }

    int TestZipPkgCompress()
    {
        return CreateZipPackage(testFileNames_, TEST_PATH_TO + testZipPackageName, TEST_PATH_FROM);
    }

    int TestZipPkgDecompress()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::string> components;
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testZipPackageName, GetTestCertName(), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("file name: %s \r\n", (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        return ret;
    }

    int TestLz4PkgCompress()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        // 使用lz4 压缩前面的bin包
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
        return pkgManager_->CreatePackage(TEST_PATH_TO + testLz4PackageName, GetTestPrivateKeyName(), &pkgInfo, files);
    }

    int TestLz4PkgCompressBlock()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        // 使用lz4 压缩前面的bin包
        std::vector<std::pair<std::string, Lz4FileInfo>> files;
        Lz4FileInfo file;
        file.fileInfo.identity = testPackageName;
        file.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4_BLOCK;
        file.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        file.compressionLevel = 0;
        file.blockSizeID = 0;
        file.contentChecksumFlag = 0;
        file.blockIndependence = 0;
        std::string fileName = TEST_PATH_TO + testPackageName;
        files.push_back(std::pair<std::string, Lz4FileInfo>(fileName, file));

        PkgInfo pkgInfo;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.digestMethod  = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.pkgType = PKG_PACK_TYPE_LZ4;
        return pkgManager_->CreatePackage(TEST_PATH_TO + testLz4PackageName, GetTestPrivateKeyName(), &pkgInfo, files);
    }

    int TestLz4PkgDecompress()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::string> components;
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testLz4PackageName, GetTestCertName(), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("file name: %s \r\n", (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        const PkgInfo *pkgInfo = pkgManager_->GetPackageInfo(TEST_PATH_TO + testCombinePkgName);
        EXPECT_EQ(nullptr, pkgInfo);
        return ret;
    }

    int TestCombinePkgPack()
    {
        int ret = TestPackagePack();
        EXPECT_EQ(ret, PKG_SUCCESS);
        std::vector<std::string> fileNames;
        fileNames.push_back(testZipPackageName);
        fileNames.push_back(testPackageName);
        ret = CreateZipPackage(fileNames, TEST_PATH_TO + testCombinePkgName, TEST_PATH_TO);
        EXPECT_EQ(ret, PKG_SUCCESS);
        return 0;
    }

    int TestCombinePkgUnpack()
    {
        EXPECT_EQ(0, TestCombinePkgPack());
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        std::vector<std::string> components;
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testCombinePkgName, GetTestCertName(), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("file name: %s \r\n", (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        const PkgInfo *pkgInfo = pkgManager_->GetPackageInfo(TEST_PATH_TO + testCombinePkgName);
        EXPECT_NE(nullptr, pkgInfo);
        return ret;
    }

    int TestVerifyUpgradePackage()
    {
        int ret = TestPackagePack();
        constexpr size_t digestSize = 32;
        constexpr uint8_t digestValue = 33;
        std::vector<uint8_t> digest(digestSize);
        BuildFileDigest(*digest.data(), digest.capacity(), TEST_PATH_TO + testPackageName);
        std::string path = TEST_PATH_TO + testPackageName;
        ret = VerifyPackage(path.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(0, ret);
        constexpr uint32_t digestLen = 10;
        digest[digestLen] = digestValue;
        ret = VerifyPackage(path.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(PKG_INVALID_SIGNATURE, ret);
        return 0;
    }

    int TestVerifyZipWithCallback()
    {
        int32_t ret = TestVerifyZip();
        EXPECT_EQ(ret, 0);
        std::string path = GetCurrPath();
        path = TEST_PATH_TO + testZipPackageName;
        ret = VerifyPackageWithCallback(path.c_str(), GetTestCertName().c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("current progress: %u\n", percent); });
        EXPECT_EQ(PKG_INVALID_PARAM, ret);
        return 0;
    }

    int TestVerifyZip()
    {
        int32_t ret = TestZipPkgCompress();
        EXPECT_EQ(ret, 0);
        constexpr size_t digestSize = 32;
        uint8_t digestValue = 33;
        int index = 10;
        std::string path = GetCurrPath();
        path = TEST_PATH_TO + testZipPackageName;
        std::vector<uint8_t> digest(digestSize);
        BuildFileDigest(*digest.data(), digest.capacity(), path);
        ret = VerifyPackage(path.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(ret, 0);
        digest[index] = digestValue;
        ret = VerifyPackage(path.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(PKG_INVALID_SIGNATURE, ret);
        return 0;
    }

    int TestVerifyLz4()
    {
        int32_t ret = TestLz4PkgCompress();
        EXPECT_EQ(ret, 0);
        constexpr size_t digestSize = 32;
        uint8_t digestValue = 33;
        int index = 10;
        std::string path = GetCurrPath();
        path = TEST_PATH_TO + testLz4PackageName;
        std::vector<uint8_t> digest(digestSize);
        BuildFileDigest(*digest.data(), digest.capacity(), path);
        ret = VerifyPackage(path.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(0, ret);
        digest[index] = digestValue;
        ret = VerifyPackage(path.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.capacity());
        EXPECT_EQ(PKG_INVALID_SIGNATURE, ret);
        return 0;
    }

    int TestInterfaceZip()
    {
        UpgradePkgInfoExt pkgInfo;
        uint8_t componentFlags = 22;
        uint32_t updateFileVersion = 1000;
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;
        pkgInfo.updateFileVersion = updateFileVersion;
        std::string filePath;
        ComponentInfoExt comp[testFileNames_.size()];
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            comp[i].componentAddr = strdup(testFileNames_[i].c_str());
            filePath = TEST_PATH_FROM;
            filePath += testFileNames_[i].c_str();
            comp[i].filePath = strdup(filePath.c_str());
            comp[i].flags = componentFlags;
            filePath.clear();
        }
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        int32_t ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(), GetTestPrivateKeyName().c_str());
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
        }
        EXPECT_EQ(ret, PKG_SUCCESS);
        constexpr size_t digestSize = 32;
        uint8_t digest[digestSize] = {0};
        ret = BuildFileDigest(*digest, sizeof(digest), packagePath);
        EXPECT_EQ(ret, PKG_SUCCESS);
        ret = VerifyPackage(packagePath.c_str(), GetTestCertName().c_str(), "", digest, digestSize);
        return ret;
    }

    int TestInterfaceLz4()
    {
        std::vector<std::string> testLz4FileNames = {
            "libcrypto.a"
        };
        UpgradePkgInfoExt pkgInfo;
        uint32_t updateFileVersion = 1000;
        pkgInfo.entryCount = testLz4FileNames.size();
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.signMethod = PKG_SIGN_METHOD_NONE;
        pkgInfo.pkgType = PKG_PACK_TYPE_LZ4;
        pkgInfo.updateFileVersion = updateFileVersion;
        std::string filePath;
        ComponentInfoExt comp[testLz4FileNames.size()];
        for (size_t i = 0; i < testLz4FileNames.size(); i++) {
            comp[i].componentAddr = strdup(testFileNames_[i].c_str());
            filePath = TEST_PATH_FROM;
            filePath += testFileNames_[i].c_str();
            comp[i].filePath = strdup(filePath.c_str());
            filePath.clear();
        }
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        int32_t ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(), TEST_PATH_FROM.c_str());
        for (size_t i = 0; i < testLz4FileNames.size(); i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
        }
        constexpr size_t digestSize = 32;
        std::vector<uint8_t> digest(digestSize);
        ret = BuildFileDigest(*digest.data(), digest.size(), packagePath.c_str());
        EXPECT_EQ(ret, PKG_SUCCESS);
        return VerifyPackage(packagePath.c_str(), GetTestCertName().c_str(), "", digest.data(), digest.size());
    }

    int TestInvalidCreatePackage() const
    {
        ComponentInfoExt compInfo;
        uint8_t pkgType = 5;
        int ret = CreatePackage(nullptr, &compInfo, nullptr, GetTestPrivateKeyName().c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        UpgradePkgInfoExt pkgInfoExt;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, nullptr, GetTestPrivateKeyName().c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        constexpr uint32_t digestLen = 32;
        ret = VerifyPackage(nullptr, GetTestCertName().c_str(), nullptr, nullptr, digestLen);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        // 无效的类型
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &compInfo, packagePath.c_str(), GetTestPrivateKeyName().c_str());
        EXPECT_EQ(ret, PKG_INVALID_PARAM);
        return 0;
    }

    int TestGZipPkgCompress()
    {
        int ret = TestPackagePack();
        EXPECT_EQ(ret, PKG_SUCCESS);

        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        // 使用Gzip 压缩前面的bin包
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
        return pkgManager_->CreatePackage(TEST_PATH_TO + testGZipPackageName, GetTestPrivateKeyName(), &pkgInfo, files);
    }

    int TestGZipPkgDecompress(const std::string& gzipPackageName)
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(nullptr, pkgManager_);
        std::vector<std::string> components;
        int32_t ret = pkgManager_->LoadPackage(gzipPackageName, GetTestCertName(), components);
        EXPECT_EQ(ret, PKG_SUCCESS);

        for (size_t i = 0; i < components.size(); i++) {
            PKG_LOGI("file name: %s \r\n", (TEST_PATH_TO + components[i]).c_str());
            ExtractFile(pkgManager_, components, i);
        }
        const PkgInfo *pkgInfo = pkgManager_->GetPackageInfo(gzipPackageName);
        EXPECT_NE(nullptr, pkgInfo);
        return ret;
    }

    int TestGZipPkgDecompressAll()
    {
        int ret = TestGZipPkgDecompress(TEST_PATH_TO + testGZipPackageName);
        EXPECT_EQ(0, ret);
        std::string path = TEST_PATH_FROM;
        path += "test_gzip_package.gz";
        ret = TestGZipPkgDecompress(path);
        EXPECT_EQ(0, ret);
        return ret;
    }

    int TestSecondLoadPackage()
    {
        TestPackagePack();
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(nullptr, pkgManager_);
        std::vector<std::string> fileIds;
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testPackageName, GetTestCertName(), fileIds);
        EXPECT_EQ(0, ret);
        // 在load其中的一个zip包
        PkgManager::StreamPtr outStream = nullptr;
        std::string secondFile = "ggg.zip";
        pkgManager_->CreatePkgStream(outStream, TEST_PATH_TO + secondFile, 0, PkgStream::PkgStreamType_Write);
        EXPECT_NE(nullptr, outStream);
        ret = pkgManager_->ExtractFile(secondFile, outStream);
        EXPECT_EQ(ret, 0);
        pkgManager_->ClosePkgStream(outStream);
        std::vector<std::string> secondFileIds;
        ret = pkgManager_->LoadPackage(TEST_PATH_TO + secondFile, GetTestCertName(), secondFileIds);
        EXPECT_EQ(0, ret);
        if (secondFileIds.size() != 1) {
            EXPECT_EQ(1, ret);
        }
        return 0;
    }

    void TestL1PackagePackSha384For(ComponentInfoExt comp[])
    {
        int32_t ret;
        std::string filePath;
        uint32_t componentIdBase = 100;
        uint8_t componentFlags = 22;
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
    }

    int TestL1PackagePack()
    {
        int32_t ret;
        uint32_t updateFileVersion = 1000;
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_UPGRADE;
        pkgInfo.updateFileVersion = updateFileVersion;
        pkgInfo.descriptPackageId = strdup("/package/pkg.bin");
        ComponentInfoExt comp[testFileNames_.size()];
        TestL1PackagePackSha384For(comp);
        std::string packagePath = TEST_PATH_TO;
        packagePath += "packageL1.bin";
        char *singStr = nullptr;
        uint32_t offset = 0;
        ret = CreatePackageL1(&pkgInfo, comp, packagePath.c_str(), &offset, &singStr);
        EXPECT_EQ(ret, PKG_SUCCESS);
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
            free(comp[i].version);
        }
        PKG_LOGI("CreatePackageL1 offset %u singStr: %s", offset, singStr);
        free(pkgInfo.softwareVersion);
        free(pkgInfo.date);
        free(pkgInfo.time);
        free(pkgInfo.productUpdateId);
        free(pkgInfo.descriptPackageId);
        free(singStr);
        return ret;
    }

    int TestL1PackagePackSha384()
    {
        int32_t ret;
        uint32_t updateFileVersion = 1000;
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA384;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_UPGRADE;
        pkgInfo.updateFileVersion = updateFileVersion;
        pkgInfo.descriptPackageId = strdup("/package/pkg.bin");
        ComponentInfoExt comp[testFileNames_.size()];
        TestL1PackagePackSha384For(comp);
        std::string packagePath = TEST_PATH_TO;
        packagePath += "packageL1_384.bin";
        char *singStr = nullptr;
        uint32_t offset = 0;
        ret = CreatePackageL1(&pkgInfo, comp, packagePath.c_str(), &offset, &singStr);
        EXPECT_EQ(ret, PKG_SUCCESS);
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
            free(comp[i].version);
        }
        PKG_LOGI("CreatePackageL1 offset %u singStr: %s", offset, singStr);
        free(pkgInfo.softwareVersion);
        free(pkgInfo.date);
        free(pkgInfo.time);
        free(pkgInfo.productUpdateId);
        free(pkgInfo.descriptPackageId);
        free(singStr);
        return ret;
    }
};

HWTEST_F(PackageUnitTest, TestPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestPackagePack());
    EXPECT_EQ(0, test.TestPackageUnpack());
}

HWTEST_F(PackageUnitTest, TestZipPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestZipPkgCompress());
    EXPECT_EQ(0, test.TestZipPkgDecompress());
}

HWTEST_F(PackageUnitTest, TestLz4Package, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestLz4PkgCompress());
    EXPECT_EQ(0, test.TestLz4PkgDecompress());
}

HWTEST_F(PackageUnitTest, TestLz4PackageBlock, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestLz4PkgCompressBlock());
    EXPECT_EQ(0, test.TestLz4PkgDecompress());
}

HWTEST_F(PackageUnitTest, TestCombinePkg, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestCombinePkgUnpack());
}

HWTEST_F(PackageUnitTest, TestVerifyUpgradePackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyUpgradePackage());
}

HWTEST_F(PackageUnitTest, TestVerifyZipWithCallback, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyZipWithCallback());
}

HWTEST_F(PackageUnitTest, TestVerifyZip, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyZip());
}

HWTEST_F(PackageUnitTest, TestVerifyLz4, TestSize.Level1) {
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestVerifyLz4());
}

HWTEST_F(PackageUnitTest, TestInterfaceLz4, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestInterfaceLz4());
}

HWTEST_F(PackageUnitTest, TestInterfaceZip, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestInterfaceZip());
}

HWTEST_F(PackageUnitTest, TestInvalidCreatePackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestInvalidCreatePackage());
}

HWTEST_F(PackageUnitTest, TestGZipPkg, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestGZipPkgCompress());
    EXPECT_EQ(0, test.TestGZipPkgDecompressAll());
}

HWTEST_F(PackageUnitTest, TestSecondLoadPackage, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestSecondLoadPackage());
}

HWTEST_F(PackageUnitTest, TestL1PackagePack, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestL1PackagePack());
}

HWTEST_F(PackageUnitTest, TestL1PackagePackSha384, TestSize.Level1)
{
    PackageUnitTest test;
    EXPECT_EQ(0, test.TestL1PackagePackSha384());
}
}
