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

#include <openssl/evp.h>
#include <openssl/pkcs7.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include "pkg_manager.h"

namespace Hpackage {
struct Pkcs7SignerInfo {
    int32_t digestNid {};
    int32_t digestEncryptNid {};
    std::vector<uint8_t> digestEncryptData;
};

class Pkcs7SignedData {
public:
    Pkcs7SignedData();

    ~Pkcs7SignedData();

    int32_t BuildPkcs7SignedData(Hpackage::PkgBuffer &p7Data, uint32_t signMethod,
        const std::vector<uint8_t> &digestBlock, const std::vector<uint8_t> &signedData);

    int32_t ParsePkcs7SignedData(const uint8_t *sourceData, uint32_t sourceDataLen,
        std::vector<uint8_t> &digestBlock, std::vector<Pkcs7SignerInfo> &signerInfos);

private:
    int32_t UpdateDigestBlock(const std::vector<uint8_t> &digestBlock);

    int32_t UpdateSignerInfo(const std::vector<uint8_t> &signedData, uint32_t signMethod);

    ASN1_OBJECT *GetSignAlgorithmObj(uint32_t signMethod);

    int32_t PutDataToBuffer(Hpackage::PkgBuffer &p7Data);

private:
    int32_t VerifyInit(const uint8_t *sourceData, uint32_t sourceDataLen);

    int32_t DoParse(std::vector<uint8_t> &digestBlock, std::vector<Pkcs7SignerInfo> &signerInfos);

    int32_t ParseSignedData(std::vector<uint8_t> &digestBlock, std::vector<Pkcs7SignerInfo> &signerInfos);

    int32_t ParseContentInfo(std::vector<uint8_t> &digestBlock, const PKCS7_SIGNED *signData);

    int32_t ParseSignerInfos(std::vector<Pkcs7SignerInfo> &signerInfos);

    int32_t SignerInfoParse(PKCS7_SIGNER_INFO *p7SignerInfo, Pkcs7SignerInfo &signerInfo);

    int32_t ParseSignerInfoX509Algo(int32_t &algoNid, const X509_ALGOR *x509Algo);

private:
    PKCS7 *pkcs7_;
    BIO *p7Bio_;
};
} // namespace Hpackage
#endif
