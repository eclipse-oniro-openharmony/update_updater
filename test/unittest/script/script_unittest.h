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

#ifndef SCRIPT_UNITTEST_H
#define SCRIPT_UNITTEST_H

#include <string>
#include "pkg_manager.h"

namespace Hpackage {
class TestScriptPkgManager : public PkgManager {
public:
    int32_t CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
        std::vector<std::pair<std::string, ZipFileInfo>> &files) override
    {
        return PKG_SUCCESS;
    }
    int32_t CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
        std::vector<std::pair<std::string, ComponentInfo>> &files) override
    {
        return PKG_SUCCESS;
    }
    int32_t CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
        std::vector<std::pair<std::string, Lz4FileInfo>> &files) override
    {
        return PKG_SUCCESS;
    }
    int32_t VerifyPackage(const std::string &packagePath, const std::string &keyPath,
        const std::string &version, const PkgBuffer &digest, VerifyCallback cb) override
    {
        return PKG_SUCCESS;
    }
    int32_t LoadPackage(const std::string &packagePath, const std::string &keyPath,
        std::vector<std::string> &fileIds) override
    {
        return PKG_SUCCESS;
    }
    int32_t VerifyOtaPackage(const std::string &packagePath) override
    {
        return PKG_SUCCESS;
    }
    int32_t VerifyBinFile(const std::string &packagePath, const std::string &keyPath,
        const std::string &version, const PkgBuffer &digest) override
    {
        return PKG_SUCCESS;
    }
    const PkgInfo *GetPackageInfo(const std::string &packagePath) override
    {
        return nullptr;
    }
    int32_t ExtractFile(const std::string &fileId, StreamPtr output) override
    {
        return PKG_SUCCESS;
    }
    const FileInfo *GetFileInfo(const std::string &fileId) override
    {
        return nullptr;
    }
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, size_t size,
        int32_t type) override
    {
        return PKG_SUCCESS;
    }
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName,
        PkgStream::ExtractFileProcessor processor, const void *context) override
    {
        return PKG_SUCCESS;
    }
    int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, const PkgBuffer &buffer) override
    {
        return PKG_SUCCESS;
    }
    void ClosePkgStream(StreamPtr &stream) override { }
    int32_t DecompressBuffer(FileInfoPtr info, const PkgBuffer &buffer, StreamPtr output) const override
    {
        return PKG_SUCCESS;
    }
    int32_t CompressBuffer(FileInfoPtr info, const PkgBuffer &buffer, StreamPtr output) const override
    {
        return PKG_SUCCESS;
    }
    int32_t LoadPackageWithoutUnPack(const std::string &packagePath,
        std::vector<std::string> &fileIds) override
    {
        return PKG_SUCCESS;
    }
    int32_t ParsePackage(StreamPtr stream, std::vector<std::string> &fileIds, int32_t type) override
    {
        return PKG_SUCCESS;
    }
    void SetPkgDecodeProgress(PkgDecodeProgress decodeProgress) override {}
    void PostDecodeProgress(int type, size_t writeDataLen, const void *context) override {}
};
}
#endif // SCRIPT_UNITTEST_H