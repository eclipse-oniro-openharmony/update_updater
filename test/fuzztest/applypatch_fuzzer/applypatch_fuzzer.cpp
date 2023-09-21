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

#include "diff_patch/diff_patch_interface.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace Updater;
namespace OHOS {
    void FuzzApplyPatch(const uint8_t* data, size_t size)
    {
        const std::string filePath = "/data/fuzz/test/MountForPath_fuzzer.fstable";
        ApplyPatch(filePath, filePath, std::string(reinterpret_cast<const char*>(data), size));
        ApplyPatch(filePath, std::string(reinterpret_cast<const char*>(data), size), filePath);
        ApplyPatch(std::string(reinterpret_cast<const char*>(data), size), filePath, filePath);
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzApplyPatch(data, size);
    return 0;
}

