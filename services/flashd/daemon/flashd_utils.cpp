/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "flashd_utils.h"

#include <unistd.h>

#include "securec.h"

namespace Flashd {
constexpr const char *PREFIX_PARTITION_NODE = "/dev/block/by-name/";

std::string GetRealPath(const std::string &path)
{
    char realPath[PATH_MAX + 1] = {0};
    auto ret = realpath(path.c_str(), realPath);
    return (ret == nullptr) ? "" : ret;
}

std::string GetPartitionRealPath(const std::string &name)
{
    return GetRealPath(PREFIX_PARTITION_NODE + name);
}

std::string GetPathRoot(const std::string &path)
{
    auto pos = path.find_first_of('/', 1);
    return path.substr(0, pos);
}

std::string GetFileName(const std::string &path)
{
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        pos = path.find_last_of('\\');
        if (pos == std::string::npos) {
            return "";
        }
    }
    return path.substr(pos + 1);
}

std::vector<std::string> Split(const std::string &src, const std::vector<std::string> &filter)
{
    std::vector<std::string> result;
    if (src.empty()) {
        return result;
    }

    const auto len = src.size() + 1;
    auto buffer = std::vector<char>(len, 0);
    buffer.assign(src.begin(), src.end());

    const char delimit[] = " ";
    char *nextToken = nullptr;
    auto token = strtok_s(buffer.data(), delimit, &nextToken);
    while (token != nullptr) {
        if (find(filter.cbegin(), filter.cend(), token) == filter.cend()) {
            result.push_back(token);
        }
        token = strtok_s(nullptr, delimit, &nextToken);
    }
    return result;
}

void SafeCloseFile(int &fd)
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}
} // namespace Flashd