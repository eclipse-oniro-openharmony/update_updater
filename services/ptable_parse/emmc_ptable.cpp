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

#include "emmc_ptable.h"

#include <algorithm>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"

namespace Updater {

uint64_t EmmcPtable::GetDeviceCapacity()
{
    uint64_t capacity = 0;
    std::string capacityPath = std::string(MMC_SIZE_FILE);
    GetCapacity(capacityPath, capacity);
    return capacity;
}

void EmmcPtable::EmmcPatchGptHeader(EmmcPartitionDataInfo &ptnDataInfo, const uint32_t blockSize)
{
    // mbr len + gptHeader len = 2 blockSize
    if (blockSize == 0 || ptnDataInfo.writeDataLen < 2 * blockSize) {
        LOG(ERROR) << "invaild argument";
        return;
    }

    uint64_t devDensity = GetDeviceCapacity();

    uint64_t devBlockSize = EMMC_BLOCK_SIZE;
    uint64_t cardSizeSector = devDensity / devBlockSize;

    // Patching primary header
    uint8_t *primaryGptHeader = ptnDataInfo.data + blockSize;
    PUT_LONG_LONG(primaryGptHeader + BACKUP_HEADER_OFFSET, (cardSizeSector - 1));
    PUT_LONG_LONG(primaryGptHeader + LAST_USABLE_LBA_OFFSET, (cardSizeSector - 1));
    // Find last partition
    uint32_t totalPart = 0;
    while (((GPT_PARTITION_SIZE - blockSize - blockSize) > totalPart * PARTITION_ENTRY_SIZE) &&
        (*(primaryGptHeader + blockSize + totalPart * PARTITION_ENTRY_SIZE) != 0)) {
        totalPart++;
    }
    if (totalPart == 0) {
        LOG(ERROR) << "no partition exist";
        return;
    }
    // Patching last partition
    uint8_t *lastPartOffset = primaryGptHeader + blockSize + (totalPart - 1) * PARTITION_ENTRY_SIZE;
    uint64_t lastLba = GET_LLWORD_FROM_BYTE(lastPartOffset + PARTITION_ENTRY_LAST_LBA);
    uint64_t firstLba = GET_LLWORD_FROM_BYTE(lastPartOffset + FIRST_LBA_OFFSET);
    // General algorithm : calculate partition size by lba
    uint64_t partitionSize = (lastLba - firstLba + 1) * MIN_EMMC_WRITE_SIZE;
    std::string partitionName;
    uint8_t *nameOffset = lastPartOffset + GPT_PARTITION_NAME_OFFSET;
    // 2 bytes for 1 charactor of partition name
    ParsePartitionName(nameOffset, MAX_GPT_NAME_SIZE, partitionName, MAX_GPT_NAME_SIZE / 2);
    if (partitionName == USERDATA_PARTITION || (totalPart == 1 && partitionSize == 0)) {
        // patch userdata or only one partition
        PUT_LONG_LONG(lastPartOffset + PARTITION_ENTRY_LAST_LBA, (cardSizeSector - 1));
        LOG(INFO) << "partitionSize=" << (cardSizeSector - 1) << ", partition_name:" << partitionName;
    }

    // Updating CRC of the Partition entry array in both headers
    uint32_t partCount = GET_LWORD_FROM_BYTE(primaryGptHeader + PARTITION_COUNT_OFFSET);
    uint32_t entrySize = GET_LWORD_FROM_BYTE(primaryGptHeader + PENTRY_SIZE_OFFSET);
    // mbr len + gptHeader len = 2 blockSize
    uint32_t crcValue = CalculateCrc32(ptnDataInfo.data + (blockSize * 2), partCount * entrySize);
    PUT_LONG(primaryGptHeader + PARTITION_CRC_OFFSET, crcValue);
    // Clearing CRC fields to calculate
    PUT_LONG(primaryGptHeader + HEADER_CRC_OFFSET, 0);
    crcValue = CalculateCrc32(primaryGptHeader, GPT_CRC_LEN);
    PUT_LONG(primaryGptHeader + HEADER_CRC_OFFSET, crcValue);
    return;
}

bool EmmcPtable::WritePartitionTable()
{
    if (partitionInfo_.empty()) {
        LOG(ERROR) << "emmcPtnDataInfo_ is empty, write failed!";
        return false;
    }

    if (!emmcPtnDataInfo_.isGptVaild) {
        LOG(WARNING) <<  "invaild ptable, no need to update";
        return false;
    }

    std::string emmcNode = std::string(MMC_BLOCK_DEV_NAME);
    if (!WriteBufferToPath(emmcNode, 0, emmcPtnDataInfo_.data, GPT_PARTITION_SIZE)) {
        LOG(ERROR) << "write gpt fail";
        return false;
    }
    return true;
}

// blocksize is 4096, lbaLen is 512. Because in ptable.img block is 512 while in device block is 4096
bool EmmcPtable::ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize)
{
    if (ptbImgBuffer == nullptr || imgBufSize < GPT_PARTITION_SIZE) {
        LOG(ERROR) << "ptbImgBuffer == NULL || imgBufSize < GPT_PARTITION_SIZE";
        return false;
    }
    return UpdateCommInitializeGptPartition(ptbImgBuffer, imgBufSize);
}

bool EmmcPtable::ParseGptHeaderByEmmc(uint8_t *gptImage, const uint32_t len)
{
    GPTHeaderInfo gptHeaderInfo;
    uint32_t blockSize = EMMC_BLOCK_SIZE;
    (void)memset_s(&gptHeaderInfo, sizeof(GPTHeaderInfo), 0, sizeof(GPTHeaderInfo));
    if (!GetPartitionGptHeaderInfo(gptImage + blockSize, blockSize, gptHeaderInfo)) {
        LOG(ERROR) << "GetPartitionGptHeaderInfo fail";
        return false;
    }
#ifndef UPDATER_UT
    uint64_t deviceSize = GetDeviceCapacity();
    uint32_t lbaNum = deviceSize / MIN_EMMC_WRITE_SIZE;
#else
    uint32_t lbaNum = 0xFFFFFFFF; // init to maximum
#endif
    return PartitionCheckGptHeader(gptImage, len, lbaNum, blockSize, gptHeaderInfo);
}

bool EmmcPtable::EmmcReadGpt(uint8_t *ptableData, uint32_t len)
{
    uint32_t number = 0;

    partitionInfo_.clear();
    if (!ParseGptHeaderByEmmc(emmcPtnDataInfo_.data, len)) {
        LOG(ERROR) << "Primary signature invalid";
        return false;
    }
    for (uint32_t i = 0; i < MAX_PARTITION_NUM; i++) {
        uint8_t *startLbaOffset = ptableData + GPT_PARTITION_START_LBA_OFFSET;
        uint8_t *endLbaOffset = ptableData + GPT_PARTITION_START_LBA_OFFSET + GPT_PARTITION_END_LBA_OFFSET;
        uint8_t *typeGuidOffset = ptableData + GPT_PARTITION_TYPE_GUID_OFFSET;
        uint8_t *nameOffset = ptableData + GPT_PARTITION_NAME_OFFSET;
        PtnInfo newPtnInfo;
        (void)memset_s(&newPtnInfo, sizeof(newPtnInfo), 0, sizeof(newPtnInfo));
        ParsePartitionName(nameOffset, MAX_GPT_NAME_SIZE, newPtnInfo.dispName, GPT_DISP_NAME_LEN);
        if (newPtnInfo.dispName[0] == 0) {
            break;
        }
        uint64_t startLba = GET_LLWORD_FROM_BYTE(startLbaOffset);
        uint64_t endLba = GET_LLWORD_FROM_BYTE(endLbaOffset);
        if (memcpy_s(newPtnInfo.partitionTypeGuid, GPT_PARTITION_TYPE_GUID_LEN,
            typeGuidOffset, GPT_PARTITION_TYPE_GUID_LEN) != EOK) {
            LOG(ERROR) << "memcpy guid fail";
        }
        newPtnInfo.startAddr = startLba * LBA_LENGTH;
        newPtnInfo.writePath = MMC_BLOCK_DEV_NAME;
        newPtnInfo.writeMode = "WRITE_RAW";
        /* General algorithm : calculate partition size by lba */
        newPtnInfo.partitionSize = (endLba - startLba + 1) * LBA_LENGTH;
        ptableData += GPT_PARTITION_INFO_LENGTH;
        partitionInfo_.push_back(newPtnInfo);
        number++;
    }

    return number != 0;
}

bool EmmcPtable::UpdateCommInitializeGptPartition(uint8_t *gptImage, const uint32_t len)
{
    if (gptImage == nullptr || len < GPT_PARTITION_SIZE) {
        LOG(ERROR) << "input param invalid";
        return false;
    }
    uint32_t imgBlockSize = ptableData_.lbaLen; // 512
    uint32_t deviceBlockSize = EMMC_BLOCK_SIZE;

    uint8_t *gptHeaderStart = gptImage + imgBlockSize;
    uint8_t *ptableData = gptImage + PROTECTIVE_MBR_SIZE + LBA_LENGTH; /* skip MBR and gpt header */

    if (!CheckProtectiveMbr(gptImage, imgBlockSize) || !CheckIfValidGpt(gptHeaderStart, imgBlockSize)) {
        LOG(ERROR) << "check mbr or header fail";
        emmcPtnDataInfo_.isGptVaild = false;
        return false;
    }

    // for hisi: change ptable.img(512 bytes/block) into format of device(4096 bytes/block)
    if (memcpy_s(emmcPtnDataInfo_.data, GPT_PARTITION_SIZE, gptImage, imgBlockSize) != EOK) {
        LOG(ERROR) << "memcpy_s mbr fail";
        return false;
    }
    if (memcpy_s(emmcPtnDataInfo_.data + deviceBlockSize, GPT_PARTITION_SIZE - deviceBlockSize,
        gptHeaderStart, imgBlockSize) != EOK) {
        LOG(ERROR) << "memcpy_s gpt header fail";
        return false;
    }
    // skip 2 lba length to set gpt entry
    if (memcpy_s(emmcPtnDataInfo_.data + 2 * deviceBlockSize, GPT_PARTITION_SIZE - 2 * deviceBlockSize,
        ptableData, GPT_PARTITION_SIZE - 2 * deviceBlockSize) != EOK) {
        LOG(ERROR) << "memcpy_s gpt data fail";
        return false;
    }
    emmcPtnDataInfo_.writeDataLen = len;
    EmmcPatchGptHeader(emmcPtnDataInfo_, deviceBlockSize);
    emmcPtnDataInfo_.isGptVaild = true;

    return EmmcReadGpt(emmcPtnDataInfo_.data + 2 * deviceBlockSize, len); // 2:skip 2 lba length to set gpt entry
}

bool EmmcPtable::ReadEmmcGptImageToRam()
{
#ifndef UPDATER_UT
    std::string emmcNode = std::string(MMC_BLOCK_DEV_NAME);
#else
    std::string emmcNode = "/data/updater_ext/ptable_parse/ptable.img";
#endif
    int32_t imgLen = GPT_PARTITION_SIZE;
    auto buf = std::make_unique<uint8_t[]>(imgLen);
    uint8_t *buffer = buf.get();
    uint32_t deviceBlockSize = EMMC_BLOCK_SIZE;
    if (buffer == nullptr) {
        LOG(ERROR) << "new buffer failed!";
        return false;
    }
    if (!MemReadWithOffset(emmcNode, 0, buffer, imgLen)) {
        LOG(ERROR) << "read " << imgLen << " bytes from emmcNode " << emmcNode << " failed!";
        return false;
    }

#ifdef UPDATER_UT
    deviceBlockSize = ptableData_.lbaLen;
#endif
    uint8_t *gptHeaderStart = buffer + deviceBlockSize; // if image imgBlockSize, if device deviceBlockSize
    if (!CheckProtectiveMbr(buffer, deviceBlockSize) || !CheckIfValidGpt(gptHeaderStart, deviceBlockSize)) {
        LOG(ERROR) << "check mbr or header fail";
        return false;
    }

    (void)memset_s(emmcPtnDataInfo_.data, GPT_PARTITION_SIZE, 0, GPT_PARTITION_SIZE);
    if (memcpy_s(emmcPtnDataInfo_.data, GPT_PARTITION_SIZE, buffer, imgLen) != EOK) {
        LOG(ERROR) << "memcpy_s GPT fail";
        return false;
    }

    emmcPtnDataInfo_.isGptVaild = true;
    uint8_t *ptableData = buffer + 2 * deviceBlockSize; /* skip MBR and gpt header */
    return EmmcReadGpt(ptableData, imgLen);
}

bool EmmcPtable::LoadPtableFromDevice()
{
    if (!partitionInfo_.empty()) {
        LOG(INFO) << "ptable is already loaded to ram";
        return true;
    }
    if (ReadEmmcGptImageToRam()) {
        LOG(INFO) << "init ptable to ram ok";
        return true;
    }
    return false;
}

bool EmmcPtable::GetPtableImageBuffer(uint8_t *imageBuf, const uint32_t imgBufSize)
{
    if (imageBuf == nullptr || imgBufSize == 0) {
        LOG(ERROR) << "input invalid";
        return false;
    }
    if (memcpy_s(imageBuf, imgBufSize, emmcPtnDataInfo_.data, GPT_PARTITION_SIZE) != EOK) {
        LOG(ERROR) << "memcpy_s failed";
        return false;
    }
    return true;
}

bool EmmcPtable::EditPartitionBuf(uint8_t *imageBuf, uint64_t imgBufSize, std::vector<PtnInfo> &modifyList)
{
    if (imageBuf == nullptr || imgBufSize == 0 || modifyList.empty()) {
        LOG(ERROR) << "input invalid";
        return false;
    }
    uint8_t *gptImage = imageBuf;
    uint32_t imgBlockSize = EMMC_BLOCK_SIZE;
    uint8_t *gptHeader = gptImage + imgBlockSize;
    uint32_t maxPtnCnt = GET_LWORD_FROM_BYTE(&gptHeader[PARTITION_COUNT_OFFSET]);
    uint32_t ptnEntrySize = GET_LWORD_FROM_BYTE(&gptHeader[PENTRY_SIZE_OFFSET]);
    uint64_t gptHeaderLen = EMMC_BLOCK_SIZE;
    uint64_t gptSize = static_cast<uint64_t>(maxPtnCnt) * ptnEntrySize + imgBlockSize + gptHeaderLen;
    uint64_t devDensity = GetDeviceCapacity();
    if (devDensity == 0) {
        LOG(ERROR) << "get emmc capacity fail";
        return false;
    }
    uint32_t devBlockSize = EMMC_BLOCK_SIZE;
    struct GptParseInfo gptInfo(imgBlockSize, devBlockSize, devDensity);
    for (auto &t : modifyList) {
        if (!ChangeGpt(gptImage, gptSize, gptInfo, t)) {
            LOG(ERROR) << "ChangeGpt failed";
            return false;
        }
    }
    EmmcPartitionDataInfo newEmmcPartitionDataInfo;
    newEmmcPartitionDataInfo.writeDataLen = ptableData_.emmcGptDataLen;
    (void)memset_s(newEmmcPartitionDataInfo.data, GPT_PARTITION_SIZE, 0, GPT_PARTITION_SIZE);
    if (memcpy_s(newEmmcPartitionDataInfo.data, GPT_PARTITION_SIZE, imageBuf, imgBlockSize) != EOK) {
        LOG(ERROR) << "memcpy_s failed";
        return false;
    }
    EmmcPatchGptHeader(newEmmcPartitionDataInfo, imgBlockSize);
    return true;
}
}