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

#ifndef UPDATER_UFS_PTABLE_H
#define UPDATER_UFS_PTABLE_H
#include <iostream>
#include "ptable.h"

namespace Updater {
class UfsPtable : public Ptable {
public:
    UfsPtable() = default;
    DISALLOW_COPY_MOVE(UfsPtable);
    ~UfsPtable() override {}

    uint32_t GetDeviceLunNum();
    bool ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize) override;
    bool LoadPtableFromDevice() override;
    bool WritePartitionTable() override;
    bool EditPartitionBuf(uint8_t *imageBuf, uint64_t imgBufSize, std::vector<PtnInfo> &modifyList) override;
    bool GetPtableImageBuffer(uint8_t *imageBuf, const uint32_t imgBufSize) override;
    bool CorrectBufByPtnList(uint8_t *imageBuf, uint64_t imgBufSize, const std::vector<PtnInfo> &srcInfo,
                             const std::vector<PtnInfo> &dstInfo) override;
    bool SyncABLunPtableDevice(const int sourceSlot) override;
    bool GetABLunPartitionInfo(const int sourceSlot, const std::string &srcNode,
        const std::string &tgtNode, uint32_t &offset) override;
#ifndef UPDATER_UT
protected:
#else
public:
#endif
    static constexpr uint32_t TMP_DATA_SIZE = 32 * 1024;
    static constexpr uint32_t MAX_LUN_NUMBERS = 26;
    static constexpr uint32_t MIN_UFS_WRITE_SIZE = 4096;
    static constexpr uint32_t GPT_PTABLE_BACKUP_SIZE = 33; // back ptable at the end of lun
    static constexpr uint32_t READ_LINK_BUFFER_LENTH = 1024; // readlink buffer size

    struct UfsPartitionDataInfo {
        bool isGptVaild;
        uint32_t lunIndex;
        uint64_t lunSize; // lun device density
        uint32_t writeDataLen; // data len written to UFS
        uint8_t data[TMP_DATA_SIZE]; // ptable image data
    };

    uint32_t deviceLunNum_ { 0 };
    std::vector<UfsPartitionDataInfo> ufsPtnDataInfo_;
    bool hasBackupPtable_ {false};

    virtual uint64_t GetDeviceLunCapacity(const uint32_t lunIndex);
    virtual uint32_t GetPtableExtraOffset(void);
    virtual uint32_t GetDeviceBlockSize(void);
    virtual std::string GetDeviceLunNodePath(const uint32_t lun);
    virtual int32_t GetLunNumFromNode(const std::string &ufsNode);
    virtual bool CheckDeviceLunRemoveable(const uint32_t lunIndex);
    virtual bool IsUsbPath(const uint32_t lunIndex);
#ifndef UPDATER_UT
private:
#else
public:
#endif
    bool EditABPartition(uint8_t *gptImage, const uint32_t blockSize, const int sourceSlot);
    void GetTgtPartitionName(std::string &name, const int sourceSlot);
    bool ModifyBufferPartitionName(uint8_t *buffer, const uint32_t bufferSize, const int sourceSlot);
    bool WriteBackupPartitionTable(uint32_t lunIdx, uint64_t lunSize);
    bool UfsReadGpt(const uint8_t *gptImage, const uint32_t len, const uint32_t lun, const uint32_t blockSize);
    void UfsReadGptEntry(const uint8_t *gptImage, const uint32_t lun,
                         const uint32_t blockSize, std::vector<PtnInfo>::iterator startIter);
    bool ParseGptHeaderByUfsLun(const uint8_t *gptImage, const uint32_t len, const uint32_t lun,
        const uint32_t blockSize);
    void UfsPatchGptHeader(UfsPartitionDataInfo &ptnDataInfo, const uint32_t blockSize);
    uint32_t LoadAllLunPartitions();
    bool ReadAndCheckMbr(const uint32_t lunIndex, const uint32_t blockSize);
    bool LoadPartitionInfoFromLun(const uint32_t lunIndex, const uint32_t imgLen);
    void SetDeviceLunNum();
    uint8_t *GetPtableImageUfsLunPmbrStart(uint8_t *imageBuf, const uint32_t lunIndex);
    uint8_t *GetPtableImageUfsLunGptHeaderStart(uint8_t *imageBuf, const uint32_t lunIndex);
    uint8_t *GetPtableImageUfsLunEntryStart(uint8_t *imageBuf, const uint32_t lunIndex);
};
} // namespace Updater
#endif // UPDATER_UFS_PTABLE_H