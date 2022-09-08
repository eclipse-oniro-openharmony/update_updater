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

#include "dopartitions_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "log/log.h"
#include "partitions.h"
#include "securec.h"

using namespace Updater;
namespace {
static constexpr int FSTAB_NAME_LENGTH = 20;
}
static void InitEmmcPartition(struct Partition &part, const std::string &partName, size_t start, size_t length)
{
    part.partName = partName;
    part.start = start;
    part.length = length;
    /* Paramters below just give a random values, DoPartition will ignore the values. */
    part.devName = "mmcblk0px";
    part.fsType = "emmc";
}

namespace OHOS {
    bool FuzzDoPartitions(const uint8_t* data, size_t size)
    {
        PartitonList nList;
        struct Partition myPaty;
        (void)memset_s(&myPaty, sizeof(struct Partition), 0, sizeof(struct Partition));

        if (size < FSTAB_NAME_LENGTH) {  /* fstable name length */
            InitEmmcPartition(myPaty, reinterpret_cast<const char*>(data), 0, size);
            nList.push_back(&myPaty);
            DoPartitions(nList);
        }

        return 0;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzDoPartitions(data, size);
    return 0;
}

