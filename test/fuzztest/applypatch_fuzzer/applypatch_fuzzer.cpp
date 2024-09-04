#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include "log/log.h"

using namespace Updater;
namespace OHOS {
    bool WriteDataToFile(const char* data, size_t size, const char* filePath)
    {
        std::ofstream ofs(filePath, std::ios::app | std::ios::binary);
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
        const std::string filePath = "/data/fuzz/test/MountForPath_fuzzer.fstable";
        ApplyPatch(filePath, filePath, std::string(reinterpret_cast<const char*>(data), size));
        ApplyPatch(filePath, std::string(reinterpret_cast<const char*>(data), size), filePath);
        ApplyPatch(std::string(reinterpret_cast<const char*>(data), size), filePath, filePath);
        bool ret = true;
        const int magicNumSize = 4;
        const char* bspatchPath = "/data/applyPatchfuzzBspatch";
        const char* imgpatchPath = "/data/applyPatchfuzzImgpatch";
        const char* oldFilePath = "/data/applyPatchfuzzOldFile";
        const char* newFilePath = "/data/applyPatchfuzzNewFile";
        ret &= WriteDataToFile("BSDIFF40", magicNumSize, bspatchPath);
        ret &= WriteDataToFile(reinterpret_cast<const char*>(data), size, bspatchPath);
        ret &= WriteDataToFile("PKGDIFF0", magicNumSize, imgpatchPath);
        ret &= WriteDataToFile(reinterpret_cast<const char*>(data), size, imgpatchPath);
        ret &= WriteDataToFile(reinterpret_cast<const char*>(data), size, oldFilePath);
        if (!ret) {
            LOG(ERROR) << "create file failed";
            return;
        }
        ApplyPatch(bspatchPath, oldFilePath, newFilePath);
        ApplyPatch(imgpatchPath, oldFilePath, newFilePath);
        if (remove(bspatchPath) != 0 || remove(imgpatchPath) != 0  || remove(oldFilePath)) {
            LOG(WARNING) << "Failed to delete file";
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

