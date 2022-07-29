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
#include "pkg_utils.h"
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "securec.h"
#include "utils.h"

namespace Hpackage {
constexpr int32_t MIN_YEAR = 80;
constexpr uint32_t TM_YEAR_BITS = 9;
constexpr uint32_t TM_MON_BITS = 5;
constexpr uint32_t TM_MIN_BITS = 5;
constexpr uint32_t TM_HOUR_BITS = 11;
constexpr uint32_t MAX_MEM_SIZE = 1 << 29;
constexpr uint8_t SHIFT_RIGHT_FOUR_BITS = 4;

using namespace Updater::Utils;

std::string GetFilePath(const std::string &fileName)
{
    std::size_t pos = fileName.find_last_of('/');
    if (pos == std::string::npos) {
        pos = fileName.find_last_of('\\');
    }
    return fileName.substr(0, pos + 1);
}

size_t GetFileSize(const std::string &fileName)
{
    char *realPath = realpath(fileName.c_str(), NULL);
    PKG_CHECK(realPath != nullptr, return 0, "realPath is null");
    FILE *fp = fopen(realPath, "r");
    free(realPath);
    PKG_CHECK(fp != nullptr, return 0, "Invalid file %s", fileName.c_str());

    if (fseek(fp, 0, SEEK_END) != 0) {
        PKG_LOGE("return value of fseek < 0");
        fclose(fp);
        return 0;
    }
    long size = ftell(fp);
    if (size < 0) {
        PKG_LOGE("return value of ftell < 0");
        fclose(fp);
        return 0;
    }
    fclose(fp);
    // return file size in bytes
    return static_cast<size_t>(size);
}

std::string GetName(const std::string &filePath)
{
    return filePath.substr(filePath.find_last_of("/") + 1);
}

int32_t CheckFile(const std::string &fileName)
{
    // Check if the directory of @fileName is exist or has write permission
    // If not, Create the directory first.
    std::string path = GetFilePath(fileName);
    if (path.empty()) {
        return PKG_SUCCESS;
    }
    if (access(path.c_str(), F_OK) == -1) {
        mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    // If the path writable
    int ret = access(path.c_str(), R_OK | W_OK);
    PKG_CHECK(ret != -1, return PKG_NONE_PERMISSION, "file %s no permission ", fileName.c_str());
    return PKG_SUCCESS;
}

uint8_t *MapMemory(const std::string &fileName, size_t size)
{
    PKG_CHECK(size <= MAX_MEM_SIZE, return nullptr, "Size bigger for alloc memory");
    void *mappedData = nullptr;
    // Map the file to memory
    mappedData = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE | MAP_ANON, -1, 0);
    PKG_CHECK(mappedData != MAP_FAILED, return nullptr, "Failed to mmap file %s ", fileName.c_str());
    return static_cast<uint8_t *>(mappedData);
}

void ReleaseMemory(uint8_t *memMap, size_t size)
{
    PKG_CHECK(size > 0 && memMap != nullptr, return, "Size must > 0");
    // Flush memory and release memory.
    msync(static_cast<void *>(memMap), size, MS_ASYNC);
    munmap(memMap, size);
}

std::string GetCurrPath()
{
    std::string path;
    char *buffer = getcwd(nullptr, 0);
    PKG_CHECK(buffer != nullptr, return "./", "getcwd error");
    path.assign(buffer);
    free(buffer);
    return path + "/";
}

void ExtraTimeAndDate(time_t when, uint16_t &date, uint16_t &time)
{
    when = static_cast<time_t>((static_cast<unsigned long>(when) + 1) & (~1));
    struct tm nowTime {};
    localtime_r(&when, &nowTime);
    int year = nowTime.tm_year;
    if (year < MIN_YEAR) {
        year = MIN_YEAR;
    }
    date = static_cast<uint16_t>(static_cast<uint16_t>(year - MIN_YEAR) << TM_YEAR_BITS);
    date |= static_cast<uint16_t>(static_cast<uint16_t>(nowTime.tm_mon + 1) << TM_MON_BITS);
    date |= static_cast<uint16_t>(nowTime.tm_mday);
    time = static_cast<uint16_t>(static_cast<uint16_t>(nowTime.tm_hour) << TM_HOUR_BITS);
    time |= static_cast<uint16_t>(static_cast<uint16_t>(nowTime.tm_min) << TM_MIN_BITS);
    time |= static_cast<uint16_t>(static_cast<uint16_t>(nowTime.tm_sec) >> 1);
}

uint32_t ReadLE32(const uint8_t *buff)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return 0;
    }
    return le32toh(*(reinterpret_cast<const uint32_t *>(buff)));    
}

uint64_t ReadLE64(const uint8_t *buff)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return 0;
    }
    return le64toh(*(reinterpret_cast<const uint64_t *>(buff)));   
}

void WriteLE32(uint8_t *buff, uint32_t value)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return;
    }
    *(reinterpret_cast<uint32_t *>(buff)) = htole32(value);
}

uint16_t ReadLE16(const uint8_t *buff)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return 0;
    }
    return le16toh(*(reinterpret_cast<const uint16_t *>(buff)));  
}

void WriteLE16(uint8_t *buff, uint16_t value)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return;
    }
    *(reinterpret_cast<uint16_t *>(buff)) = htole16(value);
}

std::string ConvertShaHex(const std::vector<uint8_t> &shaDigest)
{
    const std::string hexChars = "0123456789abcdef";
    std::string haxSha256 = "";
    unsigned int c;
    for (size_t i = 0; i < shaDigest.size(); ++i) {
        auto d = shaDigest[i];
        c = (d >> SHIFT_RIGHT_FOUR_BITS) & 0xf;     // last 4 bits
        haxSha256.push_back(hexChars[c]);
        haxSha256.push_back(hexChars[d & 0xf]);
    }
    return haxSha256;
}
} // namespace Hpackage
