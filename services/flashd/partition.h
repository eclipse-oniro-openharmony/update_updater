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

#ifndef FLASHING_PARTITION_H
#define FLASHING_PARTITION_H
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "blockdevice.h"
#include "flash_utils.h"

namespace flashd {
enum class PartitionType {
    NORMAL,
    LOGICAL,
    EXTENDED,
};

class Partition {
public:
    Partition(const std::string &name, const std::string &path, BlockDevicePtr device, FlashingPtr flash)
        : partPath_(path), devName_(name), device_(device), flash_(flash) {}
    ~Partition();

    int Load();
    int DoFlash(const std::string &fileName);
    int DoFormat(const std::string &fsType);
    int DoErase();
    int DoResize(uint32_t blocks);

    bool IsOnlyErase() const;
    const std::string GetPartitionName() const;
    PartitionType GetPartitionType() const
    {
        return type_;
    }
    const std::string GetPartitionPath() const
    {
        return partPath_;
    }
private:
    std::string ReadPartitionSysInfo(const std::string &partition,
        const std::string &type, std::vector<std::string> &table);
    int Open();
    int WriteRowData(int inputFd, size_t fileSize, std::vector<uint8_t> &buffer, size_t dataSize);
    int IsBlockDevice(int fd) const;
    int GetMountInfo();
    int DoUmount();
    int DoMount();
    int BuildCommandParam(const std::string &fsType, std::vector<std::string> &formatCmds) const;
    uint64_t GetBlockDeviceSize(int fd) const;
    uint32_t GetMountFlags(const std::string &mountFlagsStr, std::string &data) const;

    int fd_ = -1;
    int partNumber_ { 0 }; // Partition number.
    std::string partPath_ {};
    std::string devName_ {}; // partition DEVNAME
    std::string partName_ {}; // partition PARTN
    PartitionType type_ { PartitionType::NORMAL };
    std::string fsType_ {}; // File system type, ext4, f2fs etc.
    std::string mountFlags_ {}; // flags:rw,relatime,fmask=0077,dmask=0077
    std::string mountPoint_ {};
    BlockDevicePtr device_ = nullptr;
    FlashingPtr flash_ = nullptr;
};
using PartitionPtr = Partition *;
}
#endif // FLASHING_PARTITION_H