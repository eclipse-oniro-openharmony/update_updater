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

#include "ufs_ptable.h"

#include <algorithm>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"

namespace Updater {
uint32_t UfsPtable::GetDeviceLunNum()
{
    return deviceLunNum_;
}

uint64_t UfsPtable::GetDeviceLunCapacity(const uint32_t lunIndex)
{
    char lunIndexName = 'a' + lunIndex;
    std::string capacityPath = std::string(PREFIX_SYS_CLASS_BLOCK) + lunIndexName + "/size";
    uint64_t capacity = 0;
    GetCapacity(capacityPath, capacity);
    return capacity;
}

uint32_t UfsPtable::GetPtableExtraOffset(void)
{
    return 0;
}

// avoid u disk being recognized as a valid gpt lun device
bool UfsPtable::CheckDeviceLunRemoveable(const uint32_t lunIndex)
{
    constexpr uint32_t minRemoveableStartIdx = 3;
    if (lunIndex <= minRemoveableStartIdx) {
        return false;
    }
    char lunIndexName = 'a' + lunIndex;
    std::string removeableNode = std::string(PREFIX_SYS_CLASS_BLOCK) + lunIndexName + "/removeable";
    std::string removeableResult {};
    if (!ReadFileToString(removeableNode, removeableResult)) {
        LOG(ERROR) << "read removable node failed";
        return false;
    }
    LOG(DEBUG) << "lun " << lunIndex << " removable result is : " << removeableResult;
    return removeableResult == "1";
}

uint32_t UfsPtable::GetDeviceBlockSize(void)
{
    return ptableData_.blockSize;
}

std::string UfsPtable::GetDeviceLunNodePath(const uint32_t lun)
{
    char lunIndexName = 'a' + lun;
    return std::string(PREFIX_UFS_NODE) + lunIndexName;
}

void UfsPtable::SetDeviceLunNum()
{
    if (deviceLunNum_ > 0) {
        return;
    }
    uint32_t lunIndex;
    for (lunIndex = 0; lunIndex < MAX_LUN_NUMBERS; lunIndex++) {
        std::string ufsNode = GetDeviceLunNodePath(lunIndex);
        if (!CheckFileExist(ufsNode)) {
            LOG(ERROR) << "file " << ufsNode << " is not exist";
            break;
        }
#ifndef UPDATER_UT
        if (CheckDeviceRemoveable(lunIndex)) {
            LOG(ERROR) << "device " << ufsNode << " is removable, may be a u disk";
            break;
        }
#endif
    }
    deviceLunNum_ = lunIndex;
    LOG(INFO) << "device lun num is " << deviceLunNum_;
    return;
}

bool UfsPtable::ParseGptHeaderByUfsLun(const uint8_t *gptImage, const uint32_t len,
    const uint32_t lun, const uint32_t blockSize)
{
    GPTHeaderInfo gptHeaderInfo;
    (void)memset_s(&gptHeaderInfo, sizeof(GPTHeaderInfo), 0, sizeof(GPTHeaderInfo));
    if (!GetPartitionGptHeaderInfo(gptImage + blockSize, blockSize, gptHeaderInfo)) {
        LOG(ERROR) << "GetPartitionGptHeaderInfo fail";
        return false;
    }
    uint32_t deviceBlockSize = GetDeviceBlockSize();
    if (deviceBlockSize == 0) {
        LOG(ERROR) << "block device size invalid " << deviceBlockSize;
        return false;
    }
    uint64_t lunDeviceSize = GetDeviceLunCapacity(lun);
    uint32_t lunLbaNum = lunDeviceSize / deviceBlockSize;
    return PartitionCheckGptHeader(gptImage, len, lunLbaNum, blockSize, gptHeaderInfo);
}

bool UfsPtable::UfsReadGpt(const uint8_t *gptImage, const uint32_t len,
    const uint32_t lun, const uint32_t blockSize)
{
    if (gptImage == nullptr || len < ptableData_.writeDeviceLunSize || lun >= MAX_LUN_NUMBERS || blockSize == 0) {
        LOG(ERROR) << "invaild input";
        return false;
    }
    if (!ParseGptHeaderByUfsLun(gptImage, len, lun, blockSize)) {
        LOG(ERROR) << "Primary signature invalid";
        return false;
    }
    auto startIter = partitionInfo_.end();
    for (auto it = partitionInfo_.begin(); it != partitionInfo_.end();) {
        if ((*it).lun == lun) {
            it = partitionInfo_.erase(it);
            startIter = it;
            continue;
        }
        it++;
    }

    uint32_t partEntryCnt = blockSize / PARTITION_ENTRY_SIZE;
    uint32_t partition0 = GET_LLWORD_FROM_BYTE(gptImage + blockSize + PARTITION_ENTRIES_OFFSET);

    uint32_t count = 0;
    const uint8_t *data = nullptr;
    for (uint32_t i = 0; i < (MAX_PARTITION_NUM / partEntryCnt) && count < MAX_PARTITION_NUM; i++) {
        data = gptImage + (partition0 + i) * blockSize;
        for (uint32_t j = 0; j < partEntryCnt; j++) {
            uint8_t typeGuid[GPT_PARTITION_TYPE_GUID_LEN] = {0};
            if (memcpy_s(typeGuid, sizeof(typeGuid), &data[(j * PARTITION_ENTRY_SIZE)], sizeof(typeGuid)) != EOK) {
                LOG(ERROR) << "memcpy guid fail";
            }
            if (typeGuid[0] == 0x00 && typeGuid[1] == 0x00) { // 0x00 means no partition
                i = MAX_PARTITION_NUM / partEntryCnt;
                break;
            }
            uint64_t firstLba = GET_LLWORD_FROM_BYTE(&data[(j * PARTITION_ENTRY_SIZE) + FIRST_LBA_OFFSET]);
            uint64_t lastLba = GET_LLWORD_FROM_BYTE(&data[(j * PARTITION_ENTRY_SIZE) + LAST_LBA_OFFSET]);
            // add a new partition info into partitionInfo_ vector
            PtnInfo newPtnInfo = {};
            newPtnInfo.startAddr = firstLba * static_cast<uint64_t>(GetDeviceBlockSize());
            newPtnInfo.writePath = GetDeviceLunNodePath(lun);
            // General algorithm : calculate partition size by lba
            newPtnInfo.partitionSize = (lastLba - firstLba + 1) * static_cast<uint64_t>(GetDeviceBlockSize());
            const uint8_t *nameOffset = data + (j * PARTITION_ENTRY_SIZE + GPT_PARTITION_NAME_OFFSET);
            // 2 bytes for 1 charactor of partition name
            ParsePartitionName(nameOffset, MAX_GPT_NAME_SIZE, newPtnInfo.dispName, MAX_GPT_NAME_SIZE / 2);
            (void)memcpy_s(newPtnInfo.partitionTypeGuid, sizeof(newPtnInfo.partitionTypeGuid),
                typeGuid, sizeof(typeGuid));
            newPtnInfo.lun = lun;
            startIter = ++(partitionInfo_.insert(startIter, newPtnInfo));
            count++;
        }
    }
    return true;
}


void UfsPtable::UfsPatchGptHeader(UfsPartitionDataInfo &ptnDataInfo, const uint32_t blockSize)
{
    uint32_t deviceBlockSize = GetDeviceBlockSize();
    // mbr len + gptHeader len = 2 blockSize
    if (blockSize == 0 || ptnDataInfo.writeDataLen < 2 * blockSize || ptnDataInfo.lunSize == 0 ||
        deviceBlockSize == 0) {
        LOG(ERROR) << "invaild argument";
        return;
    }
    uint64_t cardSizeSector = ptnDataInfo.lunSize / deviceBlockSize;
    if (cardSizeSector == 0) {
        cardSizeSector = DEFAULT_SECTOR_NUM;
    }
    // Patching primary header
    uint8_t *primaryGptHeader = ptnDataInfo.data + blockSize;
    uint64_t lastUsableSector = cardSizeSector - 1 - (hasBackupPtable_ ? GPT_PTABLE_BACKUP_SIZE : 0);
    if (reservedSize_ != 0 && lastUsableSector > reservedSize_) {
        LOG(INFO) << "reserve " << reservedSize_ << "block for " << GetDeviceLunNodePath(ptnDataInfo.lunIndex);
        lastUsableSector -= reservedSize_;
    }
    LOG(INFO) << "cardSizeSector " << cardSizeSector << ", lastUsableSector " << lastUsableSector;
    PUT_LONG_LONG(primaryGptHeader + BACKUP_HEADER_OFFSET, (cardSizeSector - 1));
    PUT_LONG_LONG(primaryGptHeader + LAST_USABLE_LBA_OFFSET, lastUsableSector);
    // Find last partition
    uint32_t totalPart = 0;
    while (((TMP_DATA_SIZE - blockSize - blockSize) > totalPart * PARTITION_ENTRY_SIZE) &&
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
    uint64_t partitionSize = (lastLba - firstLba + 1) * deviceBlockSize;
    std::string partitionName;
    uint8_t *nameOffset = lastPartOffset + GPT_PARTITION_NAME_OFFSET;
    // 2 bytes for 1 charactor of partition name
    ParsePartitionName(nameOffset, MAX_GPT_NAME_SIZE, partitionName, MAX_GPT_NAME_SIZE / 2);
    if (partitionName == USERDATA_PARTITION || (totalPart == 1 && partitionSize == 0)) {
        // patch userdata or only one partition
        PUT_LONG_LONG(lastPartOffset + PARTITION_ENTRY_LAST_LBA, lastUsableSector);
        LOG(INFO) << "partitionSize=" << partitionSize << ", partition_name:" << partitionName;
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

// blocksize is 4096, lbaLen is 512. Because in ptable.img block is 512 while in device block is 4096
bool UfsPtable::ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize)
{
    if (ptbImgBuffer == nullptr) {
        LOG(ERROR) << "input param invalid";
        return false;
    }

    uint32_t imgBlockSize = ptableData_.lbaLen; // 512
    uint32_t deviceBlockSize = GetDeviceBlockSize();
    if (imgBufSize < ptableData_.emmcGptDataLen + ptableData_.imgLuSize + GetPtableExtraOffset()) {
        LOG(ERROR) << "input param invalid imgBufSize";
        return false;
    }

    SetDeviceLunNum();
    LOG(INFO) << "lun number of ptable:" << deviceLunNum_;

    for (uint32_t i = 0; i < deviceLunNum_; i++) {
        UfsPartitionDataInfo newLunPtnDataInfo;
        (void)memset_s(newLunPtnDataInfo.data, TMP_DATA_SIZE, 0, TMP_DATA_SIZE);
        uint8_t *lunStart = GetPtableImageUfsLunPmbrStart(ptbImgBuffer, i);
        uint8_t *gptHeaderStart = GetPtableImageUfsLunGptHeaderStart(ptbImgBuffer, i);
        // first block is mbr, second block is gptHeader
        if (!CheckProtectiveMbr(lunStart, imgBlockSize) || !CheckIfValidGpt(gptHeaderStart, imgBlockSize)) {
            newLunPtnDataInfo.isGptVaild = false;
            ufsPtnDataInfo_.push_back(newLunPtnDataInfo);
            continue;
        }
        // for hisi: change ptable.img(512 bytes/block) into format of device(4096 bytes/block)
        if (memcpy_s(newLunPtnDataInfo.data, TMP_DATA_SIZE, lunStart, imgBlockSize) != EOK) {
            LOG(WARNING) << "memcpy_s pmbr fail";
        }
        if (memcpy_s(newLunPtnDataInfo.data + deviceBlockSize, TMP_DATA_SIZE - deviceBlockSize,
            gptHeaderStart, imgBlockSize) != EOK) {
            LOG(WARNING) << "memcpy_s gpt header fail";
        }
        // skip 2 lba length to set gpt entry
        if (memcpy_s(newLunPtnDataInfo.data + 2 * deviceBlockSize, TMP_DATA_SIZE - 2 * deviceBlockSize,
            GetPtableImageUfsLunEntryStart(ptbImgBuffer, i), GPT_ENTRYS_SIZE) != EOK) {
            LOG(WARNING) << "memcpy_s gpt data fail";
        }
        newLunPtnDataInfo.writeDataLen = ptableData_.writeDeviceLunSize;
        newLunPtnDataInfo.lunIndex = i + ptableData_.startLunNumber;
        newLunPtnDataInfo.lunSize = GetDeviceLunCapacity(newLunPtnDataInfo.lunIndex);
        UfsPatchGptHeader(newLunPtnDataInfo, deviceBlockSize);
        newLunPtnDataInfo.isGptVaild = true;
        ufsPtnDataInfo_.push_back(newLunPtnDataInfo);
        if (!UfsReadGpt(newLunPtnDataInfo.data, newLunPtnDataInfo.writeDataLen,
            newLunPtnDataInfo.lunIndex, deviceBlockSize)) {
            LOG(ERROR) << "parse ufs gpt fail";
            return false;
        }
    }
    return true;
}

bool UfsPtable::ReadAndCheckMbr(const uint32_t lunIndex, const uint32_t blockSize)
{
    if (blockSize <= 0 || lunIndex < 0 || lunIndex > deviceLunNum_) {
        LOG(ERROR) << "blockSize <= 0";
        return false;
    }

    uint8_t *buffer = new(std::nothrow) uint8_t[blockSize]();
    if (buffer == nullptr) {
        LOG(ERROR) << "new buffer failed!";
        return false;
    }
    std::string ufsNode = GetDeviceLunNodePath(lunIndex);
    if (!MemReadWithOffset(ufsNode, 0, buffer, blockSize)) {
        LOG(ERROR) << "read " << blockSize << " bytes from ufsNode " << ufsNode << " failed!";
        delete [] buffer;
        return false;
    }

    bool result = CheckProtectiveMbr(buffer, blockSize);

    delete [] buffer;
    return result;
}

int32_t UfsPtable::GetLunNumFromNode(const std::string &ufsNode)
{
    if (std::char_traits<char>::length(PREFIX_UFS_NODE) + 1 != ufsNode.length()) {
        LOG(ERROR) << "ufsNode length is " << ufsNode.length() << ", \
            not equal to PREFIX_UFS_NODE(" << std::char_traits<char>::length(PREFIX_UFS_NODE) << ") + 1";
        return -1;
    }
    char ufsLunIndex = ufsNode.back();
    // such as : 'a' - 'a'
    return (ufsLunIndex - 'a');
}

bool UfsPtable::LoadPartitionInfoFromLun(const uint32_t lunIndex, const uint32_t imgLen)
{
    if (imgLen == 0 || lunIndex < 0 || lunIndex > deviceLunNum_) {
        LOG(ERROR) << "imgLen or lunIndex is invaild " << imgLen << " " << lunIndex;
        return false;
    }
    std::string ufsNode = GetDeviceLunNodePath(lunIndex);

    uint8_t *buffer = new(std::nothrow) uint8_t[imgLen]();
    if (buffer == nullptr) {
        LOG(ERROR) << "new buffer failed!";
        return false;
    }
    if (!MemReadWithOffset(ufsNode, 0, buffer, imgLen)) {
        LOG(ERROR) << "read " << imgLen << " bytes from ufsNode " << ufsNode << " failed!";
        delete [] buffer;
        return false;
    }
    UfsPartitionDataInfo newLunPtnDataInfo;
    newLunPtnDataInfo.isGptVaild = true;
    newLunPtnDataInfo.lunIndex = lunIndex;
    newLunPtnDataInfo.lunSize = imgLen;
    newLunPtnDataInfo.writeDataLen = imgLen;
    (void)memset_s(newLunPtnDataInfo.data, TMP_DATA_SIZE, 0, TMP_DATA_SIZE);
    if (memcpy_s(newLunPtnDataInfo.data, TMP_DATA_SIZE, buffer, imgLen) != EOK) {
        LOG(WARNING) << "memcpy_s mbr fail";
    }

    ufsPtnDataInfo_.push_back(newLunPtnDataInfo);
    int32_t result = UfsReadGpt(buffer, imgLen, lunIndex, GetDeviceBlockSize());
    delete [] buffer;
    return result;
}

uint32_t UfsPtable::LoadAllLunPartitions()
{
    uint32_t lunIndex;
    for (lunIndex = 0; lunIndex < deviceLunNum_; lunIndex++) {
        if (ReadAndCheckMbr(lunIndex, GetDeviceBlockSize())) {
            LoadPartitionInfoFromLun(lunIndex, ptableData_.writeDeviceLunSize);
        }
    }
    return lunIndex;
}

bool UfsPtable::LoadPtableFromDevice()
{
    if (!partitionInfo_.empty()) {
        LOG(INFO) << "ptable is already loaded to ram";
        return true;
    }
    SetDeviceLunNum();
    if (LoadAllLunPartitions() == 0) {
        LOG(ERROR) << "init ptable to ram fail";
        return false;
    }
    LOG(INFO) << "init ptable to ram ok";
    return true;
}

bool UfsPtable::WritePartitionTable()
{
    if (ufsPtnDataInfo_.empty()) {
        LOG(ERROR) << "ufsPtnDataInfo_ is empty, write failed!";
        return false;
    }
    for (uint32_t i = 0; i < ufsPtnDataInfo_.size(); i++) {
        uint64_t writeDataLen = ufsPtnDataInfo_[i].writeDataLen;
        std::string ufsNode = GetDeviceLunNodePath(ufsPtnDataInfo_[i].lunIndex);
        LOG(INFO) << "ufs node name:" << ufsNode << ", writeDataLen = " << writeDataLen;

        if (!ufsPtnDataInfo_[i].isGptVaild) {
            LOG(WARNING) <<  "invaild ptable, no need to update";
            continue;
        }
        if (!WriteBufferToPath(ufsNode, 0, ufsPtnDataInfo_[i].data, writeDataLen)) {
            LOG(ERROR) << "write first gpt fail";
            return false;
        }
#ifndef UPDATER_UT
        if (hasBackupPtable_) {
            LOG(INFO) << "should write back up ptable to device";
            uint64_t lunSize = GetDeviceLunCapacity(ufsPtnDataInfo_[i].lunIndex);
            WriteBackupPartitionTable(ufsPtnDataInfo_[i].lunIndex, lunSize);
        }
#endif
    }
    return true;
}

bool UfsPtable::WriteBackupPartitionTable(uint32_t lunIdx, uint64_t lunSize)
{
    if (lunIdx >= ufsPtnDataInfo_.size()) {
        LOG(ERROR) << "lunIdx invalid , lunIdx = " << lunIdx << ", ufsPtnDataInfo size = " << ufsPtnDataInfo_.size();
        return false;
    }

    std::string ufsNode = GetDeviceLunNodePath(lunIdx);
    uint32_t deviceBlockSize = GetDeviceBlockSize();
    if (lunSize == 0 || lunSize <= GPT_PTABLE_BACKUP_SIZE * deviceBlockSize) {
        LOG(ERROR) << "lun size invalid, lun size = " << lunSize;
        return false;
    }
    if (deviceBlockSize == 0) {
        LOG(ERROR) << "deviceBlockSize is invalid";
        return false;
    }
    uint64_t deviceBackGptEntryOffset = lunSize - GPT_PTABLE_BACKUP_SIZE * deviceBlockSize;
    uint64_t deviceBackGptHeaderOffset = lunSize - deviceBlockSize;
    std::unique_ptr<uint8_t[]> backUpHeader = std::make_unique<uint8_t[]>(deviceBlockSize);
    if (memcpy_s(backUpHeader.get(), deviceBlockSize, ufsPtnDataInfo_[lunIdx].data +
        deviceBlockSize, deviceBlockSize) != EOK) {
        LOG(ERROR) << "memcpy error, deviceBlockSize:" << deviceBlockSize;
        return false;
    }
    PatchBackUpGptHeader(backUpHeader.get(), deviceBlockSize, deviceBackGptEntryOffset / deviceBlockSize);
    if (!WriteBufferToPath(ufsNode, deviceBackGptHeaderOffset, backUpHeader.get(), deviceBlockSize)) {
        LOG(ERROR) << "write back up gpt header failed, deviceBackGptHeaderOffset = " << deviceBackGptHeaderOffset
            << ", deviceBlockSize = " << deviceBlockSize;
        return false;
    }

    if (!WriteBufferToPath(ufsNode, deviceBackGptEntryOffset, ufsPtnDataInfo_[lunIdx].data +
        deviceBlockSize * 2, (GPT_PTABLE_BACKUP_SIZE - 1) * deviceBlockSize)) { // 2 : pmbr(1) + gpt header(1)
        LOG(ERROR) << "write back up gpt entries failed, deviceBackGptEntryOffset = " << deviceBackGptEntryOffset
            << ", deviceBlockSize = " << deviceBlockSize;
        return false;
    }
    LOG(INFO) << "write backup partition table successful";
    return true;
}

uint8_t *UfsPtable::GetPtableImageUfsLunPmbrStart(uint8_t *imageBuf, const uint32_t lunIndex)
{
    uint32_t pmbrStart = ptableData_.emmcGptDataLen + GetPtableExtraOffset() + lunIndex * ptableData_.imgLuSize;
    LOG(INFO) << "GetPtableImageUfsLunPmbrStart : " << std::hex << pmbrStart << std::dec;
    return imageBuf + pmbrStart;
}

uint8_t *UfsPtable::GetPtableImageUfsLunGptHeaderStart(uint8_t *imageBuf, const uint32_t lunIndex)
{
    uint32_t gptHeaderStart = ptableData_.emmcGptDataLen + GetPtableExtraOffset() + lunIndex * ptableData_.imgLuSize +
        ptableData_.lbaLen;
    LOG(INFO) << "GetPtableImageUfsLunGptHeaderStart : " << std::hex << gptHeaderStart << std::dec;
    return imageBuf + gptHeaderStart;
}

uint8_t *UfsPtable::GetPtableImageUfsLunEntryStart(uint8_t *imageBuf, const uint32_t lunIndex)
{
    uint32_t entryStart = ptableData_.emmcGptDataLen + GetPtableExtraOffset() + lunIndex * ptableData_.imgLuSize +
        ptableData_.lbaLen + ptableData_.gptHeaderLen;
    LOG(INFO) << "GetPtableImageUfsLunEntryStart : " << std::hex << entryStart << std::dec;
    return imageBuf + entryStart;
}

bool UfsPtable::EditPartitionBuf(uint8_t *imageBuf, uint64_t imgBufSize, std::vector<PtnInfo> &modifyList)
{
    if (imageBuf == nullptr || imgBufSize == 0 || modifyList.empty() || ptableData_.blockSize == 0) {
        LOG(ERROR) << "input invalid";
        return false;
    }
    if (imgBufSize < ptableData_.emmcGptDataLen || deviceLunNum_ == 0) {
        LOG(ERROR) << "can not get offset, imgBufsize =" << imgBufSize << ",emmcGptDataLen ="
            << ptableData_.emmcGptDataLen << ", deviceLunNum = " << deviceLunNum_;
        return false;
    }
 
    uint32_t gptSize = ptableData_.imgLuSize;
    uint32_t imgBlockSize = ptableData_.lbaLen; // 512
    uint32_t deviceBlockSize = GetDeviceBlockSize(); // 4096 or 512
    uint32_t startLu = ptableData_.startLunNumber;
    for (uint32_t i = 0; i < deviceLunNum_; ++i) {
        UfsPartitionDataInfo newLunPtnDataInfo;
        (void)memset_s(newLunPtnDataInfo.data, TMP_DATA_SIZE, 0, TMP_DATA_SIZE);
        std::string ufsNode = GetDeviceLunNodePath(i + startLu);
        newLunPtnDataInfo.lunSize = GetDeviceLunCapacity(i + startLu);
        if (newLunPtnDataInfo.lunSize == 0) {
            LOG(ERROR) << "get devDenisity failed in " << ufsNode;
            return false;
        }
        uint8_t *curGptBuf = GetPtableImageUfsLunPmbrStart(imageBuf, i + startLu);
        if (!ufsPtnDataInfo_[i].isGptVaild) {
            continue;
        }
        struct GptParseInfo gptInfo(imgBlockSize, deviceBlockSize, newLunPtnDataInfo.lunSize -
            (hasBackupPtable_ ? (GPT_PTABLE_BACKUP_SIZE * deviceBlockSize) : 0));
        for (auto &t : modifyList) {
            if (static_cast<uint32_t>(t.lun) == i + startLu && !ChangeGpt(curGptBuf, gptSize, gptInfo, t)) {
                LOG(ERROR) << "ChangeGpt failed";
                return false;
            }
        }
        /* mbr block = 1 block */
        if (memcpy_s(newLunPtnDataInfo.data, TMP_DATA_SIZE, curGptBuf, imgBlockSize) != EOK) {
            LOG(WARNING) << "memcpy_s fail";
        }
        newLunPtnDataInfo.writeDataLen = ptableData_.writeDeviceLunSize;
        UfsPatchGptHeader(newLunPtnDataInfo, imgBlockSize);
    }
    return true;
}
 
bool UfsPtable::GetPtableImageBuffer(uint8_t *imageBuf, const uint32_t imgBufSize)
{
    uint32_t imgBlockSize = ptableData_.lbaLen; // 512
    uint32_t deviceBlockSize = GetDeviceBlockSize();
    SetDeviceLunNum();
    if (imageBuf == nullptr || imgBufSize == 0 ||
        imgBufSize < ptableData_.emmcGptDataLen + GetPtableExtraOffset() + ptableData_.imgLuSize * deviceLunNum_) {
        LOG(ERROR) << "input param invalid";
        return false;
    }
    for (uint32_t i = 0; i < deviceLunNum_; ++i) {
        uint32_t curImgOffset = 0;
        uint32_t curDevOffset = 0;
        uint32_t imgOffset = ptableData_.emmcGptDataLen + GetPtableExtraOffset() + ptableData_.imgLuSize * i;
        /* get ufs node name */
        std::string ufsNode = GetDeviceLunNodePath(i + ptableData_.startLunNumber);
        if (!CheckFileExist(ufsNode)) {
            LOG(ERROR) << "file " << ufsNode << " is not exist";
            return false;
        }
        /* get mbr head */
        if (!MemReadWithOffset(ufsNode, curDevOffset, imageBuf + curImgOffset + imgOffset, imgBlockSize)) {
            LOG(ERROR) << "MemReadWithOffset " << ufsNode << " error";
            return false;
        }
        bool isGptExist = CheckProtectiveMbr(imageBuf + curImgOffset + imgOffset, imgBlockSize);
        curImgOffset += imgBlockSize;
        curDevOffset += deviceBlockSize;
        if (!isGptExist) {
            continue;
        }
        /* get gpt head */
        if (!MemReadWithOffset(ufsNode, curDevOffset, imageBuf + curImgOffset + imgOffset, imgBlockSize)) {
            LOG(ERROR) << "MemReadWithOffset " << ufsNode << " error";
            return false;
        }
        uint32_t maxPartCount = GET_LWORD_FROM_BYTE(&imageBuf[imgOffset + curImgOffset + PARTITION_COUNT_OFFSET]);
        uint32_t entrySize = GET_LWORD_FROM_BYTE(&imageBuf[imgOffset + curImgOffset + PENTRY_SIZE_OFFSET]);
        curImgOffset += imgBlockSize;
        curDevOffset += deviceBlockSize;
        /* get gpt buf */
        uint32_t gptInfoLen = maxPartCount * entrySize;
        if (!MemReadWithOffset(ufsNode, curDevOffset, imageBuf + curImgOffset + imgOffset, gptInfoLen)) {
            LOG(ERROR) << "MemReadWithOffset " << ufsNode << " error" << gptInfoLen;
            return false;
        }
    }
    return true;
}
} // namespace Updater
