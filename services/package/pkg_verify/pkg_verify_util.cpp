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

#include "pkg_verify_util.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <unistd.h>
#include "pkcs7_signed_data.h"
#include "pkg_algorithm.h"
#include "pkg_utils.h"
#include "securec.h"
#include "zip_pkg_parse.h"

namespace Hpackage {
namespace {
static constexpr uint32_t HASH_SOURCE_BLOCK_LEN = 4096;
static constexpr uint32_t ZIP_EOCD_FIXED_PART_LEN = 22;
static constexpr uint32_t PKG_FOOTER_SIZE = 6;
static constexpr uint32_t PKG_HASH_CONTENT_LEN = SHA256_DIGEST_LENGTH;

static int g_algIdAndNid[][2] = {
    {NID_sha256, PKG_DIGEST_TYPE_SHA256},
};

static int ConvertNidToMethod(int algId)
{
    int nid = PKG_DIGEST_TYPE_NONE;
    for (int i = 0; i < sizeof(g_algIdAndNid) / sizeof(g_algIdAndNid[0]); i++) {
        if (algId == g_algIdAndNid[i][0]) {
            nid = g_algIdAndNid[i][1];
        }
    }

    return nid;
}
}

int32_t SignPkg::SignPackage(PkgStreamPtr outStream) const
{
    std::vector<uint8_t> digestBlock;
    std::vector<uint8_t> signData;
    int32_t ret = CreateSignResult(digestBlock, signData);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Create sign result failed.");
        return ret;
    }

    PkgBuffer p7Data;
    Pkcs7SignedData pkcs7;
    ret = pkcs7.BuildPkcs7SignedData(p7Data, signMethod_, digestBlock, signData);
    if (p7Data.buffer == nullptr || p7Data.length == 0) {
        PKG_LOGE("Create pkcs7 signed data failed.");
        return PKG_INVALID_SIGNATURE;
    }

    ZipPkgParse zipParse;
    ret = zipParse.CheckZipPkg(pkgStream_);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Invalid unsigned zip format.");
        delete [] p7Data.buffer;
        return ret;
    }

    ret = zipParse.WriteZipSignedData(outStream, p7Data, pkgStream_);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Invalid unsigned zip format.");
        delete [] p7Data.buffer;
        return ret;
    }

    delete [] p7Data.buffer;
    return PKG_SUCCESS;
}

int32_t SignPkg::CreateSignResult(std::vector<uint8_t> &digestBlock, std::vector<uint8_t> &signedData) const
{
    size_t fileLen = pkgStream_->GetFileLength();
    PKG_CHECK(fileLen > ZIP_EOCD_FIXED_PART_LEN, return PKG_INVALID_PARAM, "Invalid zip file ");
    size_t srcDataLen = fileLen - ZIP_EOCD_FIXED_PART_LEN;

    std::vector<uint8_t> digest(PKG_HASH_CONTENT_LEN);
    int32_t ret = CalcSha256ByBlock(pkgStream_, srcDataLen, digest);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        return ret;
    }

    ret = CreateDigestBlock(digestBlock, digest);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("create digest content failed");
        return ret;
    }

    return SignPackageDigest(digest, signedData);
}

int32_t SignPkg::CreateDigestBlock(std::vector<uint8_t> &digestBlock, const std::vector<uint8_t> &digest) const
{
    if (digest.empty()) {
        return PKG_INVALID_PARAM;
    }
    size_t digestBlockLen = digest.size() + sizeof(uint32_t);
    digestBlock.resize(digestBlockLen);

    size_t offset = 0;
    WriteLE16(digestBlock.data(), EVP_MD_type(EVP_sha256()));
    offset += sizeof(uint16_t);
    WriteLE16(digestBlock.data() + offset, digest.size());
    offset += sizeof(uint16_t);

    PKG_CHECK(digestBlock.size() >= offset, return PKG_INVALID_PARAM, "digestBlock size ");
    int32_t ret = memcpy_s(digestBlock.data() + offset,
        digestBlock.size() - offset, digest.data(), digest.size());
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s digestData, dataLen: %d", digest.size());
        return PKG_NONE_MEMORY;
    }

    return PKG_SUCCESS;
}

int32_t SignPkg::SignPackageDigest(std::vector<uint8_t> &digest, std::vector<uint8_t> &signData) const
{
    if (digest.size() != PKG_HASH_CONTENT_LEN) {
        return PKG_INVALID_PARAM;
    }
    SignAlgorithm::SignAlgorithmPtr signAlgorithm =
        PkgAlgorithmFactory::GetSignAlgorithm(privateKey_, signMethod_, PKG_DIGEST_TYPE_SHA256);
    if (signAlgorithm == nullptr) {
        PKG_LOGE("Invalid sign algo");
        return PKG_INVALID_ALGORITHM;
    }

    PkgBuffer digestBuffer(digest);
    signData.clear();
    size_t signDataLen = 0;
    int32_t ret = signAlgorithm->SignBuffer(digestBuffer, signData, signDataLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to Sign package");
        return ret;
    }

    PKG_LOGI("SignPackageBuffer success %u", signDataLen);
    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::VerifyPkcs7SignedData(const PkgStreamPtr pkgStream,
    const size_t signatureStart, const size_t signatureSize)
{
    size_t fileLen = pkgStream->GetFileLength();
    PKG_CHECK(fileLen > signatureSize, return PKG_INVALID_PARAM, "Invalid signature size[%zu]", signatureSize);

    std::vector<uint8_t> digestBlock;
    std::vector<Pkcs7SignerInfo> signerInfos;

    size_t p7DataLen = signatureSize - PKG_FOOTER_SIZE;
    PkgBuffer p7Data(p7DataLen);
    size_t readLen = 0;
    int32_t ret = pkgStream->Read(p7Data, signatureStart, p7DataLen, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "read signature failed", pkgStream->GetFileName().c_str());

    Pkcs7SignedData pkcs7;
    ret = pkcs7.ParsePkcs7SignedData(p7Data.buffer, p7Data.length, digestBlock, signerInfos);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ParsePkcs7SignedData() error, ret[%d]", ret);
        return ret;
    }

    ret = ParseDigestBlock(digestBlock);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse digest block failed.");
        return ret;
    }
    PKG_CHECK(fileLen > (signatureSize + ZIP_EOCD_FIXED_PART_LEN),
        return PKG_INVALID_PARAM, "Invalid signature srcDataLen");
    size_t srcDataLen = fileLen - signatureSize - ZIP_EOCD_FIXED_PART_LEN;

    return VerifySignedData(pkgStream, srcDataLen, signerInfos);
}

int32_t PkgVerifyUtil::VerifySignedData(const PkgStreamPtr srcData, const size_t dataLen,
    std::vector<Pkcs7SignerInfo> &signerInfos) const
{
    int32_t ret = VerifyDigestEncryptData(srcData, dataLen, signerInfos);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Verify digest encryptData failed.");
        return ret;
    }

    return HashCheck(srcData, dataLen);
}

int32_t PkgVerifyUtil::ParseDigestBlock(const std::vector<uint8_t> &digestBlock)
{
    if (digestBlock.size() == 0) {
        PKG_LOGE("Invalid digest block info.");
        return PKG_INVALID_PARAM;
    }

    size_t offset = 0;
    digestBlock_.algorithmId = ReadLE16(digestBlock.data() + offset);
    offset += sizeof(uint16_t);
    digestBlock_.digestLen =  ReadLE16(digestBlock.data() + offset);
    offset += sizeof(uint16_t);
    if ((digestBlock_.digestLen + offset) != digestBlock.size()) {
        PKG_LOGE("Invalid digestLen[%zu] and digestBlock len[%zu]", digestBlock_.digestLen, digestBlock.size());
        return PKG_INVALID_DIGEST;
    }
    digestBlock_.digestData.resize(digestBlock_.digestLen);
    int32_t ret = memcpy_s(digestBlock_.digestData.data(), digestBlock_.digestLen,
        digestBlock.data() + offset, digestBlock_.digestLen);
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s subBlock, dataLen: %d", digestBlock_.digestLen);
        return PKG_INVALID_DIGEST;
    }

    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::VerifyDigestEncryptData(const PkgStreamPtr srcData, const size_t dataLen,
    std::vector<Pkcs7SignerInfo> &signerInfos) const
{
    if (signerInfos.size() == 0) {
        PKG_LOGE("Invalid signer info.");
        return PKG_INVALID_PARAM;
    }

    int32_t ret = PKG_SUCCESS;
    std::vector<Pkcs7SignerInfo>::iterator iter;
    for (iter = signerInfos.begin(); iter < signerInfos.end(); iter++) {
        ret = SingleDigestEncryptVerify(*iter, srcData, dataLen);
        if (ret == PKG_SUCCESS) {
            PKG_LOGE("pkcs7 signer info verify success.");
            break;
        }
    }

    return ret;
}

int32_t PkgVerifyUtil::SingleDigestEncryptVerify(Pkcs7SignerInfo &signer,
    const PkgStreamPtr srcData, const size_t dataLen) const
{
    uint8_t digestMethod = static_cast<uint8_t>(ConvertNidToMethod(signer.digestNid));

    return VerifyPackageDigest(signer.digestEncryptData, digestBlock_.digestData, digestMethod);
}

int32_t PkgVerifyUtil::VerifyPackageDigest(const std::vector<uint8_t> &digestEncryptData,
    const std::vector<uint8_t> &digest, const uint8_t digestMethod) const
{
    if (digestMethod > PKG_DIGEST_TYPE_MAX) {
        PKG_LOGE("Invalid digest digestMethod %zu.", digestMethod);
        return PKG_INVALID_SIGNATURE;
    }
    if (digestEncryptData.size() == 0) {
        PKG_LOGE("Digest EncryptData is empty.");
        return PKG_INVALID_SIGNATURE;
    }

    SignAlgorithm::SignAlgorithmPtr verifyAlgorithm =
        PkgAlgorithmFactory::GetVerifyAlgorithm(pubKey_, digestMethod);
    if (verifyAlgorithm == nullptr) {
        PKG_LOGE("Invalid verify algo");
        return PKG_INVALID_SIGNATURE;
    }

    return verifyAlgorithm->VerifyBuffer(digest, digestEncryptData);
}

int32_t PkgVerifyUtil::HashCheck(const PkgStreamPtr srcData, const size_t dataLen) const
{
    if (srcData == nullptr || dataLen == 0) {
        return PKG_INVALID_PARAM;
    }
    if (digestBlock_.digestData.empty()) {
        PKG_LOGE("Invalid digest block vector.");
        return PKG_INVALID_DIGEST;
    }

    size_t digestLen = digestBlock_.digestLen;
    if (digestLen != PKG_HASH_CONTENT_LEN) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        return PKG_INVALID_PARAM;
    }
    std::vector<uint8_t> sourceDigest(digestBlock_.digestLen);
    int32_t ret = CalcSha256ByBlock(srcData, dataLen, sourceDigest);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        return ret;
    }

    if (memcmp(digestBlock_.digestData.data(), sourceDigest.data(), digestLen) != EOK) {
        PKG_LOGE("Failed to memcmp data.");
        return PKG_INVALID_DIGEST;
    }

    return PKG_SUCCESS;
}

int32_t CalcSha256ByBlock(const PkgStreamPtr srcData, const size_t dataLen, std::vector<uint8_t> &result)
{
    if (srcData == nullptr || dataLen == 0) {
        return PKG_INVALID_PARAM;
    }
    if (result.size() != PKG_HASH_CONTENT_LEN) {
        PKG_LOGE("Invalid digest result len %zu.", result.size());
        return PKG_INVALID_DIGEST;
    }
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    size_t offset = 0;
    size_t remainLen = dataLen;
    size_t blockLen = HASH_SOURCE_BLOCK_LEN;
    PkgBuffer buffer(blockLen);
    size_t readLen = 0;
    int32_t ret = PKG_SUCCESS;
    while (remainLen >= blockLen) {
        ret = srcData->Read(buffer, offset, blockLen, readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail read data");
        SHA256_Update(&ctx, buffer.buffer, blockLen);
        offset += blockLen;
        remainLen -= blockLen;
    }
    if (remainLen > 0) {
        ret = srcData->Read(buffer, offset, remainLen, readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail read data");
        SHA256_Update(&ctx, buffer.buffer, remainLen);
    }

    if (SHA256_Final(result.data(), &ctx) != 1) {
        PKG_LOGE("SHA256_Final(), error\n");
        return PKG_INVALID_DIGEST;
    }

    return PKG_SUCCESS;
}
} // namespace Hpackage
