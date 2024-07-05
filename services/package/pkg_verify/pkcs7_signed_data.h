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

#ifndef PKCS7_SIGNED_DATA_H
#define PKCS7_SIGNED_DATA_H

#include <vector>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include "pkg_manager.h"

namespace Hpackage {
using DataBuffer = struct {
    const unsigned char *buffer;
    size_t length;
};

using HwSigningSigntureInfo = struct {
    DataBuffer overall;
    DataBuffer hashResult;
    int nid;
};

struct Pkcs7SignerInfo {
    X509_NAME *issuerName = nullptr;
    ASN1_INTEGER *serialNumber = nullptr;
    int32_t digestNid {};
    int32_t digestEncryptNid {};
    std::vector<uint8_t> digestEncryptData;
    const unsigned char *buffer;
    size_t length;
};

class VerifyHelper {
public:
    virtual int32_t GetDigestFromSubBlocks(std::vector<uint8_t> &digestBlock,
        HwSigningSigntureInfo &signatureInfo, std::vector<uint8_t> &digest) = 0;

    virtual ~VerifyHelper() {}
};

class Pkcs7SignedData {
public:
    Pkcs7SignedData() : pkcs7_(nullptr), digest_(), signerInfos_(), signatureInfo() {}

    ~Pkcs7SignedData();

    int32_t GetHashFromSignBlock(const uint8_t *srcData, const size_t dataLen,
        std::vector<uint8_t> &hash);

    int32_t ParsePkcs7Data(const uint8_t *srcData, const size_t dataLen);

    int32_t Verify() const;

    int32_t Verify(const std::vector<uint8_t> &hash, const std::vector<uint8_t> &sig, bool sigInSignerInfo) const;

    void RegisterVerifyHelper(std::unique_ptr<VerifyHelper> ptr);

    static Pkcs7SignedData &GetInstance();

    int32_t GetDigest(std::vector<uint8_t> &digestBlock,
        HwSigningSigntureInfo &signatureInfo, std::vector<uint8_t> &digest);

    int32_t ReadSig(const uint8_t *sourceData, const uint32_t sourceData, std::vector<uint8_t> &sig);
private:
    int32_t Init(const uint8_t *sourceData, const uint32_t sourceDataLen);
    int32_t DoParse();
    int32_t ParseContentInfo(std::vector<uint8_t> &digestBlock) const;
    int32_t GetDigestFromContentInfo(std::vector<uint8_t> &digestBlock);
    int32_t DoUpdateVerify(std::vector<uint8_t> &digestBlock);
    int32_t SignerInfosParse();
    int32_t SignerInfoParse(PKCS7_SIGNER_INFO *p7SignerInfo, Pkcs7SignerInfo &signerInfo);
    int32_t Pkcs7SignleSignerVerify(const Pkcs7SignerInfo &signerInfo, const std::vector<uint8_t> &hash,
        const std::vector<uint8_t> &sig) const;
    int32_t VerifyDigest(X509 *cert, const Pkcs7SignerInfo &signer, const std::vector<uint8_t> &hash,
        const std::vector<uint8_t> &sig) const;
private:
    PKCS7 *pkcs7_;
    std::vector<uint8_t> digest_;
    std::vector<Pkcs7SignerInfo> signerInfos_;
    HwSigningSigntureInfo signatureInfo;
    std::unique_ptr<VerifyHelper> helper_ {};
};

class Pkcs7VerifyHelper : public VerifyHelper {
public:
    Pkcs7VerifyHelper() = default;

    ~Pkcs7VerifyHelper() override;

    int32_t GetDigestFromSubBlocks(std::vector<uint8_t> &digestBlock,
        HwSigningSigntureInfo &signatureInfo, std::vector<uint8_t> &digest) override;
};
} // namespace Hpackage
#endif
