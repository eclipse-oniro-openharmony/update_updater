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
#ifdef _WIN32
#include <windows.h>
#else
#include <climits>
#include <sys/mman.h>
#endif
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"

namespace Hpackage {
#ifdef _WIN32
#undef ERROR
#endif

#ifdef __APPLE__
#define MAP_POPULATE 0x08000
#endif

constexpr int32_t MIN_YEAR = 80;
constexpr uint32_t TM_YEAR_BITS = 9;
constexpr uint32_t TM_MON_BITS = 5;
constexpr uint32_t TM_MIN_BITS = 5;
constexpr uint32_t TM_HOUR_BITS = 11;
constexpr uint32_t BYTE_SIZE = 8;
constexpr uint32_t MAX_MEM_SIZE = 1 << 29;
constexpr uint32_t SECOND_BUFFER = 2;
constexpr uint32_t THIRD_BUFFER = 3;
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
    char realPath[PATH_MAX] = { 0 };
#ifdef _WIN32
    if (_fullpath(realPath, fileName.c_str(), PATH_MAX) == nullptr) {
#else
    if (realpath(fileName.c_str(), realPath) == nullptr) {
#endif
        PKG_LOGE("realPath is null");
        return 0;
    }
    FILE *fp = fopen(realPath, "r");
    if (fp == nullptr) {
        PKG_LOGE("Invalid file %s", fileName.c_str());
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) < 0) {
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
#ifdef __WIN32
        mkdir(path.c_str());
#else
        mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
    }
    // If the path writable
    int ret = access(path.c_str(), R_OK | W_OK);
    if (ret == -1) {
        PKG_LOGE("file %s no permission ", fileName.c_str());
        return PKG_NONE_PERMISSION;
    }
    return PKG_SUCCESS;
}

uint8_t *MapMemory(const std::string &fileName, size_t size)
{
    if (size > MAX_MEM_SIZE) {
        PKG_LOGE("Size bigger for alloc memory");
        return nullptr;
    }
    void *mappedData = nullptr;
    // Map the file to memory
    mappedData = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE | MAP_ANON, -1, 0);
    if (mappedData == MAP_FAILED) {
        PKG_LOGE("Failed to mmap file %s ", fileName.c_str());
        return nullptr;
    }
    return static_cast<uint8_t *>(mappedData);
}

void ReleaseMemory(uint8_t *memMap, size_t size)
{
    if (size <= 0 || memMap == nullptr) {
        PKG_LOGE("Size must > 0");
        return;
    }
    // Flush memory and release memory.
    msync(static_cast<void *>(memMap), size, MS_ASYNC);
    munmap(memMap, size);
}

std::string GetCurrPath()
{
    std::string path;
    char *buffer = getcwd(nullptr, 0);
    if (buffer == nullptr) {
        PKG_LOGE("getcwd error");
        return "./";
    }
    path.assign(buffer);
    free(buffer);
    return path + "/";
}

void ExtraTimeAndDate(time_t when, uint16_t &date, uint16_t &time)
{
    when = static_cast<time_t>((static_cast<unsigned long>(when) + 1) & (~1));
    struct tm nowTime {};
#ifdef __WIN32
    localtime_s(&nowTime, &when);
#else
    localtime_r(&when, &nowTime);
#endif
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
    size_t offset = 0;
    uint32_t value32 = buff[0];
    offset += BYTE_SIZE;
    value32 += static_cast<uint32_t>(static_cast<uint32_t>(buff[1]) << offset);
    offset +=  BYTE_SIZE;
    value32 += static_cast<uint32_t>(static_cast<uint32_t>(buff[SECOND_BUFFER]) << offset);
    offset += BYTE_SIZE;
    value32 += static_cast<uint32_t>(static_cast<uint32_t>(buff[THIRD_BUFFER]) << offset);
    return value32;
}

uint64_t ReadLE64(const uint8_t *buff)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return 0;
    }
    uint32_t low = ReadLE32(buff);
    uint32_t high = ReadLE32(buff + sizeof(uint32_t));
    uint64_t value = ((static_cast<uint64_t>(high)) << (BYTE_SIZE * sizeof(uint32_t))) | low;
    return value;
}

void WriteLE32(uint8_t *buff, uint32_t value)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return;
    }
    size_t offset = 0;
    buff[0] = static_cast<uint8_t>(value);
    offset += BYTE_SIZE;
    buff[1] = static_cast<uint8_t>(value >> offset);
    offset += BYTE_SIZE;
    buff[SECOND_BUFFER] = static_cast<uint8_t>(value >> offset);
    offset += BYTE_SIZE;
    buff[THIRD_BUFFER] = static_cast<uint8_t>(value >> offset);
}

uint16_t ReadLE16(const uint8_t *buff)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return 0;
    }
    uint16_t value16 = buff[0];
    value16 += static_cast<uint16_t>(buff[1] << BYTE_SIZE);
    return value16;
}

void WriteLE16(uint8_t *buff, uint16_t value)
{
    if (buff == nullptr) {
        PKG_LOGE("buff is null");
        return;
    }
    buff[0] = static_cast<uint8_t>(value);
    buff[1] = static_cast<uint8_t>(value >> BYTE_SIZE);
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

#ifdef _WIN32
void *mmap([[maybe_unused]] void *addr, [[maybe_unused]] size_t length,
    [[maybe_unused]] int prot, [[maybe_unused]] int flags, int fd, [[maybe_unused]] size_t offset)
{
    HANDLE FileHandle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (FileHandle == INVALID_HANDLE_VALUE) {
        return MAP_FAILED;
    }
    HANDLE FileMappingHandle = ::CreateFileMappingW(FileHandle, 0, PAGE_READONLY, 0, 0, 0);
    if (FileMappingHandle == nullptr) {
        PKG_LOGE("CreateFileMappingW Failed");
        return MAP_FAILED;
    }
    void *mapAddr = ::MapViewOfFile(FileMappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (mapAddr == nullptr) {
        PKG_LOGE("MapViewOfFile Failed");
        ::CloseHandle(FileMappingHandle);
        return MAP_FAILED;
    }
    ::CloseHandle(FileMappingHandle);
    return mapAddr;
}

int msync(void *addr, size_t len, [[maybe_unused]] int flags)
{
    return FlushViewOfFile(addr, len);
}

int munmap(void *addr, [[maybe_unused]] size_t len)
{
    return !UnmapViewOfFile(addr);
}
#endif
