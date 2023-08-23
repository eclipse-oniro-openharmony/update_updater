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

#include "package_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "log/log.h"
#include "package.h"

using namespace Updater;
namespace OHOS {
    void FuzzVerifyPackage(const uint8_t* data, size_t size)
    {
        constexpr size_t digestSize = 32;
        std::vector<uint8_t> digest(digestSize);
        const std::string keyPath = "/data/fuzz/test/signing_cert.crt";
        const std::string pkgPath = "/data/fuzz/test/updater.zip";
        const std::string pkgDir = "/data/fuzz/test";
        VerifyPackage(reinterpret_cast<const char*>(data), keyPath.c_str(), "", digest.data(), digest.capacity());
        VerifyPackage(pkgPath.c_str(), reinterpret_cast<const char*>(data), "", digest.data(), digest.capacity());
        VerifyPackage(pkgPath.c_str(), keyPath.c_str(), reinterpret_cast<const char*>(data),
            digest.data(), digest.capacity());
        VerifyPackage(pkgPath.c_str(), keyPath.c_str(), "", data, size);

        VerifyPackageWithCallback(reinterpret_cast<const char*>(data), keyPath.c_str(),
            [](int32_t result, uint32_t percent) {});
        VerifyPackageWithCallback(pkgPath, reinterpret_cast<const char*>(data),
            [](int32_t result, uint32_t percent) {});

        ExtraPackageDir(reinterpret_cast<const char*>(data), keyPath.c_str(), nullptr, pkgDir.c_str());
        ExtraPackageDir(pkgPath.c_str(), reinterpret_cast<const char*>(data), nullptr, pkgDir.c_str());
        ExtraPackageDir(pkgPath.c_str(), keyPath.c_str(), reinterpret_cast<const char*>(data), pkgDir.c_str());
        ExtraPackageDir(pkgPath.c_str(), keyPath.c_str(), nullptr, reinterpret_cast<const char*>(data));

        const std::string file = "updater.bin";
        ExtraPackageFile(reinterpret_cast<const char*>(data), keyPath.c_str(), file.c_str(), pkgDir.c_str());
        ExtraPackageFile(pkgPath.c_str(), reinterpret_cast<const char*>(data), file.c_str(), pkgDir.c_str());
        ExtraPackageFile(pkgPath.c_str(), keyPath.c_str(), reinterpret_cast<const char*>(data), pkgDir.c_str());
        ExtraPackageFile(pkgPath.c_str(), keyPath.c_str(), file.c_str(), reinterpret_cast<const char*>(data));
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzVerifyPackage(data, size);
    return 0;
}

