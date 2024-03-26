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
#ifndef UPGRADE_PKG_FILE_H
#define UPGRADE_PKG_FILE_H

#include <map>
#include "rust/image_hash_check.h"
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_info_utils.h"
#include "pkg_pkgfile.h"
#include "pkg_utils.h"

namespace Hpackage {
struct __attribute__((packed)) PkgTlv {
    uint16_t type;
    uint16_t length;
};

struct __attribute__((packed)) UpgradePkgHeader {
    uint32_t pkgInfoLength; // UpgradePkgTime + UpgradeCompInfo + UPGRADE_RESERVE_LEN
    uint32_t updateFileVersion;
    uint8_t productUpdateId[64];
    uint8_t softwareVersion[64];
};

struct __attribute__((packed)) UpgradePkgTime {
    uint8_t date[16];
    uint8_t time[16];
};

struct __attribute__((packed)) UpgradeCompInfo {
    uint8_t address[32]; // L1 16
    uint16_t id;
    uint8_t resType;
    uint8_t flags;
    uint8_t type;
    uint8_t version[10];
    uint32_t size;
    uint32_t originalSize;
    uint8_t digest[DIGEST_MAX_LEN];
};

struct __attribute__((packed))  UpgradeParam {
    size_t readLen {};
    size_t dataOffset {};
    size_t srcOffset {};
    size_t currLen {};
};

enum {
    UPGRADE_FILE_VERSION_V1 = 1,     // bin v1 version
    UPGRADE_FILE_VERSION_V2,        // bin v2 version, add img hash part
    UPGRADE_FILE_VERSION_V3,        // bin v3 version, modify img hash part
    UPGRADE_FILE_VERSION_V4,        // bin v4 version, modify bin file signature
};

class UpgradeFileEntry : public PkgEntry {
public:
    UpgradeFileEntry(PkgFilePtr pkgFile, uint32_t nodeId) : PkgEntry(pkgFile, nodeId) {}

    ~UpgradeFileEntry() override {}

    int32_t Init(const PkgManager::FileInfoPtr fileInfo, PkgStreamPtr inStream) override;

    int32_t EncodeHeader(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen) override;

    int32_t Pack(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen) override;

    int32_t DecodeHeader(PkgBuffer &buffer, size_t headerOffset, size_t dataOffset,
        size_t &decodeLen) override;

    int32_t Unpack(PkgStreamPtr outStream) override;

    int32_t Verify(PkgBuffer &buffer, size_t len, size_t offset);

    const FileInfo *GetFileInfo() const override
    {
        return &fileInfo_.fileInfo;
    }

protected:
    ComponentInfo fileInfo_ {};

private:
    int32_t GetUpGradeCompInfo(UpgradeCompInfo &comp);
};

class UpgradePkgFile : public PkgFileImpl {
public:
    UpgradePkgFile(PkgManager::PkgManagerPtr manager, PkgStreamPtr stream, PkgInfoPtr header)
        : PkgFileImpl(manager, stream, PkgFile::PKG_TYPE_UPGRADE)
    {
        if (header == nullptr || header->entryCount == 0) {
            return;
        }
        UpgradePkgInfo *info = (UpgradePkgInfo *)(header);
        pkgInfo_ = std::move(*info);
    }

    ~UpgradePkgFile() override
    {
#ifndef DIFF_PATCH_SDK
        if (hashCheck_ != nullptr) {
            if (pkgInfo_.updateFileVersion >= UPGRADE_FILE_VERSION_V3) {
                ReleaseImgHashDataNew(hashCheck_);
                return;
            }
            ReleaseImgHashData(hashCheck_);
        }
#endif
    }

    int32_t AddEntry(const PkgManager::FileInfoPtr file, const PkgStreamPtr inStream) override;

    int32_t SavePackage(size_t &signOffset) override;

    int32_t LoadPackage(std::vector<std::string> &fileNames, VerifyFunction verify = nullptr) override;

    size_t GetUpgradeSignatureLen() const;

    size_t GetDigestLen() const;

    const PkgInfo *GetPkgInfo() const override
    {
        return &pkgInfo_.pkgInfo;
    }

    const ImgHashData *GetImgHashData() const
    {
        return hashCheck_;
    }

    PkgManager::PkgManagerPtr GetPkgMgr() const
    {
        return pkgManager_;
    }

    int32_t GetUpgradeFileVer() const
    {
        return pkgInfo_.updateFileVersion;
    }

private:
    int16_t GetPackageTlvType();
    int32_t SaveEntry(const PkgBuffer &buffer, size_t &parsedLen, UpgradeParam &info,
        DigestAlgorithm::DigestAlgorithmPtr algorithm, std::vector<std::string> &fileNames);
    int32_t ReadComponents(size_t &parsedLen,
        DigestAlgorithm::DigestAlgorithmPtr algorithm, std::vector<std::string> &fileNames);

    void ParsePkgHeaderToTlv(const PkgBuffer &buffer, size_t &currLen, PkgTlv &tlv);
    int32_t ReadUpgradePkgHeader(size_t &realLen,
        DigestAlgorithm::DigestAlgorithmPtr &algorithm);

    int32_t Verify(size_t start, DigestAlgorithm::DigestAlgorithmPtr algorithm,
        VerifyFunction verifier, const std::vector<uint8_t> &signData);
    int32_t WriteBuffer(std::vector<uint8_t> &buffer, size_t &offset, size_t &signOffset);
    int32_t CheckPackageHeader(std::vector<uint8_t> &buffer, size_t &offset);
    int32_t GetEntryOffset(size_t &dataOffset, const PkgManager::FileInfoPtr file);
    int32_t ReadPackageInfo(std::vector<uint8_t> &signData,
        size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr algorithm);
    int32_t ReadReserveData(size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr &algorithm);
    int32_t ReadImgHashTLV(std::vector<uint8_t> &imgHashBuf, size_t &parsedLen,
                                        DigestAlgorithm::DigestAlgorithmPtr algorithm, uint32_t needType);
    int32_t ReadImgHashData(size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr algorithm);
    int32_t ReadSignData(std::vector<uint8_t> &signData,
                         size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr algorithm);
    int32_t VerifyHeader(DigestAlgorithm::DigestAlgorithmPtr algorithm, VerifyFunction verifier,
        const std::vector<uint8_t> &signData);
    int32_t VerifyFile(size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr algorithm,
                       VerifyFunction verifier);
    int32_t VerifyFileV1(size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr algorithm,
                         VerifyFunction verifier);
    int32_t VerifyFileV2(size_t &parsedLen, DigestAlgorithm::DigestAlgorithmPtr algorithm,
                         VerifyFunction verifier);

private:
    UpgradePkgInfo pkgInfo_ {};
    size_t packedFileSize_ {0};
    const ImgHashData *hashCheck_ = nullptr;
};
} // namespace Hpackage
#endif
