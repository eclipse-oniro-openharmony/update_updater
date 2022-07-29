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

#include "partition.h"

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <vector>
#include "flashd_define.h"
#include "flashd_utils.h"
#include "fs_manager/mount.h"
#include "utils.h"

namespace Flashd {
int Partition::DoFlash(const uint8_t *buffer, int bufferSize) const
{
    if (buffer == nullptr || bufferSize <= 0 || writer_ == nullptr) {
        FLASHD_LOGE("buffer is null or writer_ is null or bufferSize is invaild");
        return FLASHING_ARG_INVALID;
    }

    if (auto ret = writer_->Write(devName_, buffer, bufferSize); ret != 0) {
        FLASHD_LOGE("Write fail, ret = %d", ret);
        return ret;
    }
    return 0;
}

int Partition::DoErase() const
{
    auto fd = open(GetPartitionRealPath(devName_).c_str(), O_RDWR);
    if (fd < 0) {
        FLASHD_LOGE("open partition %s fail, error = %d", devName_.c_str(), errno);
        return FLASHING_OPEN_PART_ERROR;
    }

    uint64_t size = GetBlockDeviceSize(fd);
    uint64_t range[2] = { 0, size };
    if (ioctl(fd, BLKSECDISCARD, &range) >= 0) {
        close(fd);
        return 0;
    }

    range[0] = 0;
    range[1] = size;
    if (ioctl(fd, BLKDISCARD, &range) < 0) {
        close(fd);
        FLASHD_LOGE("BLKDISCARD fail");
        return FLASHING_NOPERMISSION;
    }
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    if (!Updater::Utils::WriteFully(fd, buffer.data(), buffer.size())) {
        close(fd);
        FLASHD_LOGE("WriteFully fail");
        return FLASHING_PART_WRITE_ERROR;
    }
    fsync(fd);
    close(fd);
    return 0;
}

int Partition::DoFormat() const
{
    auto name = "/" + devName_;
    if (auto ret = Updater::FormatPartition(name); ret != 0) {
        FLASHD_LOGE("FormatPartition fail, ret = %d, ret");
        return ret;
    }

    if (auto ret = Updater::MountForPath(name); ret != 0) {
        FLASHD_LOGE("MountForPath fail, ret = %d, ret");
        return ret;
    }
    return 0;
}

uint64_t Partition::GetBlockDeviceSize(int fd) const
{
    uint64_t size = 0;
    return (ioctl(fd, BLKGETSIZE64, &size) == 0) ? size : 0;
}
}
