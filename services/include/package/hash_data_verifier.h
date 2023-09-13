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

#ifndef HASH_DATA_VERIFIER_H
#define HASH_DATA_VERIFIER_H

#include "pkg_manager.h"

struct HashSignedData;

namespace Hpackage {
class Pkcs7SignedData;
// class for verification of hash signed data
class HashDataVerifier final {
public:
    HashDataVerifier(PkgManager::PkgManagerPtr manager);
    bool LoadHashDataAndPkcs7(const std::string &pkgPath);
    bool VerifyHashData(const std::string &preName, const std::string &fileName, PkgManager::StreamPtr stream) const;
    bool LoadPkcs7FromPackage(const std::string &pkgPath);
    bool LoadHashDataFromPackage(const std::string &buffer);
    ~HashDataVerifier();
private:
    bool LoadHashDataFromPackage(void);
    PkgManager::PkgManagerPtr manager_ {nullptr};
    std::unique_ptr<Pkcs7SignedData> pkcs7_ {nullptr};
    const HashSignedData *hsd_ {nullptr};
    bool isNeedVerify = true;
};
}

#endif