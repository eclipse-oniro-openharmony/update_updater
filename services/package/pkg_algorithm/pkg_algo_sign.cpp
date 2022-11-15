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
#include "pkg_algo_sign.h"
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include "openssl_util.h"
#include "pkg_algorithm.h"
#include "pkg_utils.h"

using namespace Updater;

namespace Hpackage {
#ifndef BIO_FP_READ
constexpr uint32_t BIO_FP_READ = 0x02;
#endif
int32_t SignAlgorithmRsa::SignBuffer(const PkgBuffer &buffer, std::vector<uint8_t> &sign, size_t &signLen) const
{
    if (buffer.buffer == nullptr) {
        PKG_LOGE("Param null!");
        return PKG_INVALID_PARAM;
    }
    BIO *in = BIO_new(BIO_s_file());
    if (in == nullptr) {
        PKG_LOGE("Failed to new BIO");
        return PKG_INVALID_PARAM;
    }

    int32_t ret = BIO_ctrl(in, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_READ, const_cast<char*>(keyName_.c_str()));
    if (ret != 1) {
        PKG_LOGE("Failed to BIO_read_filename ret %d %s", ret, keyName_.c_str());
        BIO_free(in);
        return PKG_INVALID_PARAM;
    }

    RSA *rsa = PEM_read_bio_RSAPrivateKey(in, nullptr, nullptr, nullptr);
    BIO_free(in);
    if (rsa == nullptr) {
        PKG_LOGE("Failed to PEM_read_bio_RSAPrivateKey ");
        return PKG_INVALID_SIGNATURE;
    }

    // Adjust key size
    uint32_t size = static_cast<uint32_t>(RSA_size(rsa));
    sign.resize(size);
    ret = 0;
    if (digestMethod_ == PKG_DIGEST_TYPE_SHA256) {
        ret = RSA_sign(NID_sha256, buffer.buffer, buffer.length, sign.data(), &size, rsa);
    } else if (digestMethod_ == PKG_DIGEST_TYPE_SHA384) {
        ret = RSA_sign(NID_sha384, buffer.buffer, buffer.length, sign.data(), &size, rsa);
    }
    signLen = size;
    RSA_free(rsa);
    return ((ret == 1) ? PKG_SUCCESS : PKG_INVALID_SIGNATURE);
}

int32_t SignAlgorithmEcc::SignBuffer(const PkgBuffer &buffer, std::vector<uint8_t> &sign, size_t &signLen) const
{
    if (buffer.buffer == nullptr) {
        PKG_LOGE("Param null!");
        return PKG_INVALID_PARAM;
    }
    BIO *in = BIO_new(BIO_s_file());
    if (in == nullptr) {
        PKG_LOGE("Failed to new BIO ");
        return PKG_INVALID_PARAM;
    }

    int ret = BIO_ctrl(in, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_READ, const_cast<char*>(keyName_.c_str()));
    if (ret != 1) {
        PKG_LOGE("Failed to BIO_read_filename ret %d %s", ret, keyName_.c_str());
        BIO_free(in);
        return PKG_INVALID_PARAM;
    }

    EC_KEY *ecKey = PEM_read_bio_ECPrivateKey(in, nullptr, nullptr, nullptr);
    BIO_free(in);
    if (ecKey == nullptr) {
        PKG_LOGE("Failed to PEM_read_bio_ECPrivateKey %s", keyName_.c_str());
        return PKG_INVALID_PARAM;
    }

    // Adjust key size
    uint32_t size = static_cast<uint32_t>(ECDSA_size(ecKey));
    sign.resize(size + sizeof(uint32_t));
    ret = ECDSA_sign(0, buffer.buffer, buffer.length, sign.data() + sizeof(uint32_t), &size, ecKey);
    WriteLE32(sign.data(), size);
    signLen = size + sizeof(uint32_t);
    EC_KEY_free(ecKey);
    return ((ret == 1) ? PKG_SUCCESS : PKG_INVALID_SIGNATURE);
}

X509 *SignAlgorithm::GetPubkey() const
{
    BIO *certbio = BIO_new_file(keyName_.c_str(), "r");
    if (certbio == nullptr) {
        PKG_LOGE("Failed to create BIO");
        return nullptr;
    }
    X509 *rcert = PEM_read_bio_X509(certbio, nullptr, nullptr, nullptr);
    if (rcert == nullptr) {
        PKG_LOGE("Failed to read x509 certificate");
        BIO_free(certbio);
        return nullptr;
    }
    BIO_free(certbio);
    return rcert;
}

int32_t SignAlgorithm::VerifyDigest(const std::vector<uint8_t> &digest, const std::vector<uint8_t> &signature)
{
    X509 *rcert = GetPubkey();
    if (rcert == nullptr) {
        PKG_LOGE("get pubkey fail");
        return PKG_INVALID_SIGNATURE;
    }
    EVP_PKEY *pubKey = X509_get_pubkey(rcert);
    if (pubKey == nullptr) {
        X509_free(rcert);
        PKG_LOGE("get pubkey from cert fail");
        return PKG_INVALID_SIGNATURE;
    }
    int nid = EVP_MD_type(EVP_sha256());
    int ret = VerifyDigestByPubKey(pubKey, nid, digest, signature);
    if (ret != 0) {
        X509_free(rcert);
        PKG_LOGE("Failed to verify digest by pubKey");
        return PKG_INVALID_SIGNATURE;
    }
    X509_free(rcert);
    return 0;
}

SignAlgorithm::SignAlgorithmPtr PkgAlgorithmFactory::GetSignAlgorithm(const std::string &path,
    uint8_t signMethod, uint8_t type)
{
    switch (signMethod) {
        case PKG_SIGN_METHOD_RSA:
            return std::make_shared<SignAlgorithmRsa>(path, type);
        case PKG_SIGN_METHOD_ECDSA:
            return std::make_shared<SignAlgorithmEcc>(path, type);
        default:
            break;
    }
    return nullptr;
}

SignAlgorithm::SignAlgorithmPtr PkgAlgorithmFactory::GetVerifyAlgorithm(const std::string &path, uint8_t type)
{
    return std::make_shared<VerifyAlgorithm>(path, type);
}
} // namespace Hpackage
