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
#include "blockdevice.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "flash_service.h"

namespace flashd {
int BlockDevice::Load()
{
    struct stat devStat {};
    int ret = stat(devPath_.c_str(), &devStat);
    FLASHING_CHECK(ret != -1, return -1, "Failed to state %s", devPath_.c_str());

    const std::string devName = GetDeviceName();
    std::string size = ReadDeviceSysInfo("size");
    FLASHING_CHECK(!size.empty(), return 0, "Failed to read dev size");
    devSize_ = atol(size.c_str());
    sectorSize_ = SECTOR_SIZE_DEFAULT;
    physSectorSize_ = SECTOR_SIZE_DEFAULT;
    return 0;
}

const std::string BlockDevice::GetDeviceName()
{
    std::string::size_type pos = devPath_.find_last_of('/') + 1;
    return devPath_.substr(pos, devPath_.size() - pos);
}

std::string BlockDevice::ReadDeviceSysInfo(const std::string &type)
{
    std::vector<char> buffer(DEVICE_PATH_SIZE, 0);
    int ret = snprintf_s(buffer.data(), DEVICE_PATH_SIZE, DEVICE_PATH_SIZE - 1,
        "/sys/block/%s/%s", GetDeviceName().c_str(), type.c_str());
    FLASHING_CHECK(ret != -1, return "", "Failed to snprintf_s %s", devPath_.c_str());
    std::vector<std::string> table;
    return FlashService::ReadSysInfo(buffer.data(), type, table);
}
} // namespace flashd