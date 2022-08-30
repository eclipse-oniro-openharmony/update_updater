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
#include <unistd.h>
#include "dump.h"
#include "openssl_util.h"
#include "pkcs7_signed_data.h"
#include "pkg_utils.h"
#include "securec.h"
#include "zip_pkg_parse.h"

namespace Hpackage {
namespace {
constexpr uint32_t ZIP_EOCD_FIXED_PART_LEN = 22;
constexpr uint32_t PKG_FOOTER_SIZE = 6;
constexpr uint32_t PKG_HASH_CONTENT_LEN = SHA256_DIGEST_LENGTH;
}

int32_t PkgVerifyUtil::VerifyPackageSign(const PkgStreamPtr pkgStream) const
{
    if (pkgStream == nullptr) {
        return PKG_INVALID_PARAM;
    }
    size_t signatureSize = 0;
    std::vector<uint8_t> signature;
    if (GetSignature(pkgStream, signatureSize, signature) != PKG_SUCCESS) {
        PKG_LOGE("get package signature fail!");
        return PKG_INVALID_SIGNATURE;
    }
    size_t fileLen = pkgStream->GetFileLength();
    if (fileLen < (signatureSize + ZIP_EOCD_FIXED_PART_LEN)) {
        PKG_LOGE("Invalid fileLen[%zu] and signature size[%zu]", fileLen, signatureSize);
        UPDATER_LAST_WORD(PKG_INVALID_PARAM, fileLen, signatureSize);
        return PKG_INVALID_PARAM;
    }

    std::vector<uint8_t> hash;
    int32_t ret = Pkcs7verify(signature, hash);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("pkcs7 verify fail!");
        return ret;
    }
    size_t srcDataLen = fileLen - signatureSize - ZIP_EOCD_FIXED_PART_LEN;

    return HashCheck(pkgStream, srcDataLen, hash);
}

int32_t PkgVerifyUtil::GetSignature(const PkgStreamPtr pkgStream, size_t &signatureSize,
    std::vector<uint8_t> &signature) const
{
    size_t signatureStart = 0;
    int32_t ret = ParsePackage(pkgStream, signatureStart, signatureSize);
    if (ret != PKG_SUCCESS || signatureSize < PKG_FOOTER_SIZE) {
        PKG_LOGE("Parse package failed.");
        return -1;
    }

    size_t signDataLen = signatureSize - PKG_FOOTER_SIZE;
    PkgBuffer signData(signDataLen);
    size_t readLen = 0;
    ret = pkgStream->Read(signData, signatureStart, signDataLen, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("read signature failed %s", pkgStream->GetFileName().c_str());
        return ret;
    }
    signature.assign(signData.buffer, signData.buffer + readLen);

    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::ParsePackage(const PkgStreamPtr pkgStream, size_t &signatureStart,
    size_t &signatureSize) const
{
    ZipPkgParse zipParse;
    int32_t ret = zipParse.ParseZipPkg(pkgStream, signatureStart, signatureSize);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse zip package signature failed.");
        return ret;
    }

    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::Pkcs7verify(std::vector<uint8_t> &signature, std::vector<uint8_t> &hash) const
{
    Pkcs7SignedData pkcs7;

    return pkcs7.GetHashFromSignBlock(signature.data(), signature.size(), hash);
}

int32_t PkgVerifyUtil::HashCheck(const PkgStreamPtr srcData, const size_t dataLen,
    const std::vector<uint8_t> &hash) const
{
    if (srcData == nullptr || dataLen == 0) {
        return PKG_INVALID_PARAM;
    }

    size_t digestLen = hash.size();
    if (digestLen != PKG_HASH_CONTENT_LEN) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        return PKG_INVALID_PARAM;
    }
    std::vector<uint8_t> sourceDigest(digestLen);
    int32_t ret = CalcSha256Digest(srcData, dataLen, sourceDigest);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        return ret;
    }

    if (memcmp(hash.data(), sourceDigest.data(), digestLen) != EOK) {
        PKG_LOGE("Failed to memcmp data.");
        return PKG_INVALID_DIGEST;
    }

    return PKG_SUCCESS;
}
} // namespace Hpackage
