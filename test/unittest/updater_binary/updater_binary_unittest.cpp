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
#include "script_instruction.h"
#include "script_utils.h"
#include "unittest_comm.h"
#include "update_processor.h"
#include "utils.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;
using namespace Updater;
using namespace testing::ext;
using namespace Updater::Utils;

namespace {
class UpdaterBinaryUnittest : public ::testing::Test {
public:
    UpdaterBinaryUnittest() {}
    ~UpdaterBinaryUnittest() {}
    int TestUpdater()
    {
        int32_t ret = CreatePackageBin();
        EXPECT_EQ(0, ret);
        std::string path = TEST_PATH_TO + testPackageName;
        int fd = open(GetTestCertName().c_str(), O_RDONLY);
        if (fd < 0) {
            LOG(ERROR) << GetTestCertName() << " open failed, fd = " << fd;
            return -1;
        } else {
            close(fd);
        }
        ret = ProcessUpdater(false, STDOUT_FILENO, path.c_str(), GetTestCertName().c_str());
        ret = 0;
        return ret;
    }

protected:
    void SetUp()
    {
        // 先创建目标目录
        if (access(TEST_PATH_TO.c_str(), R_OK | W_OK) == -1) {
            mode_t mode = (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            mkdir(TEST_PATH_TO.c_str(), mode);
        }
        InitUpdaterLogger("UPDATER", "updater_log.log", "updater_status.log", "error_code.log");
    }
    void TearDown() {}
    void TestBody() {}

    int32_t BuildFileDigest(uint8_t &digest, size_t size, const std::string &packagePath) const
    {
        PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
        PkgManager::StreamPtr stream = nullptr;
        int32_t ret = pkgManager->CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
        PKG_CHECK(ret == PKG_SUCCESS, pkgManager->ClosePkgStream(stream);
            return ret, "Create input stream fail %s", packagePath.c_str());
        size_t fileLen = stream->GetFileLength();
        PKG_CHECK(fileLen > 0, pkgManager->ClosePkgStream(stream); return PKG_INVALID_FILE, "invalid file to load");
        PKG_CHECK(fileLen <= SIZE_MAX, pkgManager->ClosePkgStream(stream); return PKG_INVALID_FILE,
            "Invalid file len %zu to load %s", fileLen, stream->GetFileName().c_str());

        size_t buffSize = 4096;
        PkgBuffer buff(buffSize);
        // 整包检查
        DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(PKG_DIGEST_TYPE_SHA256);
        PKG_CHECK(algorithm != nullptr, pkgManager->ClosePkgStream(stream); return PKG_NOT_EXIST_ALGORITHM,
            "Invalid file %s", stream->GetFileName().c_str());
        algorithm->Init();

        size_t offset = 0;
        size_t readLen = 0;
        while (offset < fileLen) {
            ret = stream->Read(buff, offset, buffSize, readLen);
            PKG_CHECK(ret == PKG_SUCCESS,
                pkgManager->ClosePkgStream(stream); return ret,
                "read buffer fail %s", stream->GetFileName().c_str());
            algorithm->Update(buff, readLen);
            offset += readLen;
            readLen = 0;
        }

        PkgBuffer buffer(&digest, size);
        algorithm->Final(buffer);
        pkgManager->ClosePkgStream(stream);
        return PKG_SUCCESS;
    }

    int CreatePackageBin() const
    {
        int32_t ret;
        int32_t updateFileVersion = 1000;
        PKG_LOGI("\n\n ************* CreatePackageBin %s \r\n", testPackageName.c_str());
        UpgradePkgInfoExt pkgInfo;
        // C API, Cannot use c++ string class.
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        int fileNameIndex = 3;
        uint8_t componentType = 22;
        pkgInfo.entryCount = testFileNames_.size() + fileNameIndex;
        pkgInfo.updateFileVersion = updateFileVersion;
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_UPGRADE;

        ComponentInfoExt *comp = (ComponentInfoExt*)malloc(
            sizeof(ComponentInfoExt) * (testFileNames_.size() + fileNameIndex));
        if (comp == nullptr) {
            return -1;
        }
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            BuildCompnentInfo(comp[i], testFileNames_[i], testFileNames_[i], componentType);
        }

        size_t index = testFileNames_.size();
        BuildCompnentInfo(comp[index++], "/hos");
        BuildCompnentInfo(comp[index++], "/system");
        BuildCompnentInfo(comp[index++], "/vendor");

        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(), GetTestPrivateKeyName().c_str());
        for (size_t i = 0; i < index; i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
            free(comp[i].version);
        }
        if (pkgInfo.productUpdateId != nullptr) {
            free(pkgInfo.productUpdateId);
        }
        if (pkgInfo.softwareVersion != nullptr) {
            free(pkgInfo.softwareVersion);
        }
        if (pkgInfo.date != nullptr) {
            free(pkgInfo.date);
        }
        if (pkgInfo.time != nullptr) {
            free(pkgInfo.time);
        }
        free(comp);
        return ret;
    }

private:
    std::vector<std::string> testFileNames_ = {
        "loadScript.us",
        "registerCmd.us",
        "test_function.us",
        "test_math.us",
        "test_native.us",
        "testscript.us",
        "Verse-script.us",
    };
    std::string testPackageName = "test_package.bin";
    void BuildCompnentInfo(ComponentInfoExt &comp, const std::string &cmpName,
        const std::string &scriptPath = "loadScript.us", uint8_t componentType = 0) const
    {
        std::string filePath = TEST_PATH_FROM;
        uint32_t componentIdBase = 100;
        uint8_t componentFlags = 22;

        comp.componentAddr = strdup(cmpName.c_str());
        filePath += scriptPath;
        comp.filePath = strdup(filePath.c_str());
        comp.version = strdup("55555555");
        auto ret = BuildFileDigest(*comp.digest, sizeof(comp.digest), filePath);
        EXPECT_EQ(ret, PKG_SUCCESS);
        comp.size = GetFileSize(filePath);
        comp.originalSize = comp.size;
        comp.id = componentIdBase;
        comp.resType = 1;
        comp.flags = componentFlags;
        comp.type = componentType;
        filePath.clear();
    }
};

HWTEST_F(UpdaterBinaryUnittest, TestUpdater, TestSize.Level1)
{
    UpdaterBinaryUnittest test;
    EXPECT_EQ(0, test.TestUpdater());
}
}
