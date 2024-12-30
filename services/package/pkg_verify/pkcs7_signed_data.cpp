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
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/pkcs7.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include "cert_verify.h"
#include "dump.h"
#include "openssl_util.h"
#include "pkg_utils.h"

namespace Hpackage {
namespace {
constexpr size_t g_digestAlgoLength[][2] = {
    {NID_sha256, SHA256_DIGEST_LENGTH},
};

size_t GetDigestLength(const size_t digestNid)
{
    for (size_t i = 0; i < sizeof(g_digestAlgoLength) / sizeof(g_digestAlgoLength[0]); i++) {
        if (digestNid == g_digestAlgoLength[i][0]) {
            return g_digestAlgoLength[i][1];
        }
    }
    return 0;
}
}

Pkcs7SignedData::~Pkcs7SignedData()
{
    if (pkcs7_ != nullptr) {
        PKCS7_free(pkcs7_);
        pkcs7_ = nullptr;
    }
}

int32_t Pkcs7SignedData::GetHashFromSignBlock(const uint8_t *srcData, const size_t dataLen,
    std::vector<uint8_t> &hash)
{
    Updater::UPDATER_INIT_RECORD;
    int32_t ret = ParsePkcs7Data(srcData, dataLen);
    if (ret != 0) {
        PKG_LOGE("parse pkcs7 data fail");
        UPDATER_LAST_WORD(ret, "parse pkcs7 data fail");
        return ret;
    }

    ret = Verify();
    if (ret != 0) {
        PKG_LOGE("verify pkcs7 data fail");
        UPDATER_LAST_WORD(ret, "verify pkcs7 data fail");
        return ret;
    }
    hash.assign(digest_.begin(), digest_.end());

    return 0;
}

int32_t Pkcs7SignedData::ParsePkcs7Data(const uint8_t *srcData, const size_t dataLen)
{
    Updater::UPDATER_INIT_RECORD;
    if (srcData == nullptr || dataLen == 0) {
        UPDATER_LAST_WORD(-1, "srcData or dataLen is invalid");
        return -1;
    }
    if (Init(srcData, dataLen) != 0) {
        PKG_LOGE("init pkcs7 data fail");
        UPDATER_LAST_WORD(-1, "init pkcs7 data fail");
        return -1;
    }

    return DoParse();
}

int32_t Pkcs7SignedData::Verify() const
{
    std::vector<uint8_t> digestForEVP;
    for (unsigned int i = 0; i < signatureInfo.overall.length; i++) {
        digestForEVP.push_back(static_cast<uint8_t>(signatureInfo.overall.buffer[i]));
    }
    if (Verify(digestForEVP, {}, true) == 0) {
        return 0;
    }
    return Verify(digest_, {}, true);
}

int32_t Pkcs7SignedData::Verify(const std::vector<uint8_t> &hash, const std::vector<uint8_t> &sig,
    bool sigInSignerInfo) const
{
    if (hash.empty()) {
        return -1;
    }
    int32_t ret = -1;
    for (auto &signerInfo : signerInfos_) {
        ret = Pkcs7SignleSignerVerify(signerInfo, hash, sigInSignerInfo ? signerInfo.digestEncryptData : sig);
        if (ret == 0) {
            PKG_LOGI("p7sourceData check success");
            break;
        }
        PKG_LOGI("p7sourceData continue");
    }

    return ret;
}

int32_t Pkcs7SignedData::Init(const uint8_t *sourceData, const uint32_t sourceDataLen)
{
    Updater::UPDATER_INIT_RECORD;
    BIO *p7Bio = BIO_new(BIO_s_mem());
    if (p7Bio == nullptr) {
        PKG_LOGE("BIO_new error!");
        UPDATER_LAST_WORD(-1, "BIO_new error!");
        return -1;
    }
    if (static_cast<uint32_t>(BIO_write(p7Bio, sourceData, sourceDataLen)) != sourceDataLen) {
        PKG_LOGE("BIO_write error!");
        UPDATER_LAST_WORD(-1, "BIO_write error!");
        BIO_free(p7Bio);
        return -1;
    }

    if (pkcs7_ != nullptr) {
        PKCS7_free(pkcs7_);
        pkcs7_ = nullptr;
    }
    pkcs7_ = d2i_PKCS7_bio(p7Bio, nullptr);
    if (pkcs7_ == nullptr) {
        PKG_LOGE("d2i_PKCS7_bio failed!");
        BIO_free(p7Bio);
        UPDATER_LAST_WORD(-1, "d2i_PKCS7_bio failed!");
        return -1;
    }

    int32_t type = OBJ_obj2nid(pkcs7_->type);
    if (type != NID_pkcs7_signed) {
        PKG_LOGE("Invalid pkcs7 data type %d", type);
        BIO_free(p7Bio);
        UPDATER_LAST_WORD(type, "Invalid pkcs7 data type");
        return -1;
    }

    BIO_free(p7Bio);
    if (CertVerify::GetInstance().Init() != 0) {
        PKG_LOGE("init cert verify fail");
        UPDATER_LAST_WORD(-1,"init cert verify fail");
        return -1;
    }
    return 0;
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
int32_t Pkcs7SignedData::DoParse()
{
    Updater::UPDATER_INIT_RECORD;
    std::vector<uint8_t> contentInfo;
    int32_t ret = ParseContentInfo(contentInfo);
    if (ret != 0) {
        PKG_LOGE("parse pkcs7 contentInfo fail");
        UPDATER_LAST_WORD(-1, "parse pkcs7 contentInfo fail");
        return -1;
    }

    if (GetInstance().GetDigest(contentInfo, signatureInfo, digest_) != 0) {
        ret = GetDigestFromContentInfo(contentInfo);
        if (ret != 0) {
            PKG_LOGE("invalid pkcs7 contentInfo fail");
            UPDATER_LAST_WORD(-1, "invalid pkcs7 contentInfo fail");
            return -1;
        }
    }

    return SignerInfosParse();
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
int32_t Pkcs7SignedData::ParseContentInfo(std::vector<uint8_t> &digestBlock) const
{
    Updater::UPDATER_INIT_RECORD;
    PKCS7_SIGNED *signData = pkcs7_->d.sign;
    if (signData == nullptr) {
        PKG_LOGE("invalid pkcs7 signed data!");
        UPDATER_LAST_WORD(-1, "invalid pkcs7 signed data!");
        return -1;
    }

    PKCS7 *contentInfo = signData->contents;
    if (contentInfo == nullptr) {
        PKG_LOGE("pkcs7 content is nullptr!");
        UPDATER_LAST_WORD(-1, "pkcs7 content is nullptr!");
        return -1;
    }
    if (OBJ_obj2nid(contentInfo->type) != NID_pkcs7_data) {
        PKG_LOGE("invalid pkcs7 signed data type");
        UPDATER_LAST_WORD(-1, "invalid pkcs7 signed data type");
        return -1;
    }

    if (GetASN1OctetStringData(contentInfo->d.data, digestBlock) != 0) {
        PKG_LOGE("get pkcs7 contentInfo fail");
        UPDATER_LAST_WORD(-1, "get pkcs7 contentInfo fail");
        return -1;
    }

    return 0;
}

int32_t Pkcs7SignedData::GetDigestFromContentInfo(std::vector<uint8_t> &digestBlock)
{
    Updater::UPDATER_INIT_RECORD;
    if (digestBlock.size() <= sizeof(uint32_t)) {
        PKG_LOGE("invalid digest block info.");
        UPDATER_LAST_WORD(-1, "invalid digest block info.");
        return -1;
    }

    size_t offset = 0;
    size_t algoId = static_cast<size_t>(ReadLE16(digestBlock.data() + offset));
    offset += static_cast<size_t>(sizeof(uint16_t));
    size_t digestLen = static_cast<size_t>(ReadLE16(digestBlock.data() + offset));
    offset += static_cast<size_t>(sizeof(uint16_t));
    if ((GetDigestLength(algoId) != digestLen) || ((digestLen + offset) != digestBlock.size())) {
        PKG_LOGE("invalid digestLen[%zu] and digestBlock len[%zu]", digestLen, digestBlock.size());
        UPDATER_LAST_WORD(-1, "invalid digestLen[%zu] and digestBlock len[%zu]", digestLen, digestBlock.size());
        return -1;
    }
    digest_.assign(digestBlock.begin() + offset, digestBlock.end());
    return 0;
}

Pkcs7SignedData &Pkcs7SignedData::GetInstance()
{
    static Pkcs7SignedData checkPackagesInfo;
    return checkPackagesInfo;
}

extern "C" __attribute__((constructor)) void RegisterVerifyHelper(void)
{
    Pkcs7SignedData::GetInstance().RegisterVerifyHelper(std::make_unique<Pkcs7VerifyHelper>());
}

void Pkcs7SignedData::RegisterVerifyHelper(std::unique_ptr<VerifyHelper> ptr)
{
    helper_ = std::move(ptr);
}

Pkcs7VerifyHelper::~Pkcs7VerifyHelper()
{
    return;
}

int32_t Pkcs7VerifyHelper::GetDigestFromSubBlocks(std::vector<uint8_t> &digestBlock,
    HwSigningSigntureInfo &signatureInfo, std::vector<uint8_t> &digest)
{
    PKG_LOGE("Pkcs7VerifyHelper in");
    return -1;
}

int32_t Pkcs7SignedData::GetDigest(std::vector<uint8_t> &digestBlock,
    HwSigningSigntureInfo &signatureInfo, std::vector<uint8_t> &digest)
{
    if (helper_ == nullptr) {
        PKG_LOGE("helper_ null error");
        return -1;
    }
    return helper_->GetDigestFromSubBlocks(digestBlock, signatureInfo, digest);
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
int32_t Pkcs7SignedData::ReadSig(const uint8_t *sourceData, const uint32_t sourceDataLen,
    std::vector<std::vector<uint8_t>> &sigs)
{
    Updater::UPDATER_INIT_RECORD;
    if (sourceData == nullptr || sourceDataLen == 0) {
        UPDATER_LAST_WORD(PKCS7_INVALID_PARAM_ERR, "sourceData is nullptr or sourceDataLen is 0");
        return PKCS7_INVALID_PARAM_ERR;
    }
    if (Init(sourceData, sourceDataLen) != 0) {
        PKG_LOGE("init pkcs7 data fail");
        UPDATER_LAST_WORD(PKCS7_INIT_ERR, "init pkcs7 data fail");
        return PKCS7_INIT_ERR;
    }
    STACK_OF(PKCS7_SIGNER_INFO) *p7SignerInfos = PKCS7_get_signer_info(pkcs7_);
    if (p7SignerInfos == nullptr) {
        PKG_LOGE("get pkcs7 signers failed!");
        UPDATER_LAST_WORD(PKCS7_INVALID_VALUE_ERR, "get pkcs7 signers failed!");
        return PKCS7_INVALID_VALUE_ERR;
    }
    int signerInfoNum = sk_PKCS7_SIGNER_INFO_num(p7SignerInfos);
    if (signerInfoNum <= 0) {
        PKG_LOGE("invalid signers info num %d!", signerInfoNum);
        UPDATER_LAST_WORD(PKCS7_INVALID_VALUE_ERR, "invalid signers info num %d!", signerInfoNum);
        return PKCS7_INVALID_VALUE_ERR;
    }
    for (int i = 0; i < signerInfoNum; i++) {
        PKCS7_SIGNER_INFO *p7SiTmp = sk_PKCS7_SIGNER_INFO_value(p7SignerInfos, i);
        Pkcs7SignerInfo signer;
        int32_t ret = SignerInfoParse(p7SiTmp, signer);
        if (ret != 0) {
            PKG_LOGE("SignerInfo Parse failed!");
            continue;
        }
        sigs.push_back(signer.digestEncryptData);
    }
    if (sigs.size() == 0) {
        PKG_LOGE("no valid sigs!");
        UPDATER_LAST_WORD(PKCS7_HAS_NO_VALID_SIG_ERR, "no valid sigs!");
        return PKCS7_HAS_NO_VALID_SIG_ERR;
    }
    return PKCS7_SUCCESS;
}

int32_t Pkcs7SignedData::SignerInfosParse()
{
    Updater::UPDATER_INIT_RECORD;
    STACK_OF(PKCS7_SIGNER_INFO) *p7SignerInfos = PKCS7_get_signer_info(pkcs7_);
    if (p7SignerInfos == nullptr) {
        PKG_LOGE("get pkcs7 signers info failed!");
        UPDATER_LAST_WORD(-1, "get pkcs7 signers info failed!");
        return -1;
    }

    int signerInfoNum = sk_PKCS7_SIGNER_INFO_num(p7SignerInfos);
    if (signerInfoNum <= 0) {
        PKG_LOGE("invalid signers info num %d!", signerInfoNum);
        UPDATER_LAST_WORD(-1, "invalid signers info num %d!", signerInfoNum);
        return -1;
    }

    for (int i = 0; i < signerInfoNum; i++) {
        PKCS7_SIGNER_INFO *p7SiTmp = sk_PKCS7_SIGNER_INFO_value(p7SignerInfos, i);
        Pkcs7SignerInfo signer;
        int32_t ret = SignerInfoParse(p7SiTmp, signer);
        if (ret != 0) {
            PKG_LOGE("SignerInfoParse failed!");
            continue;
        }
        signerInfos_.push_back(std::move(signer));
    }

    return 0;
}

int32_t Pkcs7SignedData::SignerInfoParse(PKCS7_SIGNER_INFO *p7SignerInfo, Pkcs7SignerInfo &signerInfo)
{
    Updater::UPDATER_INIT_RECORD;
    if (p7SignerInfo == nullptr) {
        return -1;
    }
    PKCS7_ISSUER_AND_SERIAL *p7IssuerAndSerial = p7SignerInfo->issuer_and_serial;
    if (p7IssuerAndSerial == nullptr) {
        PKG_LOGE("signer cert info is nullptr!");
        UPDATER_LAST_WORD(-1, "signer cert info is nullptr!");
        return -1;
    }
    signerInfo.issuerName = p7IssuerAndSerial->issuer;
    signerInfo.serialNumber = p7IssuerAndSerial->serial;

    int32_t ret = GetX509AlgorithmNid(p7SignerInfo->digest_alg, signerInfo.digestNid);
    if (ret != 0) {
        PKG_LOGE("Parse signer info digest_alg failed!");
        return ret;
    }
    ret = GetX509AlgorithmNid(p7SignerInfo->digest_enc_alg, signerInfo.digestEncryptNid);
    if (ret != 0) {
        PKG_LOGE("Parse signer info digest_enc_alg failed!");
        return ret;
    }

    ret = GetASN1OctetStringData(p7SignerInfo->enc_digest, signerInfo.digestEncryptData);
    if (ret != 0) {
        PKG_LOGE("parse signer info enc_digest failed!");
        return ret;
    }

    return 0;
}

int32_t Pkcs7SignedData::Pkcs7SignleSignerVerify(const Pkcs7SignerInfo &signerInfo, const std::vector<uint8_t> &hash,
    const std::vector<uint8_t> &sig) const
{
    Updater::UPDATER_INIT_RECORD;
    if (pkcs7_ == nullptr) {
        UPDATER_LAST_WORD(-1, "pkcs7_ is nullptr");
        return -1;
    }
    STACK_OF(X509) *certStack = pkcs7_->d.sign->cert;
    if (certStack == nullptr) {
        PKG_LOGE("certStack is empty!");
        UPDATER_LAST_WORD(-1, "certStack is empty!");
        return -1;
    }

    X509 *cert = X509_find_by_issuer_and_serial(certStack, signerInfo.issuerName, signerInfo.serialNumber);
    if (cert == nullptr) {
        PKG_LOGE("cert is empty");
        UPDATER_LAST_WORD(-1, "cert is empty");
        return -1;
    }

    if (CertVerify::GetInstance().CheckCertChain(certStack, cert) != 0) {
        PKG_LOGE("public cert check fail");
        UPDATER_LAST_WORD(-1, "public cert check fail");
        return -1;
    }

    return VerifyDigest(cert, signerInfo, hash, sig);
}

int32_t Pkcs7SignedData::VerifyDigest(X509 *cert, const Pkcs7SignerInfo &signer, const std::vector<uint8_t> &hash,
    const std::vector<uint8_t> &sig) const
{
    Updater::UPDATER_INIT_RECORD;
    if (cert == nullptr) {
        UPDATER_LAST_WORD(-1, "cert is null");
        return -1;
    }

    EVP_PKEY *pubKey = X509_get_pubkey(cert);
    if (pubKey == nullptr) {
        PKG_LOGE("get pubkey from cert fail");
        UPDATER_LAST_WORD(-1, "get pubkey from cert fail");
        return -1;
    }

    auto ret = VerifyDigestByPubKey(pubKey, signer.digestNid, hash, sig);
    EVP_PKEY_free(pubKey);
    return ret;
}
} // namespace Hpackage
