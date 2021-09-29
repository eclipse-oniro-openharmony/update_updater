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

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include "log.h"
#include "pkg_algo_deflate.h"
#include "pkg_algo_lz4.h"
#include "pkg_algorithm.h"
#include "pkg_algo_sign.h"
#include "pkg_manager.h"
#include "pkg_test.h"

using namespace std;
using namespace hpackage;
using namespace updater;
using namespace testing::ext;

namespace {
constexpr size_t BUFFER_LEN = 10;
constexpr size_t DIGEST_LEN = 32;
class PkgAlgoUnitTest : public PkgTest {
public:
    PkgAlgoUnitTest() {}
    ~PkgAlgoUnitTest() override {}

    int TestCrcDigest() const
    {
        std::unique_ptr<Crc32Algorithm> algo = std::make_unique<Crc32Algorithm>();
        EXPECT_NE(algo, nullptr);
        int ret = algo->Init();
        EXPECT_EQ(0, ret);
        uint8_t buff[BUFFER_LEN] = {1};
        PkgBuffer crcBuffer(buff, sizeof(buff));
        ret = algo->Update(crcBuffer, sizeof(buff));
        EXPECT_EQ(0, ret);
        uint32_t crc = 0;
        PkgBuffer crcResult(reinterpret_cast<uint8_t *>(&crc), sizeof(crc));
        ret = algo->Final(crcResult);
        EXPECT_EQ(0, ret);

        uint32_t crc2 = 0;
        crcResult = {reinterpret_cast<uint8_t *>(&crc2), sizeof(crc)};
        ret = algo->Calculate(crcResult, crcBuffer, sizeof(buff));
        EXPECT_EQ(0, ret);
        EXPECT_EQ(crc, crc2);
        return ret;
    }

    int TestHash256Digest() const
    {
        std::unique_ptr<Sha256Algorithm> algo = std::make_unique<Sha256Algorithm>();
        EXPECT_NE(algo, nullptr);
        int ret = algo->Init();
        EXPECT_EQ(0, ret);
        uint8_t buff[BUFFER_LEN] = {1};
        PkgBuffer buffer(buff, sizeof(buff));
        ret = algo->Update(buffer, sizeof(buff));
        EXPECT_EQ(0, ret);
        size_t bufferSize = 32;
        PkgBuffer dig(bufferSize);
        ret = algo->Final(dig);
        EXPECT_EQ(0, ret);
        ret = algo->Calculate(dig, buffer, sizeof(buff));
        EXPECT_EQ(0, ret);
        return ret;
    }

    int TestHash384Digest() const
    {
        std::unique_ptr<Sha384Algorithm> algo = std::make_unique<Sha384Algorithm>();
        EXPECT_NE(algo, nullptr);
        int ret = algo->Init();
        EXPECT_EQ(0, ret);
        uint8_t buff[BUFFER_LEN] = {1};
        PkgBuffer buffer384(buff, sizeof(buff));
        ret = algo->Update(buffer384, sizeof(buff));
        EXPECT_EQ(0, ret);
        size_t bufferSize = 64;
        PkgBuffer dig(bufferSize);
        ret = algo->Final(dig);
        EXPECT_EQ(0, ret);
        ret = algo->Calculate(dig, buffer384, sizeof(buff));
        EXPECT_EQ(0, ret);
        return ret;
    }

    int TestInvalidParam() const
    {
        constexpr int8_t invalidType = 100;
        constexpr size_t digestLen = 32;
        constexpr int16_t magicNumber = 256;
        int ret = DigestAlgorithm::GetDigestLen(invalidType);
        EXPECT_EQ(0, ret);
        ret = DigestAlgorithm::GetSignatureLen(invalidType);
        EXPECT_EQ(magicNumber, ret);

        DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(invalidType);
        EXPECT_NE(nullptr, algorithm);
        algorithm->Init();
        uint8_t dig2[digestLen];
        PkgBuffer buffer(dig2, sizeof(dig2));
        algorithm->Update(buffer, sizeof(dig2));
        algorithm->Final(buffer);

        SignAlgorithm::SignAlgorithmPtr sign = PkgAlgorithmFactory::GetSignAlgorithm(TEST_PATH_FROM, invalidType, 0);
        EXPECT_EQ(nullptr, sign);

        PkgAlgorithm::PkgAlgorithmPtr algo = PkgAlgorithmFactory::GetAlgorithm(nullptr);
        EXPECT_EQ(nullptr, algo);
        FileInfo config;
        config.packMethod = invalidType;
        algo = PkgAlgorithmFactory::GetAlgorithm(nullptr);
        EXPECT_EQ(nullptr, algo);
        EXPECT_EQ(nullptr, sign);

        return 0;
    }

    int TestSignVerify(int8_t signMethod, std::string privateKey, std::string certName)
    {
        PkgBuffer digest(DIGEST_LEN);
        std::string filePath = TEST_PATH_FROM;
        filePath += "loadScript.us";
        int ret = BuildFileDigest(*digest.buffer, digest.length, filePath);
        EXPECT_EQ(ret, PKG_SUCCESS);
        // 签名
        filePath = TEST_PATH_FROM + privateKey;
        SignAlgorithm::SignAlgorithmPtr algorithm = PkgAlgorithmFactory::GetSignAlgorithm(filePath,
            signMethod, PKG_DIGEST_TYPE_SHA256);
        EXPECT_NE(nullptr, algorithm);
        std::vector<uint8_t> signature;
        size_t signLen = 0;
        ret = algorithm->SignBuffer(digest, signature, signLen);
        EXPECT_EQ(ret, PKG_SUCCESS);

        ret = algorithm->VerifyBuffer(digest.data, signature);
        EXPECT_EQ(ret, PKG_INVALID_SIGNATURE);

        filePath = TEST_PATH_FROM + certName;
        algorithm = PkgAlgorithmFactory::GetVerifyAlgorithm(filePath, PKG_DIGEST_TYPE_SHA256);
        // 验证
        ret = algorithm->SignBuffer(digest, signature, signLen);
        EXPECT_EQ(ret, PKG_INVALID_SIGNATURE);

        ret = algorithm->VerifyBuffer(digest.data, signature);
        EXPECT_EQ(ret, PKG_SUCCESS);
        return 0;
    }

    int TestEccUserPackage(int8_t signMethod, std::string privateKey, std::string certName)
    {
        PKG_LOGI("\n\n ************* TestPackageCreate %s \r\n", testPackageName.c_str());
        uint32_t updateFileVersion = 1000;
        UpgradePkgInfoExt pkgInfo;
        pkgInfo.entryCount = testFileNames_.size();
        pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
        pkgInfo.signMethod = signMethod;
        pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;
        pkgInfo.updateFileVersion = updateFileVersion;
        std::string filePath;
        uint8_t componentFlags = 22;
        ComponentInfoExt comp[testFileNames_.size()];
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            comp[i].componentAddr = strdup(testFileNames_[i].c_str());
            filePath = TEST_PATH_FROM;
            filePath += testFileNames_[i].c_str();
            comp[i].filePath = strdup(filePath.c_str());
            comp[i].flags = componentFlags;
            filePath.clear();
        }
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        std::string keyName = TEST_PATH_FROM;
        keyName += privateKey;
        int32_t ret = CreatePackage(&pkgInfo, comp, packagePath.c_str(), keyName.c_str());
        for (size_t i = 0; i < testFileNames_.size(); i++) {
            free(comp[i].componentAddr);
            free(comp[i].filePath);
        }
        EXPECT_EQ(ret, PKG_SUCCESS);
        uint8_t digest[DIGEST_LEN] = {0};
        ret = BuildFileDigest(*digest, sizeof(digest), packagePath);
        EXPECT_EQ(ret, PKG_SUCCESS);
        keyName = TEST_PATH_FROM;
        keyName += certName;
        ret = VerifyPackage(packagePath.c_str(), keyName.c_str(), "", digest, DIGEST_LEN);
        return ret;
    }

private:
    std::string testPackageName = "test_ecc_package.zip";
    std::vector<std::string> testFileNames_ = {
        "loadScript.us",
        "registerCmd.us",
        "test_function.us",
        "test_if.us",
        "test_logic.us",
        "test_math.us",
        "test_native.us",
        "testscript.us",
        "Verse-script.us",
        "libcrypto.a"
    };
};

HWTEST_F(PkgAlgoUnitTest, TestHash256Digest, TestSize.Level1)
{
    PkgAlgoUnitTest test;
    EXPECT_EQ(0, test.TestCrcDigest());
    EXPECT_EQ(0, test.TestHash256Digest());
    EXPECT_EQ(0, test.TestHash384Digest());
}

HWTEST_F(PkgAlgoUnitTest, TestRsaSignVerify, TestSize.Level1)
{
    PkgAlgoUnitTest test;
    EXPECT_EQ(0, test.TestSignVerify(PKG_SIGN_METHOD_RSA, "rsa_private_key2048.pem", "signing_cert.crt"));
}

HWTEST_F(PkgAlgoUnitTest, TestEccSignVerify, TestSize.Level1)
{
    PkgAlgoUnitTest test;
    EXPECT_EQ(0, test.TestSignVerify(PKG_SIGN_METHOD_ECDSA, "ecc/prime256v1-key.pem", "ecc/signing_cert.crt"));
}

HWTEST_F(PkgAlgoUnitTest, TestEccUserPackage, TestSize.Level1)
{
    PkgAlgoUnitTest test;
    EXPECT_EQ(0, test.TestEccUserPackage(PKG_SIGN_METHOD_ECDSA, "ecc/prime256v1-key.pem", "ecc/signing_cert.crt"));
}

HWTEST_F(PkgAlgoUnitTest, TestInvalid, TestSize.Level1)
{
    PkgAlgoUnitTest test;
    EXPECT_EQ(0, test.TestInvalidParam());
}

HWTEST_F(PkgAlgoUnitTest, TestPkgAlgoDeflate, TestSize.Level1)
{
    ZipFileInfo info {};
    PkgAlgoDeflate a1(info);
    Lz4FileInfo config {};
    PkgAlgorithmLz4 a2(config);
    PkgAlgorithmBlockLz4 a3(config);
    VerifyAlgorithm a4("aa", 0);
    SignAlgorithmRsa a5("bb", 0);
    SignAlgorithmEcc a6("cc", 0);
    // just for executing these destructor
    PkgAlgoDeflate *a7 = new PkgAlgoDeflate(info);
    delete a7;
    PkgAlgorithmLz4 *a8 = new PkgAlgorithmLz4(config);
    delete a8;
    PkgAlgorithmBlockLz4 *a9 = new PkgAlgorithmBlockLz4(config);
    delete a9;
    VerifyAlgorithm *a10 = new VerifyAlgorithm("aa", 0);
    delete a10;
    SignAlgorithmRsa *a11 = new SignAlgorithmRsa("bb", 0);
    delete a11;
    SignAlgorithmEcc *a12 = new SignAlgorithmEcc("cc", 0);
    std::vector<uint8_t> b1;
    std::vector<uint8_t> b2;
    int32_t ret = a12->VerifyBuffer(b1, b2);
    delete a12;
    EXPECT_EQ(ret, PKG_INVALID_SIGNATURE);
}
}
