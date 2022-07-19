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

#include "updaterfactoryreset_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "mount.h"
#include "updater_main.h"

using namespace Updater;
static constexpr int FSTAB_NAME_LENGTH = 20;

namespace OHOS {
    bool FuzzFactoryReset(const uint8_t* data, size_t size)
    {
        FactoryResetMode mode = USER_WIPE_DATA;

        if (size < FSTAB_NAME_LENGTH) {  /* fstable name length */
            LoadSpecificFstab("/data/fuzz/test/FormatPartition_fuzzer.fstable");
            FactoryReset(mode, reinterpret_cast<const char*>(data));
        }

        return 0;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzFactoryReset(data, size);
    return 0;
}

