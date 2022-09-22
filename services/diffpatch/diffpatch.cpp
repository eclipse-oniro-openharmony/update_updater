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

#include "diffpatch.h"
#ifndef __WIN32
#include <climits>
#include <sys/mman.h>
#endif
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "openssl/sha.h"

namespace UpdatePatch {
int32_t WriteDataToFile(const std::string &fileName, const std::vector<uint8_t> &data, size_t dataSize)
{
    std::ofstream patchFile(fileName, std::ios::out | std::ios::binary);
    if (!patchFile) {
        PATCH_LOGE("Failed to open %s", fileName.c_str());
        return -1;
    }
    patchFile.write(reinterpret_cast<const char*>(data.data()), dataSize);
    patchFile.close();
    return PATCH_SUCCESS;
}

int32_t PatchMapFile(const std::string &fileName, MemMapInfo &info)
{
    char realPath[PATH_MAX] = { 0 };
#ifdef _WIN32
    if (_fullpath(realPath, fileName.c_str(), PATH_MAX) == nullptr) {
#else
    if (realpath(fileName.c_str(), realPath) == nullptr) {
#endif
        PATCH_LOGE("Failed to get realpath %s", fileName.c_str());
        return -1;
    }
    info.fd = open(realPath, O_RDONLY);
    if (info.fd < 0) {
        PATCH_LOGE("Failed to open file %s", fileName.c_str());
        return -1;
    }
    struct stat st {};
    int32_t ret = fstat(info.fd, &st);
    if (ret < 0) {
        PATCH_LOGE("Failed to fstat");
        return ret;
    }
    if (S_ISBLK(st.st_mode)) {
        st.st_size = lseek(info.fd, 0, SEEK_END);
        lseek(info.fd, 0, SEEK_SET);
    }

    info.memory = static_cast<uint8_t*>(mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, info.fd, 0));
    if (info.memory == nullptr) {
        PATCH_LOGE("Failed to memory map");
        return -1;
    }
    info.length = static_cast<size_t>(st.st_size);
    return PATCH_SUCCESS;
}

std::string GeneraterBufferHash(const BlockBuffer &buffer)
{
    SHA256_CTX sha256Ctx;
    SHA256_Init(&sha256Ctx);
    SHA256_Update(&sha256Ctx, buffer.buffer, buffer.length);
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    SHA256_Final(digest.data(), &sha256Ctx);
    return ConvertSha256Hex({
            digest.data(), SHA256_DIGEST_LENGTH
        });
}

std::string ConvertSha256Hex(const BlockBuffer &buffer)
{
    const std::string hexChars = "0123456789abcdef";
    std::string haxSha256 = "";
    unsigned int c;
    for (size_t i = 0; i < buffer.length; ++i) {
        auto d = buffer.buffer[i];
        c = (d >> SHIFT_RIGHT_FOUR_BITS) & 0xf;     // last 4 bits
        haxSha256.push_back(hexChars[c]);
        haxSha256.push_back(hexChars[d & 0xf]);
    }
    return haxSha256;
}
} // namespace UpdatePatch
