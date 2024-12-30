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
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include "dump.h"
#include "openssl_util.h"
#include "pkcs7_signed_data.h"
#include "pkg_algo_sign.h"
#include "pkg_algorithm.h"
#include "pkg_manager_impl.h"
#include "pkg_utils.h"
#include "securec.h"
#include "zip_pkg_parse.h"

namespace Hpackage {
namespace {
constexpr uint32_t ZIP_EOCD_FIXED_PART_LEN = 22;
constexpr uint32_t PKG_FOOTER_SIZE = 6;
constexpr uint32_t PKG_HASH_CONTENT_LEN = SHA256_DIGEST_LENGTH;
constexpr uint32_t INTERCEPT_HASH_LENGTH = 8;
}

int32_t PkgVerifyUtil::VerifySourceDigest(std::vector<uint8_t> &signature, std::vector<uint8_t> &sourceDigest,
    const std::string & keyPath) const
{
    Updater::UPDATER_INIT_RECORD;
    std::vector<std::vector<uint8_t>> sigs;
    Pkcs7SignedData pkcs7;
    SignAlgorithm::SignAlgorithmPtr signAlgorithm = PkgAlgorithmFactory::GetVerifyAlgorithm(
        keyPath, PKG_DIGEST_TYPE_SHA256);
    int32_t ret = pkcs7.ReadSig(signature.data(), signature.size(), sigs);
    if (ret != PKCS7_SUCCESS) {
        UPDATER_LAST_WORD("pkcs7", ret);
        return ret;
    }
    for (auto &sig : sigs) {
        if (signAlgorithm->VerifyDigest(sourceDigest, sig) == 0) {
            return PKG_SUCCESS;
        }
    }
    return PKG_VERIFY_FAIL;
}

int32_t PkgVerifyUtil::VerifyAccPackageSign(const PkgStreamPtr pkgStream, const std::string &keyPath) const
{
    Updater::UPDATER_INIT_RECORD;
    if (pkgStream == nullptr) {
        UPDATER_LAST_WORD(PKG_INVALID_PARAM, "pkgStream is null");
        return PKG_INVALID_PARAM;
    }
    size_t signatureSize = 0;
    std::vector<uint8_t> signature;
    uint16_t commentTotalLenAll = 0;
    if (GetSignature(pkgStream, signatureSize, signature, commentTotalLenAll) != PKG_SUCCESS) {
        PKG_LOGE("get package signature fail!");
        UPDATER_LAST_WORD(PKG_INVALID_SIGNATURE, "get package signature fail!");
        return PKG_INVALID_SIGNATURE;
    }
    size_t srcDataLen = pkgStream->GetFileLength() - commentTotalLenAll -2;
    size_t readLen = 0;
    std::vector<uint8_t> sourceDigest;
    PkgBuffer digest(srcDataLen);
    pkgStream->Read(digest, 0, srcDataLen, readLen);
    sourceDigest.assign(digest.buffer, digest.buffer + readLen);
    return VerifySourceDigest(signature, sourceDigest, keyPath);
}

int32_t PkgVerifyUtil::VerifySign(std::vector<uint8_t> &signData, std::vector<uint8_t> &digest) const
{
    Updater::UPDATER_INIT_RECORD;
    std::vector<uint8_t> hash;
    int32_t ret = Pkcs7verify(signData, hash);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("pkcs7 verify fail!");
        UPDATER_LAST_WORD(ret, "pkcs7 verify fail!");
        return ret;
    }
    size_t hashLen = hash.size();
    if ((hashLen != digest.size()) || memcmp(hash.data(), digest.data(), hashLen) != EOK) {
        PKG_LOGE("Failed to memcmp data.");
        UPDATER_LAST_WORD(PKG_INVALID_DIGEST, "Failed to memcmp data.");
        return PKG_INVALID_DIGEST;
    }
    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::VerifyPackageSign(const PkgStreamPtr pkgStream, const std::string &path) const
{
    Updater::UPDATER_INIT_RECORD;
    if (pkgStream == nullptr) {
        UPDATER_LAST_WORD(PKG_INVALID_PARAM, "pkgStream is null");
        return PKG_INVALID_PARAM;
    }
    size_t signatureSize = 0;
    std::vector<uint8_t> signature;
    uint16_t commentTotalLenAll = 0;
    if (GetSignature(pkgStream, signatureSize, signature, commentTotalLenAll) != PKG_SUCCESS) {
        PKG_LOGE("get package signature fail!");
        UPDATER_LAST_WORD(PKG_INVALID_SIGNATURE, "get package signature fail!");
        return PKG_INVALID_SIGNATURE;
    }

    std::vector<uint8_t> hash;
    int32_t ret = Pkcs7verify(signature, hash);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("pkcs7 verify fail!");
        UPDATER_LAST_WORD(ret, "pkcs7 verify fail!");
        return ret;
    }
    size_t srcDataLen = pkgStream->GetFileLength() - commentTotalLenAll - 2;

    ret = HashCheck(pkgStream, srcDataLen, hash, path);
    if (ret != PKG_SUCCESS) {
        srcDataLen = pkgStream->GetFileLength() - signatureSize - ZIP_EOCD_FIXED_PART_LEN;
        ret = HashCheck(pkgStream, srcDataLen, hash, path);
    }
    if (ret == PKG_SUCCESS) {
        UPDATER_CLEAR_RECORD;
    }
    PKG_LOGI("verify package signature %s", ret == PKG_SUCCESS ? "successfull" : "failed");
    return ret;
}

int32_t PkgVerifyUtil::GetSignature(const PkgStreamPtr pkgStream, size_t &signatureSize,
    std::vector<uint8_t> &signature, uint16_t &commentTotalLenAll) const
{
    Updater::UPDATER_INIT_RECORD;
    size_t signatureStart = 0;
    int32_t ret = ParsePackage(pkgStream, signatureStart, signatureSize, commentTotalLenAll);
    if (ret != PKG_SUCCESS || signatureSize < PKG_FOOTER_SIZE) {
        PKG_LOGE("Parse package failed.");
        UPDATER_LAST_WORD(-1, "Parse package failed.");
        return -1;
    }

    size_t signDataLen = signatureSize - PKG_FOOTER_SIZE;
    PkgBuffer signData(signDataLen);
    size_t readLen = 0;
    ret = pkgStream->Read(signData, signatureStart, signDataLen, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("read signature failed %s", pkgStream->GetFileName().c_str());
        UPDATER_LAST_WORD(ret, "read signature failed " + pkgStream->GetFileName());
        return ret;
    }
    signature.assign(signData.buffer, signData.buffer + readLen);

    size_t fileLen = pkgStream->GetFileLength();
    if (fileLen < (signatureSize + ZIP_EOCD_FIXED_PART_LEN)) {
        PKG_LOGE("Invalid fileLen[%zu] and signature size[%zu]", fileLen, signatureSize);
        UPDATER_LAST_WORD(PKG_INVALID_PARAM, fileLen, signatureSize);
        return PKG_INVALID_PARAM;
    }

    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::ParsePackage(const PkgStreamPtr pkgStream, size_t &signatureStart,
    size_t &signatureSize, uint16_t &commentTotalLenAll) const
{
    Updater::UPDATER_INIT_RECORD;
    ZipPkgParse zipParse;
    PkgSignComment pkgSignComment {};
    int32_t ret = zipParse.ParseZipPkg(pkgStream, pkgSignComment);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse zip package signature failed.");
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    signatureStart = pkgStream->GetFileLength() - pkgSignComment.signCommentAppendLen;
    signatureSize = pkgSignComment.signCommentAppendLen;
    commentTotalLenAll = pkgSignComment.signCommentTotalLen;

    return PKG_SUCCESS;
}

int32_t PkgVerifyUtil::Pkcs7verify(std::vector<uint8_t> &signature, std::vector<uint8_t> &hash) const
{
    Pkcs7SignedData pkcs7;

    return pkcs7.GetHashFromSignBlock(signature.data(), signature.size(), hash);
}

int32_t PkgVerifyUtil::HashCheck(const PkgStreamPtr srcData, const size_t dataLen,
    const std::vector<uint8_t> &hash, const std::string &path) const
{
    Updater::UPDATER_INIT_RECORD;
    struct stat statInfo {};
    std::string fileInfo = "valid info";
    if (stat(path.c_str(), &statInfo) != 0) {
        PKG_LOGE("get file info error");
    } else {
        fileInfo = "pkg size is " + std::to_string(statInfo.st_size) +
            " , pkg last change time is " + ctime(&statInfo.st_mtime);
        PKG_LOGI(fileInfo.c_str());
    }
    if (srcData == nullptr || dataLen == 0) {
        UPDATER_LAST_WORD(PKG_INVALID_PARAM);
        return PKG_INVALID_PARAM;
    }

    size_t digestLen = hash.size();
    if (digestLen != PKG_HASH_CONTENT_LEN) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        UPDATER_LAST_WORD(PKG_INVALID_PARAM, fileInfo);
        return PKG_INVALID_PARAM;
    }
    std::vector<uint8_t> sourceDigest(digestLen);
    int32_t ret = CalcSha256Digest(srcData, dataLen, sourceDigest);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("calc pkg sha256 digest failed.");
        UPDATER_LAST_WORD(ret, fileInfo);
        return ret;
    }

    if (memcmp(hash.data(), sourceDigest.data(), digestLen) != EOK) {
        PKG_LOGW("Failed to memcmp data.");
        UPDATER_LAST_WORD(PKG_INVALID_DIGEST,
                          ConvertShaHex(hash).substr(0, INTERCEPT_HASH_LENGTH),
                          ConvertShaHex(sourceDigest).substr(0, INTERCEPT_HASH_LENGTH), fileInfo);
        return PKG_INVALID_DIGEST;
    }

    return PKG_SUCCESS;
}
} // namespace Hpackage
