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
#ifndef PKG_FILE_H
#define PKG_FILE_H

#include <map>
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_info_utils.h"
#include "pkg_utils.h"

namespace Hpackage {
class PkgFileImpl : public PkgFile {
public:
    PkgFileImpl(PkgManager::PkgManagerPtr manager, PkgStreamPtr stream, PkgType type)
        : type_(type), pkgStream_(stream), pkgManager_(manager) {}

    virtual ~PkgFileImpl();

    int32_t AddEntry(const FileInfoPtr file, const PkgStreamPtr input) override
    {
        UNUSED(file);
        UNUSED(input);
        return PKG_SUCCESS;
    }

    int32_t SavePackage(size_t &signOffset) override
    {
        UNUSED(signOffset);
        return PKG_SUCCESS;
    }

    int32_t ExtractFile(const PkgEntryPtr node, const PkgStreamPtr output) override;

    int32_t LoadPackage(std::vector<std::string> &fileNames, VerifyFunction verifier = nullptr) override
    {
        UNUSED(fileNames);
        UNUSED(verifier);
        return PKG_SUCCESS;
    }

    int32_t ParseComponents(std::vector<std::string> &fileNames) override
    {
        UNUSED(fileNames);
        return PKG_SUCCESS;
    }

    PkgEntryPtr FindPkgEntry(const std::string &fileName) override;

    PkgStreamPtr GetPkgStream() const override
    {
        return pkgStream_;
    }

    const PkgInfo *GetPkgInfo() const override
    {
        return nullptr;
    }

    PkgType GetPkgType() const override
    {
        return type_;
    }

    void ClearPkgStream() override
    {
        pkgStream_ = nullptr;
    }

    int32_t ReadImgHashDataFile(const std::string &pkgType) override
    {
        UNUSED(pkgType);
        return PKG_SUCCESS;
    }

    static int32_t ConvertBufferToString(std::string &fileName, const PkgBuffer &buffer);

    static int32_t ConvertStringToBuffer(const std::string &fileName, const PkgBuffer &buffer, size_t &realLen);

    void AddSignData(uint8_t digestMethod, size_t currOffset, size_t &signOffset);

protected:
    PkgEntryPtr AddPkgEntry(const std::string& fileName);
    bool CheckState(std::vector<uint32_t> states, uint32_t state);

protected:
    enum {
        PKG_FILE_STATE_IDLE = 0,
        PKG_FILE_STATE_WORKING, // 打包数据的状态
        PKG_FILE_STATE_CLOSE
    };

    PkgType type_ {};
    PkgStreamPtr pkgStream_ = nullptr;
    PkgManager::PkgManagerPtr pkgManager_ = nullptr;
    uint32_t nodeId_ = 0;
    std::map<uint32_t, PkgEntryPtr> pkgEntryMapId_ {};
    std::multimap<std::string, PkgEntryPtr, std::greater<std::string>> pkgEntryMapFileName_ {};
    uint32_t state_ = PKG_FILE_STATE_IDLE;
};
} // namespace Hpackage
#endif
