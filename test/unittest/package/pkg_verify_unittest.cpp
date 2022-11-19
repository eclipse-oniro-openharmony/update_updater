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

#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <unistd.h>
#include "log.h"
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_test.h"
#include "pkg_utils.h"

#include "package.h"
#include "cert_verify.h"
#include "openssl_util.h"
#include "pkg_verify_util.h"
#include "zip_pkg_parse.h"
#include "pkcs7_signed_data.h"

using namespace std;
using namespace Hpackage;
using namespace Updater;
using namespace testing::ext;

namespace UpdaterUt {
class PackageVerifyTest : public PkgTest {
public:
    PackageVerifyTest() {}
    ~PackageVerifyTest() override {}
public:
    int TestGetFileSize(const std::string &testfileName)
    {
        int32_t ret = GetFileSize(testfileName);
        return ret;
    }

    int TestExtraPackageFile()
    {
        int32_t ret = ExtraPackageFile(nullptr, nullptr, nullptr, nullptr);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        std::string packagePath = "invalid_path";
        std::string keyPath = "invalid_key";
        std::string file = "invalid_file";
        std::string outPath = "invalid_path";
        ret = ExtraPackageFile(packagePath.c_str(), keyPath.c_str(), file.c_str(), outPath.c_str());
        EXPECT_EQ(ret, PKG_INVALID_FILE);

        packagePath = "/data/updater/package/test_package.zip";
        keyPath = "/data/updater/src/signing_cert.crt";
        file = "updater.bin";
        outPath = "/data/updater/package/";
        ret = ExtraPackageFile(packagePath.c_str(), keyPath.c_str(), file.c_str(), outPath.c_str());
        EXPECT_EQ(ret, PKG_SUCCESS);
        return 0;
    }

    int TestExtraPackageDir()
    {
        int32_t ret = ExtraPackageDir(nullptr, nullptr, nullptr, nullptr);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);

        std::string packagePath = "invalid_path";
        std::string keyPath = "invalid_key";
        std::string outPath = "invalid_path";
        ret = ExtraPackageDir(packagePath.c_str(), keyPath.c_str(), nullptr, outPath.c_str());
        EXPECT_EQ(ret, PKG_INVALID_FILE);

        packagePath = "/data/updater/package/test_package.zip";
        keyPath = "/data/updater/src/signing_cert.crt";
        outPath = "/data/updater/package/";
        ret = ExtraPackageDir(packagePath.c_str(), keyPath.c_str(), nullptr, outPath.c_str());
        EXPECT_EQ(ret, PKG_SUCCESS);
        return 0;
    }

    int TestCertVerifyFailed()
    {
        BIO *certbio = BIO_new_file(GetTestPrivateKeyName(0).c_str(), "r");
        X509 *rcert = PEM_read_bio_X509(certbio, nullptr, nullptr, nullptr);
        if (certbio != nullptr) {
            BIO_free(certbio);
        }
        SingleCertHelper singleCert;
        int32_t ret = singleCert.CertChainCheck(nullptr, nullptr);
        EXPECT_EQ(ret, -1);
        ret = singleCert.CertChainCheck(nullptr, rcert);
        EXPECT_EQ(ret, -1);

        ret = CertVerify::GetInstance().CheckCertChain(nullptr, nullptr);
        EXPECT_EQ(ret, -1);

        bool result = VerifyX509CertByIssuerCert(nullptr, nullptr);
        EXPECT_EQ(result, false);
        result = VerifyX509CertByIssuerCert(rcert, rcert);
        EXPECT_EQ(result, false);
        return 0;
    }

    int TestOpensslUtilFailed()
    {
        std::vector<uint8_t> asn1Data;
        int32_t ret = GetASN1OctetStringData(nullptr, asn1Data);
        EXPECT_EQ(ret, -1);

        int32_t algoNid {};
        ret = GetX509AlgorithmNid(nullptr, algoNid);
        EXPECT_EQ(ret, -1);

        X509 *result = GetX509CertFromPemString(" ");
        EXPECT_EQ(result, nullptr);
        result = GetX509CertFromPemFile("invalid_file");
        EXPECT_EQ(result, nullptr);

        BIO *certbio = BIO_new_file(GetTestPrivateKeyName(0).c_str(), "r");
        X509 *cert = PEM_read_bio_X509(certbio, nullptr, nullptr, nullptr);
        if (certbio != nullptr) {
            BIO_free(certbio);
        }
        bool boolResult = VerifyX509CertByIssuerCert(nullptr, nullptr);
        EXPECT_EQ(boolResult, false);
        boolResult = VerifyX509CertByIssuerCert(cert, cert);
        EXPECT_EQ(boolResult, false);

        ret = VerifyDigestByPubKey(nullptr, 0, asn1Data, asn1Data);
        EXPECT_EQ(ret, -1);
        ret = CalcSha256Digest(nullptr, 0, asn1Data);
        EXPECT_EQ(ret, -1);

        std::string stringResult = GetStringFromX509Name(nullptr);
        EXPECT_EQ(stringResult, "");
        stringResult = GetX509CertSubjectName(nullptr);
        EXPECT_EQ(stringResult, "");
        stringResult = GetX509CertSubjectName(cert);
        EXPECT_EQ(stringResult, "");
        stringResult = GetX509CertIssuerName(nullptr);
        EXPECT_EQ(stringResult, "");
        stringResult = GetX509CertIssuerName(cert);
        EXPECT_EQ(stringResult, "");
        return 0;
    }

    int TestPkcs7SignedDataFailed()
    {
        Pkcs7SignedData signedData;
        uint8_t testData['A'];
        uint8_t *srcData = testData;
        std::vector<uint8_t> hash;

        int32_t ret = signedData.Verify();
        EXPECT_EQ(ret, -1);
        ret = signedData.GetHashFromSignBlock(nullptr, 0, hash);
        EXPECT_EQ(ret, -1);
        ret = signedData.GetHashFromSignBlock(srcData, 0, hash);
        EXPECT_EQ(ret, -1);
        ret = signedData.GetHashFromSignBlock(srcData, 1, hash);
        EXPECT_EQ(ret, -1);
        return 0;
    }

    int TestPkgVerifyFailed()
    {
        PkgVerifyUtil pkgVerify;
        std::vector<uint8_t> data;
        size_t testData = 0;
        int32_t ret = pkgVerify.VerifyPackageSign(nullptr);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);
        ret = pkgVerify.GetSignature(nullptr, testData, data);
        EXPECT_NE(ret, PKG_SUCCESS);
        ret = pkgVerify.HashCheck(nullptr, testData, data);
        EXPECT_EQ(ret, PKG_INVALID_PARAM);
        ret = pkgVerify.ParsePackage(nullptr, testData, testData);
        EXPECT_NE(ret, PKG_SUCCESS);
        ret = pkgVerify.Pkcs7verify(data, data);
        EXPECT_NE(ret, PKG_SUCCESS);
        return 0;
    }
};

HWTEST_F(PackageVerifyTest, TestPkgVerifyFailed, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestPkgVerifyFailed());
}

HWTEST_F(PackageVerifyTest, TestPkcs7SignedDataFailed, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestPkcs7SignedDataFailed());
}

HWTEST_F(PackageVerifyTest, TestOpensslUtilFailed, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestOpensslUtilFailed());
}

HWTEST_F(PackageVerifyTest, TestCertVerifyFailed, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestCertVerifyFailed());
}

HWTEST_F(PackageVerifyTest, TestExtraPackageDir, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestExtraPackageDir());
}

HWTEST_F(PackageVerifyTest, TestExtraPackageFile, TestSize.Level1)
{
    PackageVerifyTest test;
    EXPECT_EQ(0, test.TestExtraPackageFile());
}

HWTEST_F(PackageVerifyTest, TestGetFileSize, TestSize.Level1)
{
    PackageVerifyTest test;
    std::string testFileName = "invalid_path";
    EXPECT_EQ(0, test.TestGetFileSize(testFileName));
    testFileName = "/data/updater/package/test_package.zip";
    EXPECT_EQ(1368949, test.TestGetFileSize(testFileName));
}
}
