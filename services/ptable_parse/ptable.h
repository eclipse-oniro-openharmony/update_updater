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

#ifndef UPDATER_PTABLE_H
#define UPDATER_PTABLE_H
#define LAST_PATITION_NAME "USERDATA"

#include "macros_updater.h"
#include "json_node.h"

namespace Updater {
#ifndef UPDATER_UT
constexpr const char *PTABLE_TEMP_PATH = "";
#else
constexpr const char *PTABLE_TEMP_PATH = "";
#endif
class Ptable {
public:
    Ptable() = default;
    DISALLOW_COPY_MOVE(Ptable);
    virtual ~Ptable() {}

    static constexpr uint32_t GPT_PARTITION_TYPE_GUID_LEN = 16;
    static constexpr const char *PREFIX_SYS_CLASS_BLOCK = "/sys/class/block/sd";

    struct PtnInfo {
        uint64_t startAddr {};
        uint64_t partitionSize {};
        uint8_t partitionTypeGuid[GPT_PARTITION_TYPE_GUID_LEN] {};
        uint32_t lun {};
        int gptEntryBufOffset {};
        bool isTailPart {false};
        std::string dispName {};
        std::string writeMode {"WRITE_RAW"};
        std::string writePath {};
    };

    struct GptParseInfo {
    GptParseInfo(uint64_t imgBlockSize, uint64_t devBlockSize, uint64_t devDensity)
        : imgBlockSize(imgBlockSize), devBlockSize(devBlockSize), devDensity(devDensity) {}
    uint64_t imgBlockSize;
    uint64_t devBlockSize;
    uint64_t devDensity;
    };
    std::vector<PtnInfo> GetPtablePartitionInfo() const;
    uint32_t GetPtablePartitionNum() const;
    void SetReservedSize(uint64_t reservedSize);
    bool InitPtable();
    uint32_t GetDefaultImageSize() const;
    void PrintPtableInfo() const;
    void PrintPtableInfo(const std::vector<PtnInfo> &ptnInfo) const;
    bool GetPartionInfoByName(const std::string &partitionName, PtnInfo &ptnInfo, int32_t &index);
    std::vector<PtnInfo>& GetPtablePartitionInfoInstance();
    bool LoadPtnInfo(const std::vector<PtnInfo>& ptnInfo);

    virtual bool ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize) = 0;
    virtual bool LoadPtableFromDevice() = 0;
    virtual bool WritePartitionTable() = 0;
    virtual bool EditPartitionBuf(uint8_t *imageBuf, uint64_t imgBufSize, std::vector<PtnInfo> &modifyList) = 0;
    virtual bool GetPtableImageBuffer(uint8_t *imageBuf, const uint32_t imgBufSize) = 0;
    virtual void AddChildPtable(std::unique_ptr<Ptable> child) {}
    virtual bool CorrectBufByPtnList(uint8_t *imageBuf, uint64_t imgBufSize, const std::vector<PtnInfo> &srcInfo,
                                     const std::vector<PtnInfo> &dstInfo);

    int GetEndPtnIndex()
    {
        return endPtnIndex_;
    }

    int GetUsrDataPtnIndex()
    {
        return usrDataPtnIndex_;
    }

    void SetUsrDataPtnIndex(int usrDataPtnIndex)
    {
        usrDataPtnIndex_ = usrDataPtnIndex;
    }

    bool HasTailPart()
    {
        return hasTailpart_;
    }

    void ClearPtnInfo()
    {
        partitionInfo_.clear();
    }

#ifndef UPDATER_UT
protected:
#else
public:
#endif
    const std::string USERDATA_PARTITION = "USERDATA";
    static constexpr uint32_t PARTITION_ENTRY_SIZE = 128;
    static constexpr uint32_t MAX_PARTITION_NUM = 128;
    static constexpr uint32_t FIRST_LBA_OFFSET = 32;
    static constexpr uint32_t LAST_LBA_OFFSET = 40;
    static constexpr uint32_t GPT_PARTITION_NAME_OFFSET = 56;
    static constexpr uint32_t MAX_GPT_NAME_SIZE = 72;
    static constexpr uint32_t PARTITION_ENTRIES_OFFSET = 72;
    static constexpr uint32_t PARTITION_CRC_OFFSET = 88;
    static constexpr uint32_t GPT_DISP_NAME_LEN = 32;
    static constexpr uint64_t DEFAULT_SECTOR_NUM = (4 * 1024 * 1024 * 2 - 1);
    static constexpr uint32_t GPT_HEADER_OFFSET = 24;
    static constexpr uint32_t BACKUP_HEADER_OFFSET = 32;
    static constexpr uint32_t PARTITION_ENTRY_OFFSET = 72;
    static constexpr uint32_t LAST_USABLE_LBA_OFFSET = 48;
    static constexpr uint32_t PARTITION_ENTRY_LAST_LBA = 40;
    static constexpr uint32_t PARTITION_COUNT_OFFSET = 80;
    static constexpr uint32_t PENTRY_SIZE_OFFSET = 84;
    static constexpr uint32_t HEADER_CRC_OFFSET = 16;
    static constexpr uint32_t GPT_CRC_LEN = 92;
    static constexpr uint32_t GPT_ENTRYS_SIZE = 128 * 128;

    // set 32 bits data
    inline void PUT_LONG(uint8_t *x, const uint32_t y)
    {
        *(x) = (y) & 0xff;
        *((x) + 1) = ((y) >> 8) & 0xff;
        *((x) + 2) = ((y) >> 16) & 0xff;
        *((x) + 3) = ((y) >> 24) & 0xff;
    }

    // set 64 bits data
    inline void PUT_LONG_LONG(uint8_t *x, const uint64_t y)
    {
        *(x) = (y) & 0xff;
        *((x) + 1) = (((y) >> 8) & 0xff);
        *((x) + 2) = (((y) >> 16) & 0xff);
        *((x) + 3) = (((y) >> 24) & 0xff);
        *((x) + 4) = (((y) >> 32) & 0xff);
        *((x) + 5) = (((y) >> 40) & 0xff);
        *((x) + 6) = (((y) >> 48) & 0xff);
        *((x) + 7) = (((y) >> 56) & 0xff);
    }

    // LWORD = 4 bytes (32 bits)
    inline uint32_t GET_LWORD_FROM_BYTE(const uint8_t *x)
    {
        uint32_t res = static_cast<unsigned int>(*x) |
            (static_cast<unsigned int>(*(x + 1)) << 8) |
            (static_cast<unsigned int>(*(x + 2)) << 16) |
            (static_cast<unsigned int>(*(x + 3)) << 24);
        return res;
    }

    // LLWORD = 8 bytes (64 bits)
    inline uint64_t GET_LLWORD_FROM_BYTE(const uint8_t *x)
    {
        uint64_t res = static_cast<unsigned long long>(*x) |
            (static_cast<unsigned long long>(*(x + 1)) << 8) |
            (static_cast<unsigned long long>(*(x + 2)) << 16) |
            (static_cast<unsigned long long>(*(x + 3)) << 24) |
            (static_cast<unsigned long long>(*(x + 4)) << 32) |
            (static_cast<unsigned long long>(*(x + 5)) << 40) |
            (static_cast<unsigned long long>(*(x + 6)) << 48) |
            (static_cast<unsigned long long>(*(x + 7)) << 56);
        return res;
    }

    struct GPTHeaderInfo {
        uint32_t headerSize {};
        uint32_t partitionEntrySize {};
        uint32_t maxPartitionCount {};
        uint64_t firstUsableLba {};
    };

    struct PtableData {
        bool dataValid {};
        uint32_t emmcGptDataLen {};
        uint32_t lbaLen {};
        uint32_t gptHeaderLen {};
        uint32_t blockSize {};
        uint32_t imgLuSize {};
        uint32_t startLunNumber {};
        uint32_t writeDeviceLunSize {};
        uint32_t defaultLunNum {};
    };

    std::vector<PtnInfo> partitionInfo_;
    PtableData ptableData_;
    uint64_t reservedSize_ {0};

    int startPtnIndex_ {-1};
    int endPtnIndex_ {-1};
    int usrDataPtnIndex_ {-1};
    bool hasTailpart_ {false};

    PtableData GetPtableData() const;
    bool MemReadWithOffset(const std::string &filePath, const uint64_t offset,
        uint8_t *outData, const uint32_t dataSize);
    bool CheckProtectiveMbr(const uint8_t *gptImage, const uint32_t imgLen);
    bool CheckIfValidGpt(const uint8_t *gptImage, const uint32_t gptImageLen);
    bool GetCapacity(const std::string &filePath, uint64_t &lunCapacity);
    bool GetPartitionGptHeaderInfo(const uint8_t *buffer, const uint32_t bufferLen, GPTHeaderInfo& gptHeaderInfo);
    void PatchBackUpGptHeader(uint8_t *gptHeader, const uint32_t len, uint64_t backGptEntryStart);
    bool PartitionCheckGptHeader(const uint8_t *gptImage, const uint32_t len, const uint64_t lbaNum,
        const uint32_t blockSize, GPTHeaderInfo& gptHeaderInfo);
    void ParsePartitionName(const uint8_t *data, const uint32_t dataLen,
        std::string &name, const uint32_t nameLen);
    uint32_t CalculateCrc32(const uint8_t *buffer, const uint32_t len);
    bool WritePtablePartition(const std::string &path, uint64_t offset, const uint8_t *buffer, uint32_t size);
    bool CheckFileExist(const std::string &fileName);
    bool WriteBufferToPath(const std::string &path, const uint64_t offset, const uint8_t *buffer, const uint32_t size);
    bool ChangeGpt(uint8_t *gptBuf, uint64_t gptSize, GptParseInfo gptInfo, PtnInfo &modifyInfo);
    bool AdjustGpt(uint8_t *ptnInfoBuf, uint64_t bufSize, const std::string &ptnName, uint64_t preLastLBA,
    uint64_t lastPtnLastLBA);

private:
    static constexpr uint64_t MBR_MAGIC_NUM_POS = 0x1FE;
    static constexpr uint8_t MBR_MAGIC_NUM_0 = 0x55;
    static constexpr uint8_t MBR_MAGIC_NUM_1 = 0xAA;
    static constexpr uint32_t MBR_GPT_MAX_NUM = 4; // one disk has most 4 main partitions
    static constexpr uint32_t MBR_GPT_ENTRY = 0x1BE;
    static constexpr uint32_t MBR_GPT_ENTRY_SIZE = 0x010;
    static constexpr uint32_t GPT_TYPE_SIGN_OFFSET = 0x04;
    static constexpr uint32_t MBR_PROTECTIVE_GPT_TYPE = 0xEE;
    static constexpr uint64_t EFI_MAGIC_NUMBER = 0x5452415020494645; // GPT SIGNATURE(8 bytes), little-end, (EFI PART)
    static constexpr uint32_t SECTOR_SIZE = 512;
    static constexpr uint32_t LBA_LENGTH = SECTOR_SIZE;
    static constexpr uint32_t HEADER_SIZE_OFFSET = 12;
    static constexpr uint32_t FIRST_USABLE_LBA_OFFSET = 40;
    static constexpr uint32_t GPT_HEADER_SIZE = 92;
    static constexpr uint32_t PRIMARY_HEADER_OFFSET = 24;
    static constexpr uint32_t MIN_PARTITION_ARRAY_SIZE = 0x4000;

    bool VerifyMbrMagicNum(const uint8_t *buffer, const uint32_t size);
    uint32_t Reflect(uint32_t data, const uint32_t len);

    bool CheckGptHeader(uint8_t *buffer, const uint32_t bufferLen, const uint64_t lbaNum,
        const GPTHeaderInfo& gptHeaderInfo);
    void SetPartitionName(const std::string &name, uint8_t *data, const uint32_t size);
    bool ParsePtableDataNode(const JsonNode &ptableDataNode);
    bool ParsePtableData();
};
} // namespace Updater
#endif // UPDATER_PTABLE_H
