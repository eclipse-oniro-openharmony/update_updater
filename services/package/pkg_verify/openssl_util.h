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

#ifndef OPENSSL_UTIL_H
#define OPENSSL_UTIL_H

#include <vector>
#include <openssl/asn1.h>
#include "pkg_manager.h"
#include "pkg_stream.h"

namespace Hpackage {
int32_t GetASN1OctetStringData(const ASN1_OCTET_STRING *octString, std::vector<uint8_t> &asn1Data);
int32_t GetX509AlgorithmNid(const X509_ALGOR *x509Algo, int32_t &algoNid);
X509 *GetX509CertFromPemString(const std::string &pemString);
X509 *GetX509CertFromPemFile(const std::string &filePath);
std::string GetStringFromX509Name(X509_NAME *x509Name);
std::string GetX509CertSubjectName(X509 *cert);
std::string GetX509CertIssuerName(X509 *cert);
bool VerifyX509CertByIssuerCert(X509 *cert, X509 *issuerCert);
int32_t VerifyDigestByPubKey(EVP_PKEY *pubKey, const int nid, const std::vector<uint8_t> &digestData,
    const std::vector<uint8_t> &signature);
int32_t CalcSha256Digest(const Hpackage::PkgStreamPtr srcData, const size_t dataLen, std::vector<uint8_t> &result);
}

#endif
