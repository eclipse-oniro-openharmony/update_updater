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

#ifndef CERT_VERIFY_H
#define CERT_VERIFY_H

#include <memory>
#include <string>
#include <openssl/x509.h>
#include "macros.h"

namespace Hpackage {
struct CertInfo {
    X509 *rootCert = nullptr;
    std::string subject {};
    std::string issuer {};
};

class CertHelper {
public:
    virtual int32_t CertChainCheck(STACK_OF(X509) *certStack, X509 *cert) = 0;
    virtual ~CertHelper() {}
};

class CertVerify {
    DISALLOW_COPY_MOVE(CertVerify);
public:
    void RegisterCertHelper(std::unique_ptr<CertHelper> ptr);
    static CertVerify &GetInstance();
    int32_t CheckCertChain(STACK_OF(X509) *certStack, X509 *cert);

private:
    CertVerify() = default;
    ~CertVerify() = default;
    std::unique_ptr<CertHelper> helper_ {};
};

class SingleCertHelper : public CertHelper {
public:
    SingleCertHelper() = default;
    virtual ~SingleCertHelper();

    int32_t CertChainCheck(STACK_OF(X509) *certStack, X509 *cert) override;

private:
    int32_t InitRootCert();
    int32_t VerifySingleCert(X509 *cert);
    int32_t CompareCertSubjectAndIssuer(X509 *cert);

    CertInfo rootInfo_ {};
};
} // namespace Hpackage

#endif
