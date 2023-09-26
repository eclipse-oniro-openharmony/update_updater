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

#include "rebootandinstallupgradepackage_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "log/log.h"
#include "updaterkits/updaterkits.h"
#include "securec.h"

using namespace Updater;
namespace OHOS {
    void FuzzUpdaterKits(const uint8_t* data, size_t size)
    {
        // const std::string miscFile = "/dev/block/by_name/misc";
        const std::vector<std::string> packageName {std::string(reinterpret_cast<const char*>(data), size)};
        RebootAndInstallUpgradePackage("", packageName);
        RebootAndCleanUserData("", std::string(reinterpret_cast<const char*>(data), size));
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzUpdaterKits(data, size);
    return 0;
}

