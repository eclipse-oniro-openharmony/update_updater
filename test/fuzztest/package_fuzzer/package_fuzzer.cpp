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

#include "package_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "log/log.h"
#include "package.h"
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_test.h"
#include "pkg_utils.h"

using namespace Updater;
using namespace Hpackage;
namespace OHOS {
class FuzzPackageUnitTest : public FuzzPkgTest {
public:
    FuzzPackageUnitTest() {}
    ~FuzzPackageUnitTest() override {}
public:
    int TestInvalidCreatePackage()
    {
        ComponentInfoExt info {};
        uint8_t pkgType = PkgPackType::PKG_PACK_TYPE_UPGRADE;
        int ret = CreatePackage(nullptr, &info, nullptr, GetFuzzPrivateKeyName(0).c_str());

        UpgradePkgInfoExt pkgInfoExt {};
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &info, nullptr, GetFuzzPrivateKeyName(0).c_str());

        constexpr uint32_t digestLen = 32;
        ret = VerifyPackage(nullptr, GetFuzzCertName(0).c_str(), nullptr, nullptr, digestLen);

        std::string packagePath = TEST_PATH_TO + testPackageName;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &info, packagePath.c_str(), GetFuzzPrivateKeyName(0).c_str());

        pkgType = PkgPackType::PKG_PACK_TYPE_ZIP;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &info, packagePath.c_str(), GetFuzzPrivateKeyName(0).c_str());

        pkgType = PkgPackType::PKG_PACK_TYPE_LZ4;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &info, packagePath.c_str(), GetFuzzPrivateKeyName(0).c_str());

        pkgType = PkgPackType::PKG_PACK_TYPE_GZIP;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &info, packagePath.c_str(), GetFuzzPrivateKeyName(0).c_str());

        pkgType = PkgPackType::PKG_PACK_TYPE_NONE;
        pkgInfoExt.pkgType = pkgType;
        ret = CreatePackage(&pkgInfoExt, &info, packagePath.c_str(), GetFuzzPrivateKeyName(0).c_str());
        return ret;
    }

    int TestPackagePack(int type = PKG_DIGEST_TYPE_SHA256)
    {
        int32_t ret;
        uint32_t updateFileVersion = 1000;
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.updateFileVersion = updateFileVersion;
        pkgInfo.digestMethod = type;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_UPGRADE;
        std::string filePath;
        uint32_t componentIdBase = 100;
        uint8_t componentFlags = 22;
        ComponentInfoExt comp[testFileNames_.size()];
        for (size_t n = 0; n < testFileNames_.size(); n++) {
            comp[n].componentAddr = strdup(testFileNames_[n].c_str());
            filePath = TEST_PATH_FROM;
            filePath += testFileNames_[n].c_str();
            comp[n].filePath = strdup(filePath.c_str());
            comp[n].version = strdup("55555555");
            ret = BuildFileDigest(*comp[n].digest, sizeof(comp[n].digest), filePath);
            comp[n].size = GetFileSize(filePath);
            comp[n].originalSize = comp[n].size;
            comp[n].id = n + componentIdBase;
            comp[n].resType = 1;
            comp[n].type = 1;
            comp[n].flags = componentFlags;
            filePath.clear();
        }
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(),
            GetFuzzPrivateKeyName(pkgInfo.digestMethod).c_str());
        for (size_t n = 0; n < testFileNames_.size(); n++) {
            free(comp[n].componentAddr);
            free(comp[n].filePath);
            free(comp[n].version);
        }
        free(pkgInfo.productUpdateId);
        free(pkgInfo.softwareVersion);
        free(pkgInfo.date);
        free(pkgInfo.time);
        return ret;
    }

    int TestZipPkgCompress(int digestMethod)
    {
        return CreateZipPackage(testFileNames_, TEST_PATH_TO + testZipPackageName, TEST_PATH_FROM, digestMethod);
    }

    int TestPackageUnpack(int type)
    {
        std::vector<std::string> componentsList;
        // 使用上面打包的包进行解析
        int32_t ret = pkgManager_->LoadPackage(
            testPackagePath + "test_package.zip", GetFuzzCertName(type), componentsList);

        for (size_t n = 0; n < componentsList.size(); n++) {
            PKG_LOGI("comp [%zu] file name: %s \r\n", n, (TEST_PATH_TO + componentsList[n]).c_str());
            ExtractFile(pkgManager_, componentsList, n);
        }
        return ret;
    }

    int TestZipPkgDecompress(int digestMethod)
    {
        std::vector<std::string> componentsList;
        int32_t ret = pkgManager_->LoadPackage(TEST_PATH_TO + testZipPackageName,
            GetFuzzCertName(digestMethod), componentsList);

        for (size_t n = 0; n < componentsList.size(); n++) {
            PKG_LOGI("file name: %s \r\n", (TEST_PATH_TO + componentsList[n]).c_str());
            ExtractFile(pkgManager_, componentsList, n);
        }
        return ret;
    }

    int TestGZipPkgCompress()
    {
        int ret = TestPackagePack();
        std::vector<std::pair<std::string, ZipFileInfo>> files;
        ZipFileInfo zipFile;
        zipFile.fileInfo.identity = testPackageName;
        zipFile.fileInfo.packMethod = PKG_COMPRESS_METHOD_GZIP;
        zipFile.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        std::string fileName = TEST_PATH_TO + testPackageName;
        files.push_back(std::pair<std::string, ZipFileInfo>(fileName, zipFile));

        PkgInfo info;
        info.signMethod = PKG_SIGN_METHOD_RSA;
        info.digestMethod  = PKG_DIGEST_TYPE_SHA256;
        info.pkgType = PKG_PACK_TYPE_GZIP;
        ret = pkgManager_->CreatePackage(TEST_PATH_TO + testGZipPackageName,
            GetFuzzPrivateKeyName(info.digestMethod), &info, files);
        return ret;
    }

    int TestVerifyUpgradePackage()
    {
        constexpr size_t digestSize = 32;
        std::vector<uint8_t> digest(digestSize);
        std::string path = testPackagePath + "test_package.zip";
        BuildFileDigest(*digest.data(), digest.capacity(), path.c_str());
        int ret = VerifyPackage(path.c_str(), GetFuzzCertName(0).c_str(), "", digest.data(), digest.capacity());
        ret = VerifyPackage(nullptr, nullptr, nullptr, nullptr, digest.capacity());
        return ret;
    }

    int TestVerifyPackageWithCallback()
    {
        std::string path = testPackagePath + "test_package.zip";
        int ret = VerifyPackageWithCallback(path.c_str(), GetFuzzCertName(0).c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("progress: %u\n", percent); });

        std::string keyPath = "";
        ret = VerifyPackageWithCallback(path.c_str(), keyPath.c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("progress: %u\n", percent); });

        std::function<void(int32_t result, uint32_t percent)> cb = nullptr;
        ret = VerifyPackageWithCallback(path.c_str(), GetFuzzCertName(0).c_str(), cb);

        path = "";
        ret = VerifyPackageWithCallback(path.c_str(), GetFuzzCertName(0).c_str(),
            [](int32_t result, uint32_t percent) { PKG_LOGI("progress: %u\n", percent); });
        return ret;
    }

    int TestLz4PkgCompress()
    {
        int ret = TestPackagePack();
        std::vector<std::pair<std::string, Lz4FileInfo>> files;
        Lz4FileInfo zipFile;
        int8_t compressionLevel = 14;
        zipFile.fileInfo.identity = testPackageName;
        zipFile.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
        zipFile.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        zipFile.compressionLevel = compressionLevel;
        zipFile.blockSizeID = 0;
        zipFile.contentChecksumFlag = 0;
        zipFile.blockIndependence = 0;
        std::string fileName = TEST_PATH_TO + testPackageName;
        files.push_back(std::pair<std::string, Lz4FileInfo>(fileName, zipFile));

        PkgInfo info;
        info.pkgType = PKG_PACK_TYPE_LZ4;
        info.signMethod = PKG_SIGN_METHOD_RSA;
        info.digestMethod  = PKG_DIGEST_TYPE_SHA256;
        ret = pkgManager_->CreatePackage(TEST_PATH_TO + testLz4PackageName,
            GetFuzzPrivateKeyName(info.digestMethod), &info, files);
        return ret;
    }
};

void FUZZPackageUnitTest(const uint8_t* data, size_t size)
{
    FuzzPackageUnitTest test;
    (void)test.TestLz4PkgCompress();
    (void)test.TestInvalidCreatePackage();
    (void)test.TestVerifyUpgradePackage();
    (void)test.TestVerifyPackageWithCallback();
    (void)test.TestPackagePack(PKG_DIGEST_TYPE_SHA256);
    (void)test.TestPackageUnpack(PKG_DIGEST_TYPE_SHA256);
    (void)test.TestZipPkgCompress(PKG_DIGEST_TYPE_SHA256);
    (void)test.TestZipPkgDecompress(PKG_DIGEST_TYPE_SHA256);
    (void)test.TestGZipPkgCompress();
}

void FuzzVerifyPackage(const uint8_t* data, size_t size)
{
    constexpr size_t digestSize = 32;
    std::vector<uint8_t> digest(digestSize);
    const std::string keyPath = "/data/fuzz/test/signing_cert.crt";
    const std::string pkgPath = "/data/fuzz/test/updater.zip";
    const std::string pkgDir = "/data/fuzz/test";
    const std::string dataInfo = std::string(reinterpret_cast<const char*>(data), size);
    VerifyPackage(dataInfo.c_str(), keyPath.c_str(), "", digest.data(), digest.capacity());
    VerifyPackage(pkgPath.c_str(), dataInfo.c_str(), "", digest.data(), digest.capacity());
    VerifyPackage(pkgPath.c_str(), keyPath.c_str(), dataInfo.c_str(), digest.data(), digest.capacity());
    VerifyPackage(pkgPath.c_str(), keyPath.c_str(), "", data, size);

    VerifyPackageWithCallback(dataInfo.c_str(), keyPath.c_str(),
        [](int32_t result, uint32_t percent) {});
    VerifyPackageWithCallback(pkgPath, dataInfo.c_str(), [](int32_t result, uint32_t percent) {});

    ExtraPackageDir(dataInfo.c_str(), keyPath.c_str(), nullptr, pkgDir.c_str());
    ExtraPackageDir(pkgPath.c_str(), dataInfo.c_str(), nullptr, pkgDir.c_str());
    ExtraPackageDir(pkgPath.c_str(), keyPath.c_str(), dataInfo.c_str(), pkgDir.c_str());
    ExtraPackageDir(pkgPath.c_str(), keyPath.c_str(), nullptr, dataInfo.c_str());

    const std::string file = "updater.bin";
    ExtraPackageFile(dataInfo.c_str(), keyPath.c_str(), file.c_str(), pkgDir.c_str());
    ExtraPackageFile(pkgPath.c_str(), dataInfo.c_str(), file.c_str(), pkgDir.c_str());
    ExtraPackageFile(pkgPath.c_str(), keyPath.c_str(), dataInfo.c_str(), pkgDir.c_str());
    ExtraPackageFile(pkgPath.c_str(), keyPath.c_str(), file.c_str(), dataInfo.c_str());
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzVerifyPackage(data, size);
    return 0;
}

