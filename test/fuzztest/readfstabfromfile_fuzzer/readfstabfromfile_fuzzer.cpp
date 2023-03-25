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

#include "readfstabfromfile_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "fs_manager/fs_manager.h"
#include "log/log.h"

using namespace Updater;

namespace OHOS {
    bool FuzzReadFstabFromFile(const uint8_t* data, size_t size)
    {
        FILE *pFile;
        Fstab *fstab = NULL;
        const std::string fstabFile = "ReadFstabFromFile.txt";

        pFile = fopen("ReadFstabFromFile.txt", "w+");
        if (pFile == nullptr) {
            LOG(ERROR) << "[fuzz]open file failed";
            return false;
        }

        (void)fwrite(data, 1, size, pFile);
        (void)fclose(pFile);
        fstab = ReadFstabFromFile(fstabFile.c_str(), false);
        (void)remove("ReadFstabFromFile.txt");
        ReleaseFstab(fstab);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzReadFstabFromFile(data, size);
    return 0;
}
