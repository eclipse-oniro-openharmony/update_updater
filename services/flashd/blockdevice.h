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

#ifndef FLASHING_BLOCKDEVICE_H
#define FLASHING_BLOCKDEVICE_H
#include <cstdlib>
#include <memory>
#include "flash_utils.h"

namespace flashd {
enum class DeviceType {
    DEVICE_UNKNOWN = 0,
    DEVICE_SCSI = 1,
    DEVICE_EMMC = 2,
};

class BlockDevice {
public:
    BlockDevice(DeviceType type, const std::string &devPath) : devPath_(devPath), type_(type) {}
    ~BlockDevice() {};

    int Load();
    const std::string GetDeviceName();
    DeviceType GetDeviceType()
    {
        return type_;
    }
private:
    std::string ReadDeviceSysInfo(const std::string &type);

    std::string devPath_;
    size_t devSize_ = 0;
    size_t sectorSize_ = 0;         // logical sector size
    size_t physSectorSize_ = 0;     // physical sector size
    DeviceType type_ = DeviceType::DEVICE_UNKNOWN;
};
using BlockDevicePtr = BlockDevice *;
}
#endif // FLASHING_BLOCKDEVICE_H