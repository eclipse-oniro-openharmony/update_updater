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

#include "openssl_util.h"
#include <fstream>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

namespace Hpackage {
namespace {
constexpr uint32_t HASH_SOURCE_BLOCK_LEN = 4096;

void GetTextContentFromX509Name(X509_NAME *name, int nId, std::string &textContent)
{
    int contentLen = X509_NAME_get_text_by_NID(name, nId, nullptr, 0);
    if (contentLen <= 0) {
        return;
    }

    std::unique_ptr<char[]> textBuffer = std::make_unique<char[]>(contentLen + 1);
    if (textBuffer == nullptr) {
        return;
    }

    if (X509_NAME_get_text_by_NID(name, nId, textBuffer.get(), contentLen + 1) != contentLen) {
        return;
    }
    textContent = std::string(textBuffer.get());
    textBuffer.reset(nullptr);

    return;
}
}

int32_t GetASN1OctetStringData(const ASN1_OCTET_STRING *octString, std::vector<uint8_t> &asn1Data)
{
    if (octString == nullptr) {
        return -1;
    }
    const uint8_t *octChar = ASN1_STRING_get0_data(octString);
    if (octChar == nullptr) {
        PKG_LOGE("get asn1 obj string failed!");
        return -1;
    }

    int32_t octLen = ASN1_STRING_length(octString);
    if (octLen <= 0) {
        PKG_LOGE("invalid asn1 obj string len %d!", octLen);
        return -1;
    }
    asn1Data.assign(octChar, octChar + octLen);

    return 0;
}

int32_t GetX509AlgorithmNid(const X509_ALGOR *x509Algo, int32_t &algoNid)
{
    if (x509Algo == nullptr) {
        return -1;
    }

    const ASN1_OBJECT *algObj = nullptr;
    X509_ALGOR_get0(&algObj, nullptr, nullptr, x509Algo);
    if (algObj == nullptr) {
        PKG_LOGE("x509 algor get0 fail!");
        return -1;
    }
    algoNid = OBJ_obj2nid(algObj);
    if (algoNid <= 0) {
        PKG_LOGE("invalid algo nid %d!", algoNid);
        return -1;
    }

    return 0;
}

X509 *GetX509CertFromPemString(const std::string &pemString)
{
    BIO *pemBio = BIO_new(BIO_s_mem());
    if (pemBio == nullptr) {
        return nullptr;
    }
    int strLen = static_cast<int>(pemString.size());
    if (BIO_write(pemBio, pemString.c_str(), strLen) != strLen) {
        BIO_free_all(pemBio);
        return nullptr;
    }

    X509 *cert = PEM_read_bio_X509(pemBio, nullptr, nullptr, nullptr);
    if (cert == nullptr) {
        PKG_LOGE("pem read x509 fail");
    }
    BIO_free_all(pemBio);

    return cert;
}

X509 *GetX509CertFromPemFile(const std::string &filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        PKG_LOGE("file %s not exist", filePath.c_str());
        return nullptr;
    }
    BIO *certbio = BIO_new_file(filePath.c_str(), "r");
    if (certbio == nullptr) {
        PKG_LOGE("Failed to create BIO");
        return nullptr;
    }

    X509 *cert = PEM_read_bio_X509(certbio, nullptr, nullptr, nullptr);
    if (cert == nullptr) {
        PKG_LOGE("Failed to read x509 certificate");
        BIO_free_all(certbio);
        return nullptr;
    }
    BIO_free_all(certbio);

    return cert;
}

bool VerifyX509CertByIssuerCert(X509 *cert, X509 *issuerCert)
{
    if (cert == nullptr || issuerCert == nullptr) {
        return false;
    }

    EVP_PKEY *pubKey = X509_get_pubkey(issuerCert);
    if (pubKey == nullptr) {
        PKG_LOGE("get pubkey fial.");
        return false;
    }

    return (X509_verify(cert, pubKey) == 1);
}

int32_t VerifyDigestByPubKey(EVP_PKEY *pubKey, const int nid, const std::vector<uint8_t> &digestData,
    const std::vector<uint8_t> &signature)
{
    if (pubKey == nullptr) {
        PKG_LOGE("pubKey is empty");
        return -1;
    }

    EVP_MD_CTX *mdCtx = EVP_MD_CTX_create();
    if (mdCtx == nullptr) {
        PKG_LOGE("EVP_MD_CTX_create failed");
        return -1;
    }
    EVP_PKEY_CTX *pkeyCtx = nullptr;
    if (EVP_DigestVerifyInit(mdCtx, &pkeyCtx, EVP_get_digestbynid(nid), nullptr, pubKey) != 1) {
        PKG_LOGE("EVP init, error");
        EVP_MD_CTX_destroy(mdCtx);
        return -1;
    }
    if (EVP_DigestVerifyUpdate(mdCtx, digestData.data(), digestData.size()) != 1) {
        PKG_LOGE("EVP update, error");
        EVP_MD_CTX_destroy(mdCtx);
        return -1;
    }

    int ret = EVP_DigestVerifyFinal(mdCtx, signature.data(), signature.size());
    if (ret != 1) {
        PKG_LOGE("EVP final, error");
        EVP_MD_CTX_destroy(mdCtx);
        return -1;
    }

    EVP_MD_CTX_destroy(mdCtx);
    return 0;
}

int32_t CalcSha256Digest(const PkgStreamPtr srcData, const size_t dataLen, std::vector<uint8_t> &result)
{
    if (srcData == nullptr || dataLen == 0) {
        return -1;
    }
    if (result.size() != SHA256_DIGEST_LENGTH) {
        result.resize(SHA256_DIGEST_LENGTH);
    }
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    size_t offset = 0;
    size_t remainLen = dataLen;
    size_t blockLen = HASH_SOURCE_BLOCK_LEN;
    PkgBuffer buffer(blockLen);
    size_t readLen = 0;
    int32_t ret = 0;
    while (remainLen >= blockLen) {
        ret = srcData->Read(buffer, offset, blockLen, readLen);
        if (ret != 0) {
            PKG_LOGE("Fail read data");
            return -1;
        }
        SHA256_Update(&ctx, buffer.buffer, blockLen);
        offset += readLen;
        remainLen -= readLen;
    }
    if (remainLen > 0) {
        ret = srcData->Read(buffer, offset, remainLen, readLen);
        if (ret != 0) {
            PKG_LOGE("Fail read data");
            return -1;
        }
        SHA256_Update(&ctx, buffer.buffer, readLen);
    }

    if (SHA256_Final(result.data(), &ctx) != 1) {
        PKG_LOGE("SHA256_Final(), error");
        return -1;
    }

    return 0;
}

std::string GetStringFromX509Name(X509_NAME *x509Name)
{
    if (x509Name == nullptr) {
        return "";
    }

    std::string country;
    GetTextContentFromX509Name(x509Name, NID_countryName, country);

    std::string organization;
    GetTextContentFromX509Name(x509Name, NID_organizationName, organization);

    std::string organizationalUnitName;
    GetTextContentFromX509Name(x509Name, NID_organizationalUnitName, organizationalUnitName);

    std::string commonName;
    GetTextContentFromX509Name(x509Name, NID_commonName, commonName);

    return "C=" + country + ", O=" + organization + ", OU=" + organizationalUnitName + ", CN=" + commonName;
}

std::string GetX509CertSubjectName(X509 *cert)
{
    if (cert == nullptr) {
        return "";
    }

    X509_NAME *subjectName = X509_get_subject_name(cert);
    if (subjectName == nullptr) {
        PKG_LOGE("cert subject name is null");
        return "";
    }

    return GetStringFromX509Name(subjectName);
}

std::string GetX509CertIssuerName(X509 *cert)
{
    if (cert == nullptr) {
        return "";
    }

    X509_NAME *issuerName = X509_get_issuer_name(cert);
    if (issuerName == nullptr) {
        PKG_LOGE("cert issuer name is null");
        return "";
    }

    return GetStringFromX509Name(issuerName);
}
}
