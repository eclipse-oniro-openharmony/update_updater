/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "hash_data_verifier.h"
#include "log/dump.h"
#include "openssl_util.h"
#include "package/pkg_manager.h"
#include "pkcs7_signed_data.h"
#include "rust/hash_signed_data.h"
#include "updater/updater_const.h"
#include "zip_pkg_parse.h"

namespace Hpackage {
using namespace Updater;
constexpr static std::size_t MAX_SIG_SIZE = 1024;
constexpr const char *UPDATER_HASH_SIGNED_DATA = "hash_signed_data";

HashDataVerifier::HashDataVerifier(PkgManager::PkgManagerPtr manager)
    : manager_(manager), pkcs7_(std::make_unique<Pkcs7SignedData>()) {}

HashDataVerifier::~HashDataVerifier()
{
    ReleaseHashSignedData(hsd_);
}

bool HashDataVerifier::LoadHashDataAndPkcs7(const std::string &pkgPath)
{
    // only allow loading once
    if (hsd_ != nullptr) {
        PKG_LOGW("hash signed data has been loaded before");
        return true;
    }
    if (manager_ == nullptr) {
        PKG_LOGE("pkg manager is null");
        UPDATER_LAST_WORD(false);
        return false;
    }
    // load pkcs7 from package
    if (!LoadPkcs7FromPackage(pkgPath)) {
        PKG_LOGE("load pkcs7 from %s failed", pkgPath.c_str());
        UPDATER_LAST_WORD(PKG_INVALID_FILE, pkgPath);
        return false;
    }
    if (!LoadHashDataFromPackage()) {
        PKG_LOGE("load pkcs7 from %s failed", pkgPath.c_str());
        UPDATER_LAST_WORD(PKG_INVALID_FILE, pkgPath);
        return false;
    }
    return true;
}

bool HashDataVerifier::LoadHashDataFromPackage(void)
{
    Updater::UPDATER_INIT_RECORD;
    PkgManager::StreamPtr outStream = nullptr;
    auto info = manager_->GetFileInfo(UPDATER_HASH_SIGNED_DATA);
    if (info == nullptr || info->unpackedSize == 0) {
        PKG_LOGE("hash signed data not find in pkg manager");
        UPDATER_LAST_WORD(false);
        return false;
    }
    // 1 more byte bigger than unpacked size to ensure a ending '\0' in buffer
    PkgBuffer buffer {info->unpackedSize + 1};
    int32_t ret = manager_->CreatePkgStream(outStream, "", buffer);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("create stream fail");
        UPDATER_LAST_WORD(false);
        return false;
    }
    ret = manager_->ExtractFile(UPDATER_HASH_SIGNED_DATA, outStream);
    if (ret != PKG_SUCCESS) {
        manager_->ClosePkgStream(outStream);
        PKG_LOGE("extract file failed");
        UPDATER_LAST_WORD(false);
        return false;
    }
    hsd_ = LoadHashSignedData(reinterpret_cast<const char *>(buffer.data.data()));
    manager_->ClosePkgStream(outStream);
    if (hsd_ == nullptr) {
        PKG_LOGE("load hash signed data failed");
        UPDATER_LAST_WORD(false);
        return false;
    }
    return true;
}

bool HashDataVerifier::LoadPkcs7FromPackage(const std::string &pkgPath)
{
    PkgManager::StreamPtr pkgStream = nullptr;
    int32_t ret = manager_->CreatePkgStream(pkgStream, pkgPath, 0, PkgStream::PkgStreamType_Read);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("CreatePackage fail %s", pkgPath.c_str());
        UPDATER_LAST_WORD(PKG_INVALID_FILE, pkgPath);
        UPDATER_LAST_WORD(false);
        return false;
    }

    PkgVerifyUtil verifyUtil {};
    size_t signatureSize = 0;
    std::vector<uint8_t> signature {};
    ret = verifyUtil.GetSignature(PkgStreamImpl::ConvertPkgStream(pkgStream), signatureSize, signature);
    manager_->ClosePkgStream(pkgStream);
    if (ret != PKG_SUCCESS) {
        UPDATER_LAST_WORD(ret);
        return false;
    }
    return pkcs7_ != nullptr && pkcs7_->ParsePkcs7Data(signature.data(), signature.size()) == 0;
}

bool HashDataVerifier::VerifyHashData(const std::string &fileName, PkgManager::StreamPtr stream) const
{
    Updater::UPDATER_INIT_RECORD;
    if (stream == nullptr) {
        PKG_LOGE("stream is null");
        UPDATER_LAST_WORD(false);
        return false;
    }

    // get hash from stream
    std::vector<uint8_t> hash {};
    int32_t ret = CalcSha256Digest(PkgStreamImpl::ConvertPkgStream(stream), stream->GetFileLength(), hash);
    if (ret != 0) {
        PKG_LOGE("cal digest for pkg stream");
        UPDATER_LAST_WORD(false, fileName);
        return false;
    }

    // get sig from hash data
    std::string name = std::string("build_tools/") + fileName;
    std::vector<uint8_t> sig(MAX_SIG_SIZE, 0);
    auto sigLen = GetSigFromHashData(hsd_, sig.data(), sig.size(), name.c_str());
    if (sigLen == 0 || sig.size() < sigLen) {
        PKG_LOGE("get sig for %s failed", name.c_str());
        UPDATER_LAST_WORD(PKG_INVALID_SIGNATURE, fileName, sigLen);
        return false;
    }
    sig.resize(sigLen);

    // then using cert from pkcs7 to verify hash data
    if (pkcs7_ == nullptr || pkcs7_->Verify(hash, sig, false) != 0) {
        PKG_LOGE("verify hash signed data for %s failed", fileName.c_str());
        UPDATER_LAST_WORD(PKG_INVALID_SIGNATURE, fileName);
        return false;
    }
    PKG_LOGI("verify hash signed data for %s successfully", fileName.c_str());
    return true;
}
} // namespace Hpackage
