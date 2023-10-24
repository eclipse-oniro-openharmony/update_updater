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

#include "scriptmanager_fuzzer.h"

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
#include "hash_data_verifier.h"
#include "log.h"
#include "pkg_stream.h"
#include "pkg_utils.h"
#include "scope_guard.h"
#include "script_instructionhelper.h"
#include "script_manager_impl.h"
#include "script_manager.h"
#include "script_unittest.h"
#include "script_utils.h"
#include "utils.h"

using namespace Hpackage;
using namespace Uscript;
using namespace Updater;

namespace OHOS {
constexpr int32_t SCRIPT_TEST_PRIORITY_NUM = 3;
constexpr int32_t SCRIPT_TEST_LAST_PRIORITY = 2;
const std::string FUZZ_TEST_PATH_FROM = "/data/fuzz/test/";

class TestPkgManager : public TestScriptPkgManager {
public:
    const FileInfo *GetFileInfo(const std::string &fileId) override
    {
        static FileInfo fileInfo {};
        static std::vector<std::string> testFileNames = {
            "loadScript.us",
            "test_function.us",
            "test_if.us",
            "test_logic.us",
            "testscript.us",
            "test_math.us",
            "test_native.us",
            "Verse-script.us",
            "test_script.us"
        };
        if (std::find(testFileNames.begin(), testFileNames.end(), fileId) != testFileNames.end()) {
            return &fileInfo;
        }
        if (fileId == "hash_signed_data") {
            fileInfo.unpackedSize = GetFileSize(FUZZ_TEST_PATH_FROM + fileId);
            return &fileInfo;
        }
        return nullptr;
    }
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, const PkgBuffer &buffer) override
    {
        PKG_LOGI("create pkg stream success for %s ", fileName.c_str());
        stream = new MemoryMapStream(this, fileName, buffer, PkgStream::PkgStreamType_Buffer);
        return PKG_SUCCESS;
    }
    int32_t ExtractFile(const std::string &fileId, StreamPtr output) override
    {
        if (fileId != "hash_signed_data") {
            return PKG_SUCCESS;
        }
        if (output == nullptr) {
            return PKG_INVALID_STREAM;
        }
        auto fd = open((FUZZ_TEST_PATH_FROM + fileId).c_str(), O_RDWR);
        if (fd == -1) {
            PKG_LOGE("file %s not existed", (FUZZ_TEST_PATH_FROM + fileId).c_str());
            return PKG_INVALID_FILE;
        }
        ON_SCOPE_EXIT(close) {
            close(fd);
        };
        std::string content {};
        if (!Utils::ReadFileToString(fd, content)) {
            PKG_LOGE("read file to string failed");
            return PKG_INVALID_FILE;
        }
        auto stream = static_cast<MemoryMapStream *>(output);
        PkgBuffer buffer = {};
        stream->GetBuffer(buffer);
        if (content.size() + 1 != buffer.length) {
            PKG_LOGE("content size is not valid, %u != %u", content.size(), buffer.data.size());
            return PKG_INVALID_FILE;
        }
        std::copy(content.begin(), content.end(), buffer.buffer);
        return PKG_SUCCESS;
    }
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName,
         size_t size, int32_t type) override
    {
        FILE *file = nullptr;
        std::string realFileName = fileName;
        auto pos = fileName.rfind('/');
        if (pos != std::string::npos) {
            realFileName = fileName.substr(pos + 1);
        }
        char realPath[PATH_MAX + 1] = {};
        if (realpath((FUZZ_TEST_PATH_FROM + realFileName).c_str(), realPath) == nullptr) {
            LOG(ERROR) << (FUZZ_TEST_PATH_FROM + realFileName) << " realpath failed";
            return PKG_INVALID_FILE;
        }
        file = fopen(realPath, "rb");
        if (file != nullptr) {
            stream = new FileStream(this, realFileName, file, PkgStream::PkgStreamType_Read);
            return USCRIPT_SUCCESS;
        }
        PKG_LOGE("Fail to open file %s ", realFileName.c_str());
        return PKG_INVALID_FILE;
    }
    void ClosePkgStream(StreamPtr &stream) override
    {
        delete stream;
    }
};

class TestScriptInstructionFactory : public UScriptInstructionFactory {
public:
    virtual int32_t CreateInstructionInstance(UScriptInstructionPtr& instr, const std::string& name)
    {
        return USCRIPT_SUCCESS;
    }
    virtual void DestoryInstructionInstance(UScriptInstructionPtr& instr)
    {
        delete instr;
        instr = nullptr;
    }
    TestScriptInstructionFactory() {}
    virtual ~TestScriptInstructionFactory() {}
};

class FuzzTestScriptEnv : public UScriptEnv {
public:
    explicit FuzzTestScriptEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager) : UScriptEnv(pkgManager) {}
    ~FuzzTestScriptEnv()
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
        return {};
    }

    virtual bool IsRetry() const
    {
        return isRetry;
    }

    virtual PostMessageFunction GetPostmsgFunc()
    {
        return nullptr;
    }
    UScriptInstructionFactory *factory_ = nullptr;
private:
    bool isRetry = false;
};

class FuzzScriptTest {
public:
    FuzzScriptTest()
    {
        InitUpdaterLogger("UPDATER", "updater_log.log", "updater_status.log", "error_code.log");
    }

    ~FuzzScriptTest()
    {
        ScriptManager::ReleaseScriptManager();
    }

    int TestUscriptExecute()
    {
        int32_t ret {};
        TestPkgManager packageManager;
        auto env = std::make_unique<FuzzTestScriptEnv>(&packageManager);
        HashDataVerifier verifier {&packageManager};
        verifier.LoadHashDataAndPkcs7(FUZZ_TEST_PATH_FROM + "updater_fake_pkg.zip");
        ScriptManager *manager = ScriptManager::GetScriptManager(env.get(), &verifier);
        if (manager == nullptr) {
            USCRIPT_LOGI("create manager fail ret:%d", ret);
            return USCRIPT_INVALID_SCRIPT;
        }
        int32_t priority = SCRIPT_TEST_PRIORITY_NUM;
        ret = manager->ExecuteScript(priority);
        USCRIPT_LOGI("ExecuteScript ret:%d", ret);
        priority = 0;
        ret = manager->ExecuteScript(priority);
        priority = 1;
        ret = manager->ExecuteScript(priority);
        priority = SCRIPT_TEST_LAST_PRIORITY;
        ret = manager->ExecuteScript(priority);
        ScriptManager::ReleaseScriptManager();
        return ret;
    }

protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

private:
    std::vector<std::string> testFileNames_ = {
        "loadScript.us",
        "registerCmd.us",
        "test_function.us",
        "test_if.us",
        "test_logic.us",
        "test_math.us",
        "test_native.us",
        "test_script.us"
        "testscript.us",
        "Verse-script.us",
    };
};

void FuzzScriptManager(const uint8_t *data, size_t size)
{
    FuzzScriptTest test;
    test.TestUscriptExecute();
}
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzScriptManager(data, size);
    return 0;
}

