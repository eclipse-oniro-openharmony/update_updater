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
 
#include "partition_utils.h"
 
#include <cstdio>
#include <vector>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
 
#include "log/log.h"
#include "utils.h"
 
namespace Updater {
namespace Utils {
uint64_t PartitionUtils::GetBlockDeviceSize(int fd) const
{
    uint64_t size = 0;
    return (ioctl(fd, BLKGETSIZE64, &size) == 0) ? size : 0;
}
 
int PartitionUtils::WipeBlockPartition() const
{
    auto fd = open(Updater::Utils::GetPartitionRealPath(devName_).c_str(), O_RDWR);
    if (fd < 0) {
        LOG(ERROR) << "open partition "<<  devName_.c_str() << " fail, error = " << errno;
        return -1;
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
        LOG(ERROR) << "BLKDISCARD fail";
        return -1;
    }
    std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
    if (!Updater::Utils::WriteFully(fd, buffer.data(), buffer.size())) {
        close(fd);
        LOG(ERROR) << "wipe block partition write fully fail";
        return -1;
    }
    fsync(fd);
    close(fd);
    return 0;
}
}
}