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

#include <climits>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hash_data_verifier.h"
#include "log.h"
#include "pkg_stream.h"
#include "pkg_utils.h"
#include "scope_guard.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "script/script_unittest.h"
#include "script_utils.h"
#include "unittest_comm.h"
#include "utils.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;
using namespace Updater;
using namespace testing::ext;

namespace {
constexpr int32_t SCRIPT_TEST_PRIORITY_NUM = 3;
constexpr int32_t SCRIPT_TEST_LAST_PRIORITY = 2;

class TestPkgManager : public TestScriptPkgManager {
public:
    const FileInfo *GetFileInfo(const std::string &fileId) override
    {
        static FileInfo fileInfo {};
        static std::vector<std::string> testFileNames = {
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
        if (fileId == "hash_signed_data") {
            fileInfo.unpackedSize = GetFileSize(TEST_PATH_FROM + fileId);
            return &fileInfo;
        }
        if (std::find(testFileNames.begin(), testFileNames.end(), fileId) != testFileNames.end()) {
            return &fileInfo;
        }
        return nullptr;
    }
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, const PkgBuffer &buffer) override
    {
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
        auto stream = static_cast<MemoryMapStream *>(output);
        auto fd = open((TEST_PATH_FROM + fileId).c_str(), O_RDWR);
        if (fd == -1) {
            PKG_LOGE("file %s not existed", (TEST_PATH_FROM + fileId).c_str());
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
        PkgBuffer buffer {};
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
        std::string fileNameReal = fileName;
        auto pos = fileName.rfind('/');
        if (pos != std::string::npos) {
            fileNameReal = fileName.substr(pos + 1);
        }
        char realPath[PATH_MAX + 1] = {};
        if (realpath((TEST_PATH_FROM + fileNameReal).c_str(), realPath) == nullptr) {
            LOG(ERROR) << (TEST_PATH_FROM + fileNameReal) << " realpath failed";
            return PKG_INVALID_FILE;
        }
        file = fopen(realPath, "rb");
        if (file == nullptr) {
            PKG_LOGE("Fail to open file %s ", fileNameReal.c_str());
            return PKG_INVALID_FILE;
        }
        stream = new FileStream(this, fileNameReal, file, PkgStream::PkgStreamType_Read);
        return USCRIPT_SUCCESS;
    }
    void ClosePkgStream(StreamPtr &stream) override
    {
        delete stream;
    }
};


class TestScriptInstructionSparseImageWrite : public Uscript::UScriptInstruction {
public:
    TestScriptInstructionSparseImageWrite() {}
    virtual ~TestScriptInstructionSparseImageWrite() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override
    {
        // 从参数中获取分区信息
        std::string partitionName;
        int32_t ret = context.GetParam(0, partitionName);
        if (ret != USCRIPT_SUCCESS) {
            LOG(ERROR) << "Error to get param";
            return ret;
        }
        LOG(INFO) << "UScriptInstructionSparseImageWrite::Execute " << partitionName;
        if (env.GetPkgManager() == nullptr) {
            LOG(ERROR) << "Error to get pkg manager";
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
        instr = nullptr;
    }
    TestScriptInstructionFactory() {}
    virtual ~TestScriptInstructionFactory() {}
};

class UTestScriptEnv : public UScriptEnv {
public:
    explicit UTestScriptEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager) : UScriptEnv(pkgManager) {}
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

    virtual PostMessageFunction GetPostmsgFunc() override
    {
        return nullptr;
    }
    UScriptInstructionFactory *factory_ = nullptr;
private:
    bool isRetry = false;
};

class UScriptTest : public ::testing::Test {
public:
    UScriptTest() {}

    ~UScriptTest()
    {
        ScriptManager::ReleaseScriptManager();
    }

    int TestUscriptExecute()
    {
        int32_t ret {};
        TestPkgManager packageManager;
        UTestScriptEnv *env = new UTestScriptEnv(&packageManager);
        HashDataVerifier verifier {&packageManager};
        verifier.LoadHashDataAndPkcs7(TEST_PATH_FROM + "updater_fake_pkg.zip");
        ScriptManager *manager = ScriptManager::GetScriptManager(env, &verifier);
        if (manager == nullptr) {
            USCRIPT_LOGI("create manager fail ret:%d", ret);
            delete env;
            return USCRIPT_INVALID_SCRIPT;
        }
        int32_t priority = SCRIPT_TEST_PRIORITY_NUM;
        ret = manager->ExecuteScript(priority);
        EXPECT_EQ(ret, USCRIPT_ABOART);
        USCRIPT_LOGI("ExecuteScript ret:%d", ret);
        priority = 0;
        ret = manager->ExecuteScript(priority);
        priority = 1;
        ret = manager->ExecuteScript(priority);
        priority = SCRIPT_TEST_LAST_PRIORITY;
        ret = manager->ExecuteScript(priority);
        delete env;
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
        "testscript.us",
        "Verse-script.us",
        "test_script.us"
    };
};

HWTEST_F(UScriptTest, TestUscriptExecute, TestSize.Level1)
{
    UScriptTest test;
    EXPECT_EQ(0, test.TestUscriptExecute());
}
}
