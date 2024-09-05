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
#include <fstream>
#include <cstdio>
#include "log/log.h"
#include <fuzzer/FuzzedDataProvider.h>

using namespace Updater;
namespace OHOS {
    bool WriteDataToFile(const char* data, size_t size, const char* filePath, bool isAppend = false)
    {
        if (isAppend) {
            std::ofstream ofs(filePath, std::ios::app | std::ios::binary);
        } else {
            std::ofstream ofs(filePath, std::ios::ate | std::ios::binary);
        }
        if (!ofs.is_open()) {
            LOG(ERROR) << "open " << filePath << " failed";
            return false;
        }
        ofs.write(data, size);
        ofs.close();
        return true;
    }

    void FuzzApplyPatch(const uint8_t* data, size_t size)
    {
        FuzzedDataProvider fdp(data, size);
        
        const int magicNumSize = 4;
        const char* bspatchPath = "/data/applyPatchfuzzBspatch";
        const char* imgpatchPath = "/data/applyPatchfuzzImgpatch";
        const char* oldFilePath = "/data/applyPatchfuzzOldFile";
        const char* newFilePath = "/data/applyPatchfuzzNewFile";
        bool isPkgFormat = false;
        bool ret = WriteDataToFile(reinterpret_cast<const char*>(data), size, oldFilePath);
        isPkgFormat = fdp.ConsumeBool();
        if (isPkgFormat) {
            ret &= WriteDataToFile("BSDIFF40", magicNumSize, bspatchPath);
            ret &= WriteDataToFile(reinterpret_cast<const char*>(data), size, bspatchPath, true);
            ApplyPatch(bspatchPath, oldFilePath, newFilePath);
        }else{
            ret &= WriteDataToFile("PKGDIFF0", magicNumSize, imgpatchPath);
            ret &= WriteDataToFile(reinterpret_cast<const char*>(data), size, imgpatchPath, true);
            ApplyPatch(imgpatchPath, oldFilePath, newFilePath);
        }
        if (!ret) {
            LOG(ERROR) << "create file failed";
        }
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzApplyPatch(data, size);
    return 0;
}
