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

#include "extractandexecutescript_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "pkg_algorithm.h"
#include "pkg_algo_digest.h"
#include "pkg_utils.h"
#include "script_instructionhelper.h"
#include "script_manager_impl.h"
#include "script_utils.h"


using namespace Hpackage;
using namespace Uscript;
using namespace Updater;

const static std::string TEST_PATH_TO = "/data/fuzz/test/";
const static int32_t SCRIPT_TEST_PRIORITY_NUM = 3;
const static int32_t SCRIPT_TEST_LAST_PRIORITY = 2;

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

class TestScriptInstructionSparseImageWrite : public uscript::UScriptInstruction {
public:
    TestScriptInstructionSparseImageWrite() {}
    virtual ~TestScriptInstructionSparseImageWrite() {}
    int32_t Execute(uscript::UScriptEnv &env, uscript::UScriptContext &context) override
    {
        /* 从参数中获取分区信息 */
        std::string partitionName;
        int32_t ret = context.GetParam(0, partitionName);
        if (ret != USCRIPT_SUCCESS) {
            return ret;
        }
        if (env.GetPkgManager() == nullptr) {
            return USCRIPT_ERROR_EXECUTE;
        }
        return ret;
    }
};

class TestScriptInstructionFactory : public UScriptInstructionFactory {
public:
    virtual int32_t CreateInstructionInstance(UScriptInstructionPtr& instr, const std::string& name)
    {
        if (name == "sparse_image_write") {
            instr = new TestScriptInstructionSparseImageWrite();
        }
        return USCRIPT_SUCCESS;
    }
    virtual void DestoryInstructionInstance(UScriptInstructionPtr& instr)
    {
        delete instr;
    }
    TestScriptInstructionFactory() {}
    virtual ~TestScriptInstructionFactory() {}
};

class UTestScriptEnv : public UScriptEnv {
public:
    UScriptInstructionFactory *factory_ = nullptr;
    explicit UTestScriptEnv(hpackage::PkgManager::PkgManagerPtr pkgManager) : UScriptEnv(pkgManager) {}
    ~UTestScriptEnv()
    {
        if (factory_ != nullptr) {
            delete factory_;
            factory_ = nullptr;
        }
    }

    virtual void PostMessage(const std::string &cmd, std::string content) {}

    virtual UScriptInstructionFactoryPtr GetInstructionFactory()
    {
        if (factory_ == nullptr) {
            factory_ = new TestScriptInstructionFactory();
        }
        return factory_;
    }

    virtual const std::vector<std::string> GetInstructionNames() const
    {
        static std::vector<std::string> updaterCmds = {"sparse_image_write"};
        return updaterCmds;
    }

    virtual bool IsRetry() const
    {
        return isRetry;
    }
private:
    bool isRetry = false;
};

class UScriptTest {
public:
    UScriptTest()
    {
        packageManager = PkgManager::GetPackageInstance();
    }

    ~UScriptTest()
    {
        packageManager = PkgManager::GetPackageInstance();
        PkgManager::ReleasePackageInstance(packageManager);
        ScriptManager::ReleaseScriptManager();
    }

    int TestUscriptExecute(const std::vector<std::string> &inputFile)
    {
        CreatePackageBin(inputFile);
        packageManager = PkgManager::GetPackageInstance();
        if (packageManager == nullptr) {
            return PKG_SUCCESS;
        }
        std::vector<std::string> components;
        int32_t ret = packageManager->LoadPackage(TEST_PATH_TO + testPackageName, GetTestCertName(), components);
        if (ret != USCRIPT_SUCCESS) {
            USCRIPT_LOGI("LoadPackage fail ret:%d", ret);
            return USCRIPT_INVALID_SCRIPT;
        }

        UTestScriptEnv* env = new UTestScriptEnv(packageManager);
        ScriptManager* manager = ScriptManager::GetScriptManager(env);
        if (manager == nullptr) {
            USCRIPT_LOGI("create manager fail ret:%d", ret);
            delete env;
            return USCRIPT_INVALID_SCRIPT;
        }
        int32_t priority = SCRIPT_TEST_PRIORITY_NUM;
        ret = manager->ExecuteScript(priority);
        USCRIPT_LOGI("ExecuteScript ret:%d", ret);
        priority = 0;
        ret = manager->ExecuteScript(priority);
        USCRIPT_LOGI("ExecuteScript ret:%d", ret);
        priority = 1;
        ret = manager->ExecuteScript(priority);
        priority = SCRIPT_TEST_LAST_PRIORITY;
        ret = manager->ExecuteScript(priority);
        delete env;
        ScriptManager::ReleaseScriptManager();
        return ret;
    }

protected:
    int32_t BuildFileDigest(uint8_t &digest, size_t size, const std::string &packagePath)
    {
        PkgManager::StreamPtr stream = nullptr;
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

    int CreatePackageBin(const std::vector<std::string> &inputFile)
    {
        int32_t ret = PKG_SUCCESS;
        uint32_t updateFileVersion = 1000;
        uint32_t componentInfoIdBase = 100;
        uint8_t componentInfoFlags = 22;
        PKG_LOGI("\n\n ************* CreatePackageBin %s \r\n", testPackageName.c_str());
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.softwareVersion = strdup("100.100.100.100");
        pkgInfo.date = strdup("2021-02-02");
        pkgInfo.time = strdup("21:23:49");
        pkgInfo.productUpdateId = strdup("555.555.100.555");
        pkgInfo.entryCount = inputFile.size();
        pkgInfo.updateFileVersion = updateFileVersion;
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
        pkgInfo.pkgType = PKG_PACK_TYPE_UPGRADE;
        std::string filePath;
        ComponentInfoExt comp[inputFile.size()];
        for (size_t i = 0; i < inputFile.size(); i++) {
            comp[i].componentAddr = strdup(inputFile[i].c_str());
            filePath = TEST_PATH_TO;
            filePath += inputFile[i].c_str();
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
        for (size_t i = 0; i < inputFile.size(); i++) {
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

private:
    PkgManager::PkgManagerPtr packageManager = nullptr;
    std::string testPackageName = "test_package.bin";
};

static void ExtractAndExecuteScriptFun(const std::vector<std::string> &inputFile)
{
    UScriptTest test;
    test.TestUscriptExecute(inputFile);
}

namespace OHOS {
    bool FuzzExtractAndExecuteScript(const uint8_t* data, size_t size)
    {
        FILE *pFile;
        std::vector<std::string> inputFile = {
            "loadScript.us",
            "registerCmd.us",
            "test_function.us",
            "test_if.us",
            "test_logic.us",
            "test_math.us",
            "test_native.us",
            "testscript.us",
            "Verse-script.us",
            "test_script.us"
        };

        pFile = fopen("test_script.us", "w+");
        if (pFile == nullptr) {
            LOG(ERROR) << "[fuzz]open file failed";
            return -1;
        }

        (void)fwrite(data, 1, size, pFile);
        (void)fclose(pFile);

        ExtractAndExecuteScriptFun(inputFile);
        (void)remove("test_script.us");
        return 0;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzExtractAndExecuteScript(data, size);
    return 0;
}

