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

#ifndef PKG_TEST
#define PKG_TEST

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
#include "pkg_utils.h"
#include "unittest_comm.h"
#include "utils.h"

using Hpackage::PkgManager;
using Hpackage::PkgManagerImpl;
using Hpackage::PkgStream;
using Hpackage::PkgAlgorithmFactory;
using Hpackage::DigestAlgorithm;
using Hpackage::ComponentInfo;
using Hpackage::FileInfo;
using Hpackage::ZipFileInfo;
using Hpackage::PkgInfo;
using Updater::InitUpdaterLogger;

namespace UpdaterUt {
class PkgTest : public ::testing::Test {
public:
    PkgTest()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
    }
    virtual ~PkgTest()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        PkgManager::ReleasePackageInstance(pkgManager_);
        pkgManager_ = nullptr;
    }

protected:
    void SetUp()
    {
        // 先创建目标目录
        if (access(TEST_PATH_TO.c_str(), R_OK | W_OK) == -1) {
            mkdir(TEST_PATH_TO.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        }
        InitUpdaterLogger("UPDATER ", "updater_log.log", "updater_status.log", "error_code.log");
    }
    void TearDown() {}
    void TestBody() {}

    int32_t BuildFileDigest(uint8_t &digest, size_t size, const std::string &packagePath)
    {
        PkgManager::StreamPtr stream = nullptr;
        int32_t ret = pkgManager_->CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
        PKG_CHECK(ret == 0, return ret, "Create input stream fail %s", packagePath.c_str());
        size_t fileLen = stream->GetFileLength();
        PKG_CHECK(fileLen > 0, pkgManager_->ClosePkgStream(stream); return -1, "invalid file to load");
        PKG_CHECK(fileLen <= SIZE_MAX, pkgManager_->ClosePkgStream(stream); return -1,
            "Invalid file len %zu to load %s", fileLen, stream->GetFileName().c_str());

        size_t buffSize = 4096;
        Hpackage::PkgBuffer buff(buffSize);
        // 整包检查
        DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(PKG_DIGEST_TYPE_SHA256);
        PKG_CHECK(algorithm != nullptr, pkgManager_->ClosePkgStream(stream); return -1,
            "Invalid file %s", stream->GetFileName().c_str());
        algorithm->Init();

        size_t offset = 0;
        size_t readLen = 0;
        while (offset < fileLen) {
            ret = stream->Read(buff, offset, buffSize, readLen);
            PKG_CHECK(ret == 0,
                pkgManager_->ClosePkgStream(stream); return ret,
                "read buffer fail %s", stream->GetFileName().c_str());
            algorithm->Update(buff, readLen);

            offset += readLen;
            readLen = 0;
        }
        Hpackage::PkgBuffer buffer(&digest, size);
        algorithm->Final(buffer);
        pkgManager_->ClosePkgStream(stream);
        return 0;
    }

    void ExtractFile(PkgManager::PkgManagerPtr manager, std::vector<std::string> components, size_t i)
    {
        PkgManager::StreamPtr outStream = nullptr;
        PKG_LOGI("comp [%zu] file name: %s \r\n", i, (TEST_PATH_TO + components[i]).c_str());
        manager->CreatePkgStream(outStream, TEST_PATH_TO + components[i], 0, PkgStream::PkgStreamType_Write);
        EXPECT_NE(nullptr, outStream);
        if (outStream == nullptr) {
            return;
        }
        int ret = manager->ExtractFile(components[i], outStream);
        EXPECT_EQ(ret, 0);
        manager->ClosePkgStream(outStream);
        const FileInfo *info = manager->GetFileInfo(components[i]);
        ASSERT_NE(info, nullptr);
        if (info->packMethod == PKG_COMPRESS_METHOD_NONE) {
            const ComponentInfo* compInfo = (const ComponentInfo*)manager->GetFileInfo(components[i]);
            if (compInfo != nullptr) {
                PKG_LOGI("comp [%zu] componentAddr: %s \n", i, (*compInfo).fileInfo.identity.c_str());
                PKG_LOGI("comp [%zu] version: %s \n", i, (*compInfo).version.c_str());
                PKG_LOGI("comp [%zu] originalSize: %zu \n", i, (*compInfo).originalSize);
                PKG_LOGI("comp [%zu] size: %zu \n", i, (*compInfo).fileInfo.unpackedSize);
                PKG_LOGI("comp [%zu] id: %d \n", i, (*compInfo).id);
                PKG_LOGI("comp [%zu] resType: %d \n", i, (*compInfo).resType);
                PKG_LOGI("comp [%zu] flags: %d \n", i, (*compInfo).compFlags);
                PKG_LOGI("comp [%zu] type: %d \n", i, (*compInfo).type);
            }
        } else {
            PKG_LOGI("FileInfo [%zu] id: %s \n", i, info->identity.c_str());
            PKG_LOGI("FileInfo [%zu] unpackedSize: %zu \n", i, info->unpackedSize);
            PKG_LOGI("FileInfo [%zu] packedSize: %zu \n", i, info->packedSize);
            PKG_LOGI("FileInfo [%zu] packMethod: %d \n", i, info->packMethod);
            PKG_LOGI("FileInfo [%zu] digestMethod: %d \n", i, info->digestMethod);
            PKG_LOGI("FileInfo [%zu] flags: %d \n", i, info->flags);
        }
    }

    int CreateZipPackage(const std::vector<std::string> &testFileNames,
        const std::string packageName, const std::string &base, int digestMethod)
    {
        PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
        EXPECT_NE(pkgManager, nullptr);
        std::vector<std::pair<std::string, ZipFileInfo>> files;
        // 构建要打包的zip文件
        for (auto name : testFileNames) {
            ZipFileInfo file;
            file.fileInfo.identity = name;
            file.fileInfo.packMethod = PKG_COMPRESS_METHOD_ZIP;
            file.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
            std::string fileName = base + name;
            files.push_back(std::pair<std::string, ZipFileInfo>(fileName, file));
        }

        PkgInfo pkgInfo;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.digestMethod = digestMethod;
        pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;
        int32_t ret = pkgManager->CreatePackage(packageName, GetTestPrivateKeyName(digestMethod), &pkgInfo, files);
        EXPECT_EQ(ret, 0);
        return ret;
    }
    std::vector<std::string> testFileNames_ = {
        "loadScript.us",
        "registerCmd.us",
        "test_function.us",
        "test_if.us",
        "test_logic.us",
        "test_math.us",
        "test_native.us",
        "testscript.us",
        "Verse-script.us",
        "libcrypto.a",
        "ggg.zip"
    };
    PkgManagerImpl* pkgManager_ = nullptr;
    std::string testPackageName = "test_package.bin";
    std::string testZipPackageName = "test_package.zip";
    std::string testLz4PackageName = "test_package.lz4";
    std::string testGZipPackageName = "test_package.gz";
    std::string testCombinePkgName = "test_CombinePackage.zip";
};
}
#endif // PKG_TEST
