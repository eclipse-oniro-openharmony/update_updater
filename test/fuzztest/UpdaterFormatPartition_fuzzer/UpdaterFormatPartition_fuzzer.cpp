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

#include "UpdaterFormatPartition_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "fuzz_utils.h"
#include "mount.h"

using namespace Updater;

namespace OHOS {
    void FuzzFormatPartition(const uint8_t* data, size_t size)
    {
        CloseStdout();
        LoadSpecificFstab("/data/fuzz/test/FormatPartition_fuzzer.fstable");
        FormatPartition(std::string(reinterpret_cast<const char*>(data), size), false);
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzFormatPartition(data, size);
    return 0;
}

