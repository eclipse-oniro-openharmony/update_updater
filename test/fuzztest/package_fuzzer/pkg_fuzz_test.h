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
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_utils.h"
#include "log.h"
#include "utils.h"

using Updater::InitUpdaterLogger;
using Hpackage::PkgManager;
using Hpackage::PkgManagerImpl;
using Hpackage::PkgStream;
using Hpackage::PkgAlgorithmFactory;
using Hpackage::DigestAlgorithm;
using Hpackage::ComponentInfo;
using Hpackage::FileInfo;
using Hpackage::ZipFileInfo;
using Hpackage::PkgInfo;

namespace OHOS {
const std::string TEST_PATH_FROM = "/data/updater/src/";
const std::string TEST_PATH_TO = "/data/updater/dst/";

inline std::string GetFuzzPrivateKeyName(int type = 0)
{
    std::string name = TEST_PATH_FROM;
    if (type == PKG_DIGEST_TYPE_SHA384) {
        name += "rsa_private_key384.pem";
    } else {
        name += "rsa_private_key2048.pem";
    }
    return name;
}

inline std::string GetFuzzCertName(int type = 0)
{
    std::string name = TEST_PATH_FROM;
    if (type == PKG_DIGEST_TYPE_SHA384) {
        name += "signing_cert384.crt";
    } else {
        name += "signing_cert.crt";
    }
    return name;
}

class FuzzPkgTest{
public:
    FuzzPkgTest()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::CreatePackageInstance());
    }
    virtual ~FuzzPkgTest()
    {
        PkgManager::ReleasePackageInstance(pkgManager_);
        pkgManager_ = nullptr;
    }

protected:
    void SetUp()
    {
        InitUpdaterLogger("UPDATER ", "updater_log.log", "updater_status.log", "error_code.log");
        // 先创建目标目录
        if (access(TEST_PATH_TO.c_str(), R_OK | W_OK) == -1) {
            mkdir(TEST_PATH_TO.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        }
    }
    void TestBody() {}
    void TearDown() {}

    int32_t BuildFileDigest(uint8_t &digest, size_t size, const std::string &pkgPath)
    {
        PkgManager::StreamPtr stream = nullptr;
        int32_t ret = pkgManager_->CreatePkgStream(stream, pkgPath, 0, PkgStream::PkgStreamType_Read);
        if (ret != 0) {
            PKG_LOGE("Create input stream fail %s", pkgPath.c_str());
            return ret;
        }
        size_t FileLength = stream->GetFileLength();
        if (FileLength <= 0 || FileLength > SIZE_MAX) {
            PKG_LOGE("Invalid file len %zu to load %s", FileLength, stream->GetFileName().c_str());
            pkgManager_->ClosePkgStream(stream);
            return -1;
        }

        size_t bufSize = 4096;
        Hpackage::PkgBuffer buff(bufSize);
        // 整包检查
        DigestAlgorithm::DigestAlgorithmPtr alg =
            PkgAlgorithmFactory::GetDigestAlgorithm(PKG_DIGEST_TYPE_SHA256);
        if (alg == nullptr) {
            PKG_LOGE("Invalid file %s", stream->GetFileName().c_str());
            pkgManager_->ClosePkgStream(stream);
            return -1;
        }
        alg->Init();

        size_t offset = 0;
        size_t readLen = 0;
        while (offset < FileLength) {
            ret = stream->Read(buff, offset, bufSize, readLen);
            if (ret != 0) {
                PKG_LOGE("read buffer fail %s", stream->GetFileName().c_str());
                pkgManager_->ClosePkgStream(stream);
                return ret;
            }
            alg->Update(buff, readLen);

            offset += readLen;
            readLen = 0;
        }
        Hpackage::PkgBuffer buffer(&digest, size);
        alg->Final(buffer);
        pkgManager_->ClosePkgStream(stream);
        return 0;
    }

    void ExtractFile(PkgManager::PkgManagerPtr manager, std::vector<std::string> components, size_t num)
    {
        PkgManager::StreamPtr outStream = nullptr;
        PKG_LOGI("comp [%zu] file name: %s \r\n", num, (TEST_PATH_TO + components[num]).c_str());
        manager->CreatePkgStream(outStream, TEST_PATH_TO + components[num], 0, PkgStream::PkgStreamType_Write);
        if (outStream == nullptr) {
            return;
        }
        (void)manager->ExtractFile(components[num], outStream);
        manager->ClosePkgStream(outStream);
        const FileInfo *info = manager->GetFileInfo(components[num]);
        if (info->packMethod == PKG_COMPRESS_METHOD_NONE) {
            const ComponentInfo* compInfo = (const ComponentInfo*)manager->GetFileInfo(components[num]);
            if (compInfo != nullptr) {
                PKG_LOGI("comp [%zu] componentAddr: %s \n", num, (*compInfo).fileInfo.identity.c_str());
                PKG_LOGI("comp [%zu] version: %s \n", num, (*compInfo).version.c_str());
                PKG_LOGI("comp [%zu] originalSize: %zu \n", num, (*compInfo).originalSize);
                PKG_LOGI("comp [%zu] size: %zu \n", num, (*compInfo).fileInfo.unpackedSize);
                PKG_LOGI("comp [%zu] id: %d \n", num, (*compInfo).id);
                PKG_LOGI("comp [%zu] resType: %d \n", num, (*compInfo).resType);
                PKG_LOGI("comp [%zu] flags: %d \n", num, (*compInfo).compFlags);
                PKG_LOGI("comp [%zu] type: %d \n", num, (*compInfo).type);
            }
        } else {
            PKG_LOGI("FileInfo [%zu] id: %s \n", num, info->identity.c_str());
            PKG_LOGI("FileInfo [%zu] unpackedSize: %zu \n", num, info->unpackedSize);
            PKG_LOGI("FileInfo [%zu] packedSize: %zu \n", num, info->packedSize);
            PKG_LOGI("FileInfo [%zu] packMethod: %d \n", num, info->packMethod);
            PKG_LOGI("FileInfo [%zu] digestMethod: %d \n", num, info->digestMethod);
            PKG_LOGI("FileInfo [%zu] flags: %d \n", num, info->flags);
        }
    }

    int CreateZipPackage(const std::vector<std::string> &testNames,
        const std::string pkgName, const std::string &base, int digestMethod)
    {
        PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
        std::vector<std::pair<std::string, ZipFileInfo>> files;
        // 构建要打包的zip文件
        for (auto name : testNames) {
            ZipFileInfo zipFile;
            zipFile.fileInfo.identity = name;
            zipFile.fileInfo.packMethod = PKG_COMPRESS_METHOD_ZIP;
            zipFile.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
            std::string fileName = base + name;
            files.push_back(std::pair<std::string, ZipFileInfo>(fileName, zipFile));
        }

        PkgInfo pkgInfo;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.digestMethod = digestMethod;
        pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;
        int32_t ret = pkgManager->CreatePackage(pkgName, GetFuzzPrivateKeyName(digestMethod), &pkgInfo, files);
        PkgManager::ReleasePackageInstance(pkgManager);
        return ret;
    }
    std::vector<std::string> testFileNames_ = {
        "test_math.us",
        "test_native.us",
        "testscript.us",
        "Verse-script.us",
        "libcrypto.a",
        "ggg.zip",
        "loadScript.us",
        "registerCmd.us",
        "test_function.us",
        "test_if.us",
        "test_logic.us",
    };
    PkgManagerImpl* pkgManager_ = nullptr;
    std::string testCombinePkgName = "test_CombinePackage.zip";
    std::string testPackagePath = "/data/updater/package/";
    std::string testPackageName = "test_package.bin";
    std::string testZipPackageName = "test_package.zip";
    std::string testLz4PackageName = "test_package.lz4";
    std::string testGZipPackageName = "test_package.gz";
};
}
#endif // PKG_TEST
