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

#ifndef UPDATE_BINARY_UNITTEST_H
#define UPDATE_BINARY_UNITTEST_H

#include "script_instruction.h"
#include "script_unittest.h"

using namespace Uscript;
using namespace Hpackage;

namespace UpdaterUt {
class UTestBinaryEnv : public Uscript::UScriptEnv {
public:
    explicit UTestBinaryEnv(PkgManager::PkgManagerPtr pkgManager) : Uscript::UScriptEnv(pkgManager) {}

    ~UTestBinaryEnv() = default;

    void PostMessage(const std::string &cmd, std::string content) {}

    UScriptInstructionFactoryPtr GetInstructionFactory()
    {
        return nullptr;
    }

    const std::vector<std::string> GetInstructionNames() const
    {
        return {};
    }

    bool IsRetry() const
    {
        return isRetry_;
    }

    bool SetRetry(bool retry)
    {
        return isRetry_ = retry;
    }

    UScriptInstructionFactory *factory_ = nullptr;
private:
    bool isRetry_ = false;
};

class TestPkgMgr : public TestScriptPkgManager {
public:
    int32_t ExtractFile(const std::string &fileId, StreamPtr output) override
    {
        return PKG_SUCCESS;
    }
    const FileInfo *GetFileInfo(const std::string &fileId) override
    {
        static FileInfo fileInfo {};
        if (fileId == "binary") {
            return &fileInfo;
        }
        return nullptr;
    }
};

class TestPkgMgrStream1 : public TestScriptPkgManager {
public:
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, size_t size,
        int32_t type) override
    {
        return PKG_ERROR_BASE;
    }
};

class TestPkgMgrStream2 : public TestScriptPkgManager {
public:
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, size_t size,
        int32_t type) override
    {
        stream = nullptr;
        return PKG_SUCCESS;
    }
};

class TestPkgMgrExtract1 : public TestScriptPkgManager {
public:
    int32_t ExtractFile(const std::string &fileId, StreamPtr output) override
    {
        return PKG_ERROR_BASE;
    }
};
}
#endif // UPDATE_BINARY_UNITTEST_H