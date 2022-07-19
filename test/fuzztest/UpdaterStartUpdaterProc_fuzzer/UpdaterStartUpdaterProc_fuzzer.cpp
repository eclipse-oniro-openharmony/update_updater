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

#include "UpdaterStartUpdaterProc_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "pkg_algorithm.h"
#include "pkg_algo_digest.h"
#include "pkg_utils.h"
#include "updater.h"
#include "updater_ui.h"

using namespace Updater;
using namespace Hpackage;

const static std::string TEST_PATH_TO = "/data/fuzz/test/";

static inline std::string GetTestCertName()
{
    std::string name = TEST_PATH_TO;
    name += "signing_cert.crt";
    return name;
}

static inline std::string GetTestPrivateKeyName()
{
    std::string name = TEST_PATH_TO;
    name += "rsa_private_key2048.pem";
    return name;
}

static int32_t BuildFileDigest(uint8_t &digest, size_t size, const std::string &packagePath)
    {
        PkgManager::StreamPtr stream = nullptr;
        PkgManager::PkgManagerPtr packageManager = PkgManager::GetPackageInstance();

        int32_t ret = packageManager->CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
        PKG_CHECK(ret == PKG_SUCCESS, packageManager->ClosePkgStream(stream);
            return ret, "Create input stream fail %s", packagePath.c_str());
        size_t fileLen = stream->GetFileLength();
        PKG_CHECK(fileLen > 0, packageManager->ClosePkgStream(stream); return PKG_INVALID_FILE, "invalid file to load");
        PKG_CHECK(fileLen <= SIZE_MAX, packageManager->ClosePkgStream(stream); return PKG_INVALID_FILE,
            "Invalid file len %zu to load %s", fileLen, stream->GetFileName().c_str());

        size_t buffSize = 4096;
        PkgBuffer buff(buffSize);
        // 整包检查
        DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(PKG_DIGEST_TYPE_SHA256);
        PKG_CHECK(algorithm != nullptr, packageManager->ClosePkgStream(stream); return PKG_NOT_EXIST_ALGORITHM,
            "Invalid file %s", stream->GetFileName().c_str());
        algorithm->Init();

        size_t offset = 0;
        size_t readLen = 0;
        while (offset < fileLen) {
            ret = stream->Read(buff, offset, buffSize, readLen);
            PKG_CHECK(ret == PKG_SUCCESS,
                packageManager->ClosePkgStream(stream); return ret,
                "read buffer fail %s", stream->GetFileName().c_str());
            algorithm->Update(buff, readLen);

            offset += readLen;
            readLen = 0;
        }
        PkgBuffer signBuffer(&digest, size);
        algorithm->Final(signBuffer);
        packageManager->ClosePkgStream(stream);
        return PKG_SUCCESS;
    }

static int CreatePackageZip(const std::vector<std::string> &fstabFile)
{
    int32_t ret = PKG_SUCCESS;
    uint32_t updateFileVersion = 1000;
    uint32_t componentInfoIdBase = 100;
    uint8_t componentInfoFlags = 22;
    std::string testPackageName = "test_package.zip";

    PKG_LOGI("\n\n ************* CreatePackageZip %s \r\n", testPackageName.c_str());
    UpgradePkgInfoExt pkgInfo;
    pkgInfo.softwareVersion = strdup("100.100.100.100");
    pkgInfo.date = strdup("2021-07-16");
    pkgInfo.time = strdup("13:14:00");
    pkgInfo.productUpdateId = strdup("555.555.100.555");
    pkgInfo.entryCount = fstabFile.size();
    pkgInfo.updateFileVersion = updateFileVersion;
    pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
    pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
    pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;
    std::string filePath;
    ComponentInfoExt comp[fstabFile.size()];
    for (size_t i = 0; i < fstabFile.size(); i++) {
        comp[i].componentAddr = strdup(fstabFile[i].c_str());
        filePath = TEST_PATH_TO;
        filePath += fstabFile[i].c_str();
        comp[i].filePath = strdup(filePath.c_str());
        comp[i].version = strdup("55555555");

        ret = BuildFileDigest(*comp[i].digest, sizeof(comp[i].digest), filePath);
        comp[i].size = GetFileSize(filePath);
        comp[i].originalSize = comp[i].size;
        comp[i].id = componentInfoIdBase;
        comp[i].resType = 1;
        comp[i].type = 1;
        comp[i].flags = componentInfoFlags;
        filePath.clear();
    }

    std::string packagePath = TEST_PATH_TO;
    packagePath += testPackageName;
    ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(), GetTestPrivateKeyName().c_str());
    if (ret == 0) {
        PKG_LOGI("CreatePackage success offset");
    }
    for (size_t i = 0; i < fstabFile.size(); i++) {
        free(comp[i].componentAddr);
        free(comp[i].filePath);
        free(comp[i].version);
    }
    free(pkgInfo.productUpdateId);
    free(pkgInfo.softwareVersion);
    free(pkgInfo.date);
    free(pkgInfo.time);
    return ret;
}

static int StartUpdaterProcFun(const std::string &patch)
{
    UpdaterStatus status;
    std::vector<std::string> components;
    int maxTemperature;
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();

    pkgManager->LoadPackage(patch, GetTestCertName(), components);
    status = StartUpdaterProc(pkgManager, patch, 0, maxTemperature);
    LOG(INFO) << "[fuzz] status " << status;

    return status;
}

namespace OHOS {
    bool FuzzStartUpdaterProc(const uint8_t* data, size_t size)
    {
        FILE *pFile;
        std::vector<std::string> fstabFile = {
            "build_tools.zip",
            "updater.txt"
        };

        pFile = fopen("updater.txt", "w+");
        if (pFile == nullptr) {
            LOG(ERROR) << "[fuzz]open file failed";
            return -1;
        }

        fwrite(data, 1, size, pFile);
        fclose(pFile);

        CreatePackageZip(fstabFile);
        StartUpdaterProcFun(TEST_PATH_TO + "test_package.zip");

        remove("updater.txt");
        remove("test_package.zip");

        return 0;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzStartUpdaterProc(data, size);
    return 0;
}

