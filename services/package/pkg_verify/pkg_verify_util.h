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

#ifndef PKG_VERIFY_UTIL_H
#define PKG_VERIFY_UTIL_H

#include <vector>
#include "pkcs7_signed_data.h"
#include "pkg_stream.h"

namespace hpackage {
struct DigestBlock {
    uint16_t algorithmId;
    uint16_t digestLen;
    std::vector<uint8_t> digestData;
};

class PkgVerifyUtil {
public:
    explicit PkgVerifyUtil(const std::string &publicKey) : pubKey_(publicKey) {}

    ~PkgVerifyUtil() {}

    int32_t VerifyPkcs7SignedData(const hpackage::PkgStreamPtr pkgStream, const size_t signatureStart,
        const size_t signatureSize);

private:
    int32_t VerifySignedData(const hpackage::PkgStreamPtr srcData, const size_t dataLen,
        std::vector<Pkcs7SignerInfo> &signerInfos) const;

    int32_t ParseDigestBlock(const std::vector<uint8_t> &digestBlock);

    int32_t VerifyDigestEncryptData(const hpackage::PkgStreamPtr srcData, const size_t dataLen,
        std::vector<Pkcs7SignerInfo> &signerInfos) const;

    int32_t SingleDigestEncryptVerify(Pkcs7SignerInfo &signer,
        const hpackage::PkgStreamPtr srcData, const size_t dataLen) const;

    int32_t VerifyPackageDigest(const std::vector<uint8_t> &digestEncryptData,
    const std::vector<uint8_t> &digest, const uint8_t digestMethod) const;

    int32_t HashCheck(const hpackage::PkgStreamPtr srcData, const size_t dataLen) const;

private:
    std::string pubKey_;
    DigestBlock digestBlock_ {};
};

class SignPkg {
public:
    SignPkg(hpackage::PkgStreamPtr inStream, const std::string &keyPath, uint32_t signMethod)
        : pkgStream_(inStream), privateKey_(keyPath), signMethod_(signMethod) {}

    ~SignPkg() {}

    int32_t SignPackage(hpackage::PkgStreamPtr outStream) const;

private:
    int32_t SignPackageDigest(std::vector<uint8_t> &digest, std::vector<uint8_t> &signData) const;

    int32_t CreateSignResult(std::vector<uint8_t> &digestBlock, std::vector<uint8_t> &signedData) const;

    int32_t CreateDigestBlock(std::vector<uint8_t> &digestBlock, const std::vector<uint8_t> &digest) const;

private:
    hpackage::PkgStreamPtr pkgStream_;
    std::string privateKey_;
    uint32_t signMethod_;
};

int32_t CalcSha256ByBlock(const hpackage::PkgStreamPtr srcData, const size_t dataLen, std::vector<uint8_t> &result);
} // namespace hpackage
#endif
