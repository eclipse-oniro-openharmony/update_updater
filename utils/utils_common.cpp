/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "utils_common.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <linux/reboot.h>
#include <string>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "log/log.h"

namespace Updater {
namespace Utils {
constexpr int USECONDS_PER_SECONDS = 1000000; // 1s = 1000000us
constexpr int NANOSECS_PER_USECONDS = 1000; // 1us = 1000ns

bool PathToRealPath(const std::string &path, std::string &realPath)
{
    if (path.empty()) {
        LOG(ERROR) << "path is empty!";
        return false;
    }

    if ((path.length() >= PATH_MAX)) {
        LOG(ERROR) << "path len is error, the len is: " << path.length();
        return false;
    }

    char tmpPath[PATH_MAX] = {0};
    if (realpath(path.c_str(), tmpPath) == nullptr) {
        LOG(ERROR) << "path to realpath error " << path;
        return false;
    }

    realPath = tmpPath;
    return true;
}

void UsSleep(int usec)
{
    auto seconds = usec / USECONDS_PER_SECONDS;
    long nanoSeconds = static_cast<long>(usec) % USECONDS_PER_SECONDS * NANOSECS_PER_USECONDS;
    struct timespec ts = { static_cast<time_t>(seconds), nanoSeconds };
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
    }
}

bool IsUpdaterMode()
{
    struct stat st {};
    if (stat("/bin/updater", &st) == 0 && S_ISREG(st.st_mode)) {
        LOG(INFO) << "updater mode";
        return true;
    }
    LOG(INFO) << "normal mode";
    return false;
}
} // Utils
} // updater
