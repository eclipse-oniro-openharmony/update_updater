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

#include "writeupdatermsg_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "securec.h"

using namespace Updater;
namespace OHOS {
    void FuzzWriteUpdaterMsg(const uint8_t* data, size_t size)
    {
        UpdateMessage boot;
        if (memcpy_s(boot.update, MAX_UPDATE_SIZE - 1, reinterpret_cast<const char*>(data), size) != EOK) {
            return;
        }
        ReadUpdaterMiscMsg(boot);
        const std::string path = "/dev/block/by_name/misc";
        WriteUpdaterMessage(path, boot);
        WriteUpdaterMiscMsg(boot);
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzWriteUpdaterMsg(data, size);
    return 0;
}

