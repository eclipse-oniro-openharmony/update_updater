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
#ifndef UPDATER_UTILS_H
#define UPDATER_UTILS_H

#include <cerrno>
#include <string>
#include <vector>

namespace updater {
namespace utils {
constexpr int N_BIN = 2;
constexpr int N_OCT = 8;
constexpr int N_DEC = 10;
constexpr int N_HEX = 16;
constexpr int O_USER_GROUP_ID = 1000;
constexpr int ARGC_TWO_NUMS = 2;
template<class T>
T String2Int(const std::string &str, int base = N_HEX)
{
    char *end = nullptr;
    if (str.empty()) {
        errno = EINVAL;
        return 0;
    }
    if (((str[0] == '0') && (str[1] == 'x')) || (str[1] == 'X')) {
        base = N_HEX;
    }
    T result = strtoull(str.c_str(), &end, base);
    return result;
}
int32_t DeleteFile(const std::string& filename);
int MkdirRecursive(const std::string &pathName, mode_t mode);
int64_t GetFilesFromDirectory(const std::string &path, std::vector<std::string> &files, bool isRecursive = false);
std::vector<std::string> SplitString(const std::string &str, const std::string del = " \t");
std::string Trim(const std::string &str);
std::string ConvertSha256Hex(const uint8_t* shaDigest, size_t length);
void DoReboot(const std::string& rebootTarget);
std::string GetCertName();
bool WriteFully(int fd, const void *data, size_t size);
bool ReadFully(int fd, void* data, size_t size);
bool ReadFileToString(int fd, std::string &content);
bool WriteStringToFile(int fd, const std::string& content);
std::string GetLocalBoardId();
bool CopyUpdaterLogs(const std::string &sLog, const std::string &dLog);
void CompressLogs(const std::string &name);
} // utils
} // updater
#endif // UPDATER_UTILS_H
