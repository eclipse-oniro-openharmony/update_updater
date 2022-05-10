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

#include "pkcs7_signed_data.h"
#include <iostream>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/pkcs7.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include "pkg_utils.h"

namespace hpackage {
Pkcs7SignedData::Pkcs7SignedData()
{
    pkcs7_ = nullptr;
    p7Bio_ = nullptr;
}

Pkcs7SignedData::~Pkcs7SignedData()
{
    if (p7Bio_ != nullptr) {
        BIO_free(p7Bio_);
        p7Bio_ = nullptr;
    }
    if (pkcs7_ != nullptr) {
        PKCS7_free(pkcs7_);
        pkcs7_ = nullptr;
    }
}

int32_t Pkcs7SignedData::BuildPkcs7SignedData(PkgBuffer &p7Data, uint32_t signMethod,
    const std::vector<uint8_t> &digestBlock, const std::vector<uint8_t> &signedData)
{
    if (digestBlock.empty() || signedData.empty()) {
        return PKG_INVALID_PARAM;
    }
    pkcs7_ = PKCS7_new();
    if (pkcs7_ == nullptr) {
        PKG_LOGE("new PKCS7 data failed.");
        return PKG_INVALID_STREAM;
    }

    int32_t ret = PKCS7_set_type(pkcs7_, NID_pkcs7_signed);
    if (ret <= 0) {
        PKG_LOGE("set PKCS7 data type failed.");
        return ret;
    }
    PKCS7_set_detached(pkcs7_, 0);

    ret = UpdateDigestBlock(digestBlock);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("update digest block failed.");
        return ret;
    }
    ret = UpdateSignerInfo(signedData, signMethod);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("update signer info failed.");
        return ret;
    }

    return PutDataToBuffer(p7Data);
}

int32_t Pkcs7SignedData::UpdateDigestBlock(const std::vector<uint8_t> &digestBlock)
{
    int32_t ret = PKCS7_content_new(pkcs7_, NID_pkcs7_data);
    if (ret <= 0) {
        PKG_LOGE("new PKCS7 content failed.");
        return ret;
    }

    p7Bio_ = PKCS7_dataInit(pkcs7_, nullptr);
    if (p7Bio_ == nullptr) {
        PKG_LOGE("Pkcs7 data init failed.");
        return PKG_NONE_MEMORY;
    }
    ret = BIO_write(p7Bio_, digestBlock.data(), digestBlock.size());
    if (ret <= 0) {
        PKG_LOGE("bio write digest block failed.");
        return ret;
    }
    (void)BIO_flush(p7Bio_);
    ret = PKCS7_dataFinal(pkcs7_, p7Bio_);
    if (ret <= 0) {
        PKG_LOGE("Pkcs7 data final failed.");
        return ret;
    }

    return PKG_SUCCESS;
}

int32_t Pkcs7SignedData::UpdateSignerInfo(const std::vector<uint8_t> &signedData, uint32_t signMethod)
{
    PKCS7_SIGNER_INFO *p7Si = PKCS7_SIGNER_INFO_new();
    if (p7Si == nullptr) {
        PKG_LOGE("new pkcs7 signed info failed.");
        return PKG_INVALID_STREAM;
    }

    ASN1_OBJECT *digObj = OBJ_nid2obj(EVP_MD_type(EVP_sha256()));
    X509_ALGOR_set0(p7Si->digest_alg, digObj, V_ASN1_NULL, NULL);

    ASN1_OBJECT *signObj = GetSignAlgorithmObj(signMethod);
    if (signObj != nullptr) {
        X509_ALGOR_set0(p7Si->digest_enc_alg, signObj, V_ASN1_NULL, NULL);
    }

    int32_t ret = ASN1_STRING_set(p7Si->enc_digest, signedData.data(), signedData.size());
    if (ret <= 0) {
        PKG_LOGE("Set signer info enc_digest failed.");
        PKCS7_SIGNER_INFO_free(p7Si);
        return ret;
    }

    ret = sk_PKCS7_SIGNER_INFO_push(pkcs7_->d.sign->signer_info, p7Si);
    if (ret <= 0) {
        PKG_LOGE("Set signer info failed.");
        PKCS7_SIGNER_INFO_free(p7Si);
        return ret;
    }

    return PKG_SUCCESS;
}

ASN1_OBJECT *Pkcs7SignedData::GetSignAlgorithmObj(uint32_t signMethod)
{
    switch (signMethod) {
        case PKG_SIGN_METHOD_RSA:
            return OBJ_nid2obj(NID_sha256WithRSAEncryption);
        case PKG_SIGN_METHOD_ECDSA:
            return OBJ_nid2obj(NID_ecdsa_with_SHA256);
        default:
            PKG_LOGE("Invalid sign method.");
            return nullptr;
    }

    return nullptr;
}

int32_t Pkcs7SignedData::PutDataToBuffer(PkgBuffer &p7Data)
{
    int32_t p7DataLen = i2d_PKCS7(pkcs7_, NULL);
    PKG_CHECK(p7DataLen > 0, return PKG_INVALID_STREAM, "Invalid p7DataLen");
    uint8_t *outBuf = new(std::nothrow) uint8_t[p7DataLen]();
    PKG_CHECK(outBuf != nullptr, return PKG_INVALID_STREAM, "malloc mem failed.");

    BIO *p7OutBio = BIO_new(BIO_s_mem());
    PKG_CHECK(p7OutBio != nullptr, delete [] outBuf; return PKG_INVALID_STREAM, "BIO new failed.");

    int32_t ret = i2d_PKCS7_bio(p7OutBio, pkcs7_);
    PKG_CHECK(ret > 0, delete [] outBuf; BIO_free(p7OutBio); return ret, "i2d_PKCS7_bio failed.");

    (void)BIO_read(p7OutBio, outBuf, p7DataLen);
    p7Data.buffer = outBuf;
    p7Data.length = static_cast<size_t>(p7DataLen);

    BIO_free(p7OutBio);
    return PKG_SUCCESS;
}

int32_t Pkcs7SignedData::ParsePkcs7SignedData(const uint8_t *sourceData, uint32_t sourceDataLen,
    std::vector<uint8_t> &digestBlock, std::vector<Pkcs7SignerInfo> &signerInfos)
{
    int32_t ret = VerifyInit(sourceData, sourceDataLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Init pkcs7 data failed.!");
        return ret;
    }

    return DoParse(digestBlock, signerInfos);
}

int32_t Pkcs7SignedData::VerifyInit(const uint8_t *sourceData, uint32_t sourceDataLen)
{
    if (sourceData == nullptr || sourceDataLen == 0) {
        return PKG_INVALID_PARAM;
    }

    p7Bio_ = BIO_new(BIO_s_mem());
    if (p7Bio_ == nullptr) {
        PKG_LOGE("BIO_new error!");
        return PKG_INVALID_STREAM;
    }
    (void)BIO_write(p7Bio_, sourceData, sourceDataLen);

    pkcs7_ = d2i_PKCS7_bio(p7Bio_, nullptr);
    if (pkcs7_ == nullptr) {
        PKG_LOGE("d2i_PKCS7_bio failed!");
        return PKG_INVALID_STREAM;
    }

    return PKG_SUCCESS;
}

int32_t Pkcs7SignedData::DoParse(std::vector<uint8_t> &digestBlock, std::vector<Pkcs7SignerInfo> &signerInfos)
{
    int32_t p7Type = OBJ_obj2nid(pkcs7_->type);
    if (p7Type != NID_pkcs7_signed) {
        PKG_LOGE("Invalid pkcs7 data type %d!", p7Type);
        return PKG_INVALID_PKG_FORMAT;
    }

    PKCS7_SIGNED *signData = pkcs7_->d.sign;
    if (signData == nullptr) {
        PKG_LOGE("Invalid pkcs7 signed data!");
        return PKG_INVALID_PKG_FORMAT;
    }

    return ParseSignedData(digestBlock, signerInfos);
}

/*
 * tools.ietf.org/html/rfc2315#section-9.1
 * SignedData ::= SEQUENCE(0x30) {
 *     INTEGER(0x02)            version Version,
 *     SET(0x31)                digestAlgorithms DigestAlgorithmIdentifiers,
 *     SEQUENCE(0x30)           contentInfo ContentInfo,
 *     CONTET_SPECIFIC[0](0xA0) certificates [0] IMPLICIT ExtendedCertificatesAndCertificates OPTIONAL,
 *     CONTET_SPECIFIC[1](0xA1) crls [1] IMPLICIT CertificateRevocationLists OPTIONAL,
 *     SET(0x31)                signerInfos SignerInfos }
 */
int32_t Pkcs7SignedData::ParseSignedData(std::vector<uint8_t> &digestBlock,
    std::vector<Pkcs7SignerInfo> &signerInfos)
{
    int32_t ret = ParseContentInfo(digestBlock, pkcs7_->d.sign);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse pkcs7 content info failed!");
        return ret;
    }

    return ParseSignerInfos(signerInfos);
}

/*
 * tools.ietf.org/html/rfc2315#section-7
 * ContentInfo ::= SEQUENCE(0x30) {
 *     OBJECT_IDENTIFIER(0x06)  contentType ContentType,
 *     CONTET_SPECIFIC[0](0xA0) content [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL }
 *
 * tools.ietf.org/html/rfc2315#section-8
 *     Data ::= OCTET STRING
 */
int32_t Pkcs7SignedData::ParseContentInfo(std::vector<uint8_t> &digestBlock, const PKCS7_SIGNED *signData)
{
    if (signData == nullptr) {
        return PKG_INVALID_PARAM;
    }
    PKCS7 *contentInfo = signData->contents;
    if (contentInfo == nullptr) {
        PKG_LOGE("Invalid pkcs7 signed data!");
        return PKG_INVALID_PKG_FORMAT;
    }
    int32_t type = OBJ_obj2nid(contentInfo->type);
    if (type != NID_pkcs7_data) {
        PKG_LOGE("Invalid pkcs7 signed data %d!", type);
        return PKG_INVALID_PKG_FORMAT;
    }

    const uint8_t *digest = ASN1_STRING_get0_data(contentInfo->d.data);
    if (digest == nullptr) {
        PKG_LOGE("Get asn1 obj string failed!");
        return PKG_INVALID_PKG_FORMAT;
    }
    int32_t digestLen = ASN1_STRING_length(contentInfo->d.data);
    if (digestLen <= 0) {
        PKG_LOGE("Invalid asn1 obj string len %d!", digestLen);
        return PKG_INVALID_PKG_FORMAT;
    }

    digestBlock.resize(digestLen);
    int32_t ret = memcpy_s(digestBlock.data(), digestLen, digest, digestLen);
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s digestBlock, digestLen%d", digestLen);
        return PKG_NONE_MEMORY;
    }

    return PKG_SUCCESS;
}

/*
 * tools.ietf.org/html/rfc2315#section-9.2
 * SignerInfo ::= SEQUENCE(0x30) {
 *     INTEGER(0x02)             version Version,
 *     SEQUENCE(0x30)            issuerAndSerialNumber IssuerAndSerialNumber,
 *     SEQUENCE(0x30)            digestAlgorithm DigestAlgorithmIdentifier,
 *     CONTET_SPECIFIC[0](0xA0)  authenticatedAttributes [0] IMPLICIT Attributes OPTIONAL,
 *     SEQUENCE(0x30)            digestEncryptionAlgorithm DigestEncryptionAlgorithmIdentifier,
 *     OCTET_STRING(0x30)        encryptedDigest EncryptedDigest,
 *     CONTET_SPECIFIC[1](0xA1)  unauthenticatedAttributes [1] IMPLICIT Attributes OPTIONAL }
 */
int32_t Pkcs7SignedData::ParseSignerInfos(std::vector<Pkcs7SignerInfo> &signerInfos)
{
    if (PKCS7_get_signer_info(pkcs7_) == nullptr) {
        PKG_LOGE("Get pkcs7 signers info failed!");
        return PKG_INVALID_PKG_FORMAT;
    }

    int signerInfoNum = sk_PKCS7_SIGNER_INFO_num(PKCS7_get_signer_info(pkcs7_));
    if (signerInfoNum <= 0) {
        PKG_LOGE("Invalid signers info num %d!", signerInfoNum);
        return PKG_INVALID_PKG_FORMAT;
    }

    for (int i = 0; i < signerInfoNum; i++) {
        PKCS7_SIGNER_INFO *p7SiTmp = sk_PKCS7_SIGNER_INFO_value(PKCS7_get_signer_info(pkcs7_), i);
        Pkcs7SignerInfo signer;
        int32_t ret = SignerInfoParse(p7SiTmp, signer);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("SignerInfoParse failed!");
            continue;
        }
        signerInfos.push_back(std::move(signer));
    }

    return PKG_SUCCESS;
}

int32_t Pkcs7SignedData::SignerInfoParse(PKCS7_SIGNER_INFO *p7SignerInfo, Pkcs7SignerInfo &signerInfo)
{
    if (p7SignerInfo == nullptr) {
        return PKG_INVALID_PARAM;
    }
    int32_t ret = ParseSignerInfoX509Algo(signerInfo.digestNid, p7SignerInfo->digest_alg);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse signer info digest_alg failed!");
        return ret;
    }
    ret = ParseSignerInfoX509Algo(signerInfo.digestEncryptNid, p7SignerInfo->digest_enc_alg);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse signer info digest_enc_alg failed!");
        return ret;
    }

    const uint8_t *encDigest = ASN1_STRING_get0_data(p7SignerInfo->enc_digest);
    if (encDigest == nullptr) {
        PKG_LOGE("Get asn1 obj string failed!");
        return PKG_INVALID_PKG_FORMAT;
    }
    int32_t encDigestLen = ASN1_STRING_length(p7SignerInfo->enc_digest);
    if (encDigestLen <= 0) {
        PKG_LOGE("Invalid asn1 obj string len %d!", encDigestLen);
        return PKG_INVALID_PKG_FORMAT;
    }

    signerInfo.digestEncryptData.resize(encDigestLen);
    ret = memcpy_s(signerInfo.digestEncryptData.data(),
        signerInfo.digestEncryptData.size(), encDigest, encDigestLen);
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s digestEncryptData, encDigestLen %d", encDigestLen);
        return PKG_NONE_MEMORY;
    }

    return PKG_SUCCESS;
}

int32_t Pkcs7SignedData::ParseSignerInfoX509Algo(int32_t &algoNid, const X509_ALGOR *x509Algo)
{
    if (x509Algo == nullptr) {
        return PKG_INVALID_PARAM;
    }

    const ASN1_OBJECT *algObj = nullptr;
    X509_ALGOR_get0(&algObj, nullptr, nullptr, x509Algo);
    if (algObj == nullptr) {
        PKG_LOGE("Signer info ASN1_OBJECT null!");
        return PKG_INVALID_PKG_FORMAT;
    }
    algoNid = OBJ_obj2nid(algObj);
    if (algoNid <= 0) {
        PKG_LOGE("Invalid Signer info ASN1_OBJECT!");
        return PKG_INVALID_PKG_FORMAT;
    }

    return PKG_SUCCESS;
}
} // namespace hpackage
