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

#include "cert_verify.h"

#include <sys/stat.h>
#include "dump.h"
#include "openssl_util.h"
#include "pkg_utils.h"
#include "utils.h"

using namespace std;
using namespace Updater;
namespace Hpackage {
extern "C" __attribute__((constructor)) void RegisterCertHelper(void)
{
    CertVerify::GetInstance().RegisterCertHelper(std::make_unique<SingleCertHelper>());
}

void CertVerify::RegisterCertHelper(std::unique_ptr<CertHelper> ptr)
{
    helper_ = std::move(ptr);
}

CertVerify &CertVerify::GetInstance()
{
    static CertVerify certVerify;
    return certVerify;
}

int32_t CertVerify::CheckCertChain(STACK_OF(X509) *certStack, X509 *cert)
{
    if (helper_ == nullptr) {
        PKG_LOGE("helper_ null error");
        return -1;
    }
    return helper_->CertChainCheck(certStack, cert);
}

SingleCertHelper::~SingleCertHelper()
{
    if (rootInfo_.rootCert != nullptr) {
        X509_free(rootInfo_.rootCert);
    }
}

int32_t SingleCertHelper::CertChainCheck(STACK_OF(X509) *certStack, X509 *cert)
{
    UNUSED(certStack);
    if (cert == nullptr) {
        return -1;
    }

    int32_t ret = InitRootCert();
    if (ret != 0) {
        PKG_LOGE("Init root cert fail");
        return -1;
    }

    return VerifySingleCert(cert);
}

int32_t SingleCertHelper::InitRootCert()
{
#ifndef DIFF_PATCH_SDK
    X509 *rootCert = GetX509CertFromPemFile(Utils::GetCertName());
    if (rootCert == nullptr) {
        PKG_LOGE("Get root cert fail, file: %s", Utils::GetCertName().c_str());
        UPDATER_LAST_WORD(-1);
        return -1;
    }
    rootInfo_.rootCert = rootCert;
    rootInfo_.subject = GetX509CertSubjectName(rootCert);
    rootInfo_.issuer = GetX509CertIssuerName(rootCert);
#endif

    return 0;
}

int32_t SingleCertHelper::VerifySingleCert(X509 *cert)
{
    int32_t ret = CompareCertSubjectAndIssuer(cert);
    if (ret != 0) {
        PKG_LOGE("compare cert subject and issuer fail");
        return -1;
    }

    return ((VerifyX509CertByIssuerCert(cert, rootInfo_.rootCert)) ? 0 : -1);
}

int32_t SingleCertHelper::CompareCertSubjectAndIssuer(X509 *cert)
{
    string certSubject = GetX509CertSubjectName(cert);
    string certIssuer = GetX509CertIssuerName(cert);
    if (rootInfo_.subject.compare(certSubject) == 0 &&
        rootInfo_.issuer.compare(certIssuer) == 0) {
        return 0;
    }

    return -1;
}
} // namespace Hpackage
