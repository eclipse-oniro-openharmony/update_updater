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

#ifndef UPDATER_EMMC_PTABLE_H
#define UPDATER_EMMC_PTABLE_H
#include <iostream>
#include "ptable.h"

namespace Updater {
class EmmcPtable : public Ptable {
public:
    EmmcPtable() = default;
    DISALLOW_COPY_MOVE(EmmcPtable);
    ~EmmcPtable() override {}

    uint32_t GetDeviceLunNum();
    bool ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize) override;
    bool LoadPtableFromDevice() override;
    bool WritePartitionTable() override;

private:
    static constexpr uint32_t  GPT_PARTITION_START_LBA_OFFSET = 32;
    static constexpr uint32_t  GPT_PARTITION_END_LBA_OFFSET = 8;
    static constexpr uint32_t  GPT_PARTITION_NAME_OFFSET = 56;
    static constexpr uint32_t  GPT_PARTITION_TYPE_GUID_OFFSET = 0;
    static constexpr uint32_t  GPT_DISP_NAME_LEN = 32;
    static constexpr uint32_t GPT_PARTITION_SIZE = 128 * 1024;
    static constexpr uint32_t LBA_LENGTH = 512;
    static constexpr uint32_t GPT_PARTITION_INFO_LENGTH = 128;
    static constexpr uint32_t PROTECTIVE_MBR_SIZE = 512;
    static constexpr uint32_t MIN_EMMC_WRITE_SIZE = 4096;
    static constexpr uint32_t EMMC_BLOCK_SIZE = 512;

    struct EmmcPartitionDataInfo {
        bool isGptVaild = false;
        uint32_t writeDataLen = 0; // data len written to emmc
        uint8_t data[GPT_PARTITION_SIZE] = {0}; // ptable image data
    };

    struct EmmcPartitionDataInfo emmcPtnDataInfo_;

    bool EmmcReadGpt(uint8_t *ptableData);
    bool UpdateCommInitializeGptPartition(uint8_t *gptImage, const uint32_t len);
    bool ReadEmmcGptImageToRam();
    uint64_t GetDeviceCapacity();
    void EmmcPatchGptHeader(EmmcPartitionDataInfo &ptnDataInfo, const uint32_t blockSize);
    bool ParseGptHeaderByEmmc(uint8_t *gptImage, const uint32_t len);
};
} // namespace Updater
#endif // UPDATER_EMMC_PTABLE_H