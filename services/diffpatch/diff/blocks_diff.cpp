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

#include "blocks_diff.h"
#include "scope_guard.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include "update_diff.h"

using namespace Hpackage;
using namespace std;
using namespace Updater;

namespace UpdatePatch {
#define SWAP(a, b) auto swapTmp = (a); (a) = (b); (b) = swapTmp
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define SET_BUFFER(y, buffer, index) \
    (buffer)[index] = static_cast<uint8_t>((y) % 256); (y) -= (buffer)[index]; (y) = (y) / 256

constexpr uint32_t BUCKET_SIZE = 256;
constexpr uint32_t MULTIPLE_TWO = 2;
constexpr int64_t BLOCK_SCORE = 8;
constexpr int64_t MIN_LENGTH = 16;

static void WriteLE64(const BlockBuffer &buffer, int64_t value)
{
    int32_t index = 0;
    auto y = static_cast<uint64_t>((value < 0) ? -value : value);
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    index++;
    SET_BUFFER(y, buffer.buffer, index);
    if (value < 0) {
        buffer.buffer[index] |= 0x80;
    }
}

int32_t BlocksDiff::MakePatch(const std::string &oldFileName, const std::string &newFileName,
    const std::string &patchFileName)
{
    PATCH_LOGI("BlocksDiff::MakePatch %s ", patchFileName.c_str());
    std::fstream patchFile(patchFileName, std::ios::out | std::ios::trunc | std::ios::binary);
    if (patchFile.fail()) {
        PATCH_LOGE("Failed to open %s", strerror(errno));
        return -1;
    }

    MemMapInfo oldBuffer {};
    MemMapInfo newBuffer {};
    int32_t ret = PatchMapFile(oldFileName, oldBuffer);
    int32_t ret1 = PatchMapFile(newFileName, newBuffer);
    if (ret != 0 || ret1 != 0) {
        PATCH_LOGE("Failed to open %s", newFileName.c_str());
        return -1;
    }
    BlockBuffer newInfo = {newBuffer.memory, newBuffer.length};
    BlockBuffer oldInfo = {oldBuffer.memory, oldBuffer.length};
    std::unique_ptr<BlocksDiff> blockdiff = std::make_unique<BlocksStreamDiff>(patchFile, 0);
    size_t patchSize = 0;
    ret = blockdiff->MakePatch(newInfo, oldInfo, patchSize);
    if (ret != PATCH_SUCCESS) {
        PATCH_LOGE("Failed to generate patch");
        return ret;
    }
    PATCH_DEBUG("MakePatch %zu", static_cast<size_t>(patchFile.tellp()));
    patchFile.close();
    PATCH_LOGI("BlocksDiff::MakePatch success %zu", patchSize);
    return ret;
}

int32_t BlocksDiff::MakePatch(const BlockBuffer &newInfo,
    const BlockBuffer &oldInfo, std::vector<uint8_t> &patchData, size_t offset, size_t &patchSize)
{
    if (patchData.empty()) {
        patchData.resize(IGMDIFF_LIMIT_UNIT);
    }
    std::unique_ptr<BlocksDiff> blockdiff = std::make_unique<BlocksBufferDiff>(patchData, offset);
    int32_t ret = blockdiff->MakePatch(newInfo, oldInfo, patchSize);
    if (ret != PATCH_SUCCESS) {
        PATCH_LOGE("Failed to generate patch");
        return ret;
    }
    if (patchData.size() < patchSize) {
        PATCH_LOGE("Failed to make block patch");
        return -1;
    }
    patchData.resize(patchSize);
    PATCH_LOGI("BlocksDiff::MakePatch success %zu", patchSize);
    return ret;
}

int32_t BlocksDiff::MakePatch(const BlockBuffer &newInfo,
    const BlockBuffer &oldInfo, std::fstream &patchFile, size_t &patchSize)
{
    std::unique_ptr<BlocksDiff> blockdiff = std::make_unique<BlocksStreamDiff>(
        patchFile, static_cast<size_t>(patchFile.tellp()));
    int32_t ret = blockdiff->MakePatch(newInfo, oldInfo, patchSize);
    if (ret != PATCH_SUCCESS) {
        PATCH_LOGE("Failed to generate patch");
        return ret;
    }
    PATCH_LOGI("BlocksDiff::MakePatch success %zu patchFile %zu",
        patchSize, static_cast<size_t>(patchFile.tellp()));
    return ret;
}

int32_t BlocksDiff::MakePatch(const BlockBuffer &newInfo, const BlockBuffer &oldInfo, size_t &patchSize)
{
    if (suffixArray_ == nullptr) {
        suffixArray_.reset(new SuffixArray<int32_t>());
        if (suffixArray_ == nullptr) {
            PATCH_LOGE("Failed to create SuffixArray");
            return -1;
        }
        suffixArray_->Init(oldInfo);
    }

    std::vector<ControlData> controlDatas;
    int32_t ret = GetCtrlDatas(newInfo, oldInfo, controlDatas);
    if (ret != 0) {
        PATCH_LOGE("Failed to get control data");
        return ret;
    }
    patchSize = 0;
    ret = WritePatchHeader(0, 0, 0, patchSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to write patch header");
        return ret;
    }

    size_t controlSize = patchSize;
    ret = WriteControlData(controlDatas, patchSize);
    PATCH_CHECK(ret == BZ_OK, return ret, "Failed to write diff data");
    controlSize = patchSize - controlSize;

    // 写diff数据
    size_t diffDataSize = patchSize;
    ret = WriteDiffData(controlDatas, patchSize);
    PATCH_CHECK(ret == BZ_OK, return ret, "Failed to write diff data");
    diffDataSize = patchSize - diffDataSize;

    size_t extraDataSize = patchSize;
    ret = WriteExtraData(controlDatas, patchSize);
    PATCH_CHECK(ret == BZ_OK, return ret, "Failed to write extra data");
    extraDataSize = patchSize - extraDataSize;

    // write real data
    size_t headerLen = 0;
    ret = WritePatchHeader(controlSize, diffDataSize, newInfo.length, headerLen);
    if (ret != 0) {
        PATCH_LOGE("Failed to write patch header");
        return ret;
    }
    PATCH_DEBUG("MakePatch success patchSize:%zu controlSize:%zu diffDataSize:%zu, extraDataSize:%zu",
        patchSize, controlSize, diffDataSize, extraDataSize);
    return 0;
}

std::unique_ptr<DeflateAdapter> BlocksBufferDiff::CreateBZip2Adapter(size_t patchOffset)
{
    std::unique_ptr<DeflateAdapter> bzip2Adapter = std::make_unique<BZipBuffer2Adapter>(patchData_,
        offset_ + patchOffset);
    if (bzip2Adapter == nullptr) {
        PATCH_LOGE("Failed to create bzip2Adapter");
        return nullptr;
    }
    bzip2Adapter->Open();
    return bzip2Adapter;
}

std::unique_ptr<DeflateAdapter> BlocksStreamDiff::CreateBZip2Adapter(size_t patchOffset)
{
    std::unique_ptr<DeflateAdapter> bzip2Adapter = std::make_unique<BZip2StreamAdapter>(stream_);
    if (bzip2Adapter == nullptr) {
        PATCH_LOGE("Failed to create bzip2Adapter");
        return nullptr;
    }
    bzip2Adapter->Open();
    return bzip2Adapter;
}

int32_t BlocksBufferDiff::WritePatchHeader(int64_t controlSize,
    int64_t diffDataSize, int64_t newSize, size_t &headerLen)
{
    headerLen = std::char_traits<char>::length(BSDIFF_MAGIC) + sizeof(int64_t) + sizeof(int64_t) + sizeof(int64_t);
    if (patchData_.size() <= headerLen + offset_) {
        PATCH_LOGE("Invalid patch size");
        return -1;
    }

    int32_t ret = memcpy_s(patchData_.data() + offset_, patchData_.size(), BSDIFF_MAGIC,
        std::char_traits<char>::length(BSDIFF_MAGIC));
    if (ret != 0) {
        PATCH_LOGE("Failed to copy magic");
        return ret;
    }
    headerLen = std::char_traits<char>::length(BSDIFF_MAGIC);
    BlockBuffer data = {patchData_.data() + offset_ + headerLen, patchData_.size()};
    WriteLE64(data, controlSize);
    headerLen += sizeof(int64_t);
    BlockBuffer diffData = {patchData_.data() + offset_ + headerLen, patchData_.size()};
    WriteLE64(diffData, diffDataSize);
    headerLen += sizeof(int64_t);
    BlockBuffer newData = {patchData_.data() + offset_ + headerLen, patchData_.size()};
    WriteLE64(newData, newSize);
    headerLen += sizeof(int64_t);
    return 0;
}

int32_t BlocksStreamDiff::WritePatchHeader(int64_t controlSize,
    int64_t diffDataSize, int64_t newSize, size_t &headerLen)
{
    PATCH_DEBUG("WritePatchHeader %zu", static_cast<size_t>(stream_.tellp()));
#ifdef __aarch64__
    if (offset_ > static_cast<size_t>(numeric_limits<std::fstream::off_type>::max())) {
        PATCH_LOGE("offset_ error");
        return -1;
    }
#endif
    stream_.seekp(offset_, std::ios::beg);
    stream_.write(BSDIFF_MAGIC, std::char_traits<char>::length(BSDIFF_MAGIC));
    PkgBuffer buffer(sizeof(int64_t));
    WriteLE64(buffer, controlSize);
    stream_.write(reinterpret_cast<const char*>(buffer.buffer), sizeof(int64_t));
    WriteLE64(buffer, diffDataSize);
    stream_.write(reinterpret_cast<const char*>(buffer.buffer), sizeof(int64_t));
    WriteLE64(buffer, newSize);
    stream_.write(reinterpret_cast<const char*>(buffer.buffer), sizeof(int64_t));
    headerLen = std::char_traits<char>::length(BSDIFF_MAGIC) + sizeof(int64_t)  + sizeof(int64_t)  + sizeof(int64_t);
    stream_.seekp(0, std::ios::end);
    return 0;
}

void BlocksDiff::ComputeOldScore(const BlockBuffer &newInfo,
    const BlockBuffer &oldInfo, int64_t &oldScore, int64_t &matchLen)
{
    int64_t newSize = static_cast<int64_t>(newInfo.length);
    for (int64_t begin = currentOffset_ += matchLen; currentOffset_ < newSize; currentOffset_++) {
        BlockBuffer newBuff = {newInfo.buffer + currentOffset_, newInfo.length - currentOffset_};
        matchLen = suffixArray_->Search(newBuff, { oldInfo.buffer, oldInfo.length }, 0, oldInfo.length, matchPos_);
        for (; begin < currentOffset_ + matchLen; begin++) {
            if ((begin + lastOffset_ < static_cast<int64_t>(oldInfo.length))
                && (oldInfo.buffer[begin + lastOffset_] == newInfo.buffer[begin])) {
                oldScore++;
            }
        }
        if (((matchLen == oldScore) && (matchLen != 0)) || (matchLen > (oldScore + BLOCK_SCORE))) {
            break;
        }
        if ((currentOffset_ + lastOffset_ < static_cast<int64_t>(oldInfo.length)) &&
            (oldInfo.buffer[currentOffset_ + lastOffset_] == newInfo.buffer[currentOffset_])) {
            oldScore--;
        }
    }
}

void BlocksDiff::ComputeLength(const BlockBuffer &newInfo,
    const BlockBuffer &oldInfo, int64_t &lengthFront, int64_t &lengthBack)
{
    lengthFront = 0;
    lengthBack = 0;
    int64_t i = 0;
    int64_t s = 0;
    int64_t tmp = 0;
    while (((lastScan_ + i) < currentOffset_) && ((lastPos_ + i) < static_cast<int64_t>(oldInfo.length))) {
        if (oldInfo.buffer[lastPos_ + i] == newInfo.buffer[lastScan_ + i]) {
            s++;
        }
        i++;
        if ((s * MULTIPLE_TWO - i) > (tmp * MULTIPLE_TWO - lengthFront)) {
            tmp = s;
            lengthFront = i;
        }
    }
    s = 0;
    tmp = 0;
    if (currentOffset_ < static_cast<int64_t>(newInfo.length)) {
        for (i = 1; (currentOffset_ >= lastScan_ + i) && (matchPos_ >= i); i++) {
            if (oldInfo.buffer[matchPos_ - i] == newInfo.buffer[currentOffset_ - i]) {
                s++;
            }
            if ((s * MULTIPLE_TWO - i) > (tmp * MULTIPLE_TWO - lengthBack)) {
                tmp = s;
                lengthBack = i;
            }
        }
    }

    if (lastScan_ + lengthFront > currentOffset_ - lengthBack) {
        int64_t lens = 0;
        int64_t overlap = (lastScan_ + lengthFront) - (currentOffset_ - lengthBack);
        s = 0;
        tmp = 0;
        for (i = 0; i < overlap; i++) {
            if (newInfo.buffer[lastScan_ + lengthFront - overlap + i] ==
                oldInfo.buffer[lastPos_ + lengthFront - overlap + i]) {
                s++;
            }
            if (newInfo.buffer[currentOffset_ - lengthBack + i] == oldInfo.buffer[matchPos_ - lengthBack + i]) {
                s--;
            }
            if (s > tmp) {
                tmp = s;
                lens = i + 1;
            }
        }
        lengthFront += lens - overlap;
        lengthBack -= lens;
    }
}

int32_t BlocksDiff::GetCtrlDatas(const BlockBuffer &newInfo,
    const BlockBuffer &oldInfo, std::vector<ControlData> &controlDatas)
{
    int64_t matchLen = 0;
    while (currentOffset_ < static_cast<int64_t>(newInfo.length)) {
        int64_t oldScore = 0;
        int64_t lenFront = 0;
        int64_t lenBack = 0;
        ComputeOldScore(newInfo, oldInfo, oldScore, matchLen);
        if ((matchLen == oldScore) && (currentOffset_ != static_cast<int64_t>(newInfo.length))) {
            continue;
        }
        ComputeLength(newInfo, oldInfo, lenFront, lenBack);

        // save ctrl data
        ControlData ctrlData;
        ctrlData.diffLength = lenFront;
        ctrlData.extraLength = (currentOffset_ - lenBack) - (lastScan_ + lenFront);
        ctrlData.offsetIncrement = (matchPos_ - lenBack) - (lastPos_ + lenFront);
        ctrlData.diffNewStart = &newInfo.buffer[lastScan_];
        ctrlData.diffOldStart = &oldInfo.buffer[lastPos_];
        ctrlData.extraNewStart = &newInfo.buffer[lastScan_ + lenFront];
        controlDatas.push_back(ctrlData);
        lastScan_ = currentOffset_ - lenBack;
        lastPos_ = matchPos_ - lenBack;
        lastOffset_ = matchPos_ - currentOffset_;
    }
    return 0;
}

int32_t BlocksDiff::WriteControlData(const std::vector<ControlData> controlDatas, size_t &patchSize)
{
    std::unique_ptr<DeflateAdapter> bzip2Adapter = CreateBZip2Adapter(patchSize);
    if (bzip2Adapter == nullptr) {
        PATCH_LOGE("Failed to create bzip2Adapter");
        return -1;
    }
    bzip2Adapter->Open();
    int32_t ret = 0;
    uint8_t buffer[sizeof(int64_t)] = {0};
    BlockBuffer srcData = {buffer, sizeof(int64_t)};
    PATCH_DEBUG("WriteControlData patchSize %zu controlDatas %zu", patchSize, controlDatas.size());
    std::vector<int64_t> data;
    ON_SCOPE_EXIT(closeBzip2Adapter) {
        bzip2Adapter->Close();
    };
    for (size_t i = 0; i < controlDatas.size(); i++) {
        WriteLE64(srcData, controlDatas[i].diffLength);
        ret = bzip2Adapter->WriteData(srcData);
        if (ret != 0) {
            PATCH_LOGE("Failed to write data");
            return ret;
        }
        WriteLE64(srcData, controlDatas[i].extraLength);
        ret = bzip2Adapter->WriteData(srcData);
        if (ret != 0) {
            PATCH_LOGE("Failed to write data");
            return ret;
        }
        WriteLE64(srcData, controlDatas[i].offsetIncrement);
        ret = bzip2Adapter->WriteData(srcData);
        if (ret != 0) {
            PATCH_LOGE("WriteControlData : Failed to write data");
            return ret;
        }
    }
    size_t dataSize = 0;
    ret = bzip2Adapter->FlushData(dataSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to FlushData %d", ret);
        return ret;
    }
    patchSize += dataSize;
    PATCH_DEBUG("WriteControlData exit patchSize %zu", patchSize);
    return 0;
}

int32_t BlocksDiff::WriteDiffData(const std::vector<ControlData> controlDatas, size_t &patchSize)
{
    PATCH_DEBUG("WriteDiffData patchSize %zu", patchSize);
    std::unique_ptr<DeflateAdapter> bzip2Adapter = CreateBZip2Adapter(patchSize);
    if (bzip2Adapter == nullptr) {
        PATCH_LOGE("Failed to create bzip2Adapter");
        return -1;
    }
    bzip2Adapter->Open();
    ON_SCOPE_EXIT(closeBzip2Adapter) {
        bzip2Adapter->Close();
    };

    std::vector<uint8_t> diffData(IGMDIFF_LIMIT_UNIT, 0);
    int32_t ret = 0;
    for (size_t i = 0; i < controlDatas.size(); i++) {
        if (controlDatas[i].diffLength <= 0) {
            continue;
        }

        int64_t offset = 0;
        while (offset < controlDatas[i].diffLength) {
            int64_t cpyLen = static_cast<int64_t>(IGMDIFF_LIMIT_UNIT);
            cpyLen = (offset + cpyLen > controlDatas[i].diffLength) ? (controlDatas[i].diffLength - offset) : cpyLen;
            for (int64_t k = 0; k < cpyLen; k++) {
                diffData[k] = controlDatas[i].diffNewStart[offset + k] - controlDatas[i].diffOldStart[offset + k];
            }

            BlockBuffer srcData = {reinterpret_cast<uint8_t*>(diffData.data()), static_cast<size_t>(cpyLen)};
            ret = bzip2Adapter->WriteData(srcData);
            if (ret != 0) {
                PATCH_LOGE("Failed to write data");
                return ret;
            }
            offset += cpyLen;
        }
    }
    size_t dataSize = 0;
    ret = bzip2Adapter->FlushData(dataSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to FlushData %d", ret);
        return ret;
    }
    patchSize += dataSize;
    PATCH_DEBUG("WriteDiffData exit patchSize %zu dataSize %zu ", patchSize, dataSize);
    return 0;
}

int32_t BlocksDiff::WriteExtraData(const std::vector<ControlData> controlDatas, size_t &patchSize)
{
    PATCH_DEBUG("WriteExtraData patchSize %zu ", patchSize);
    std::unique_ptr<DeflateAdapter> bzip2Adapter = CreateBZip2Adapter(patchSize);
    if (bzip2Adapter == nullptr) {
        PATCH_LOGE("Failed to create bzip2Adapter");
        return -1;
    }
    bzip2Adapter->Open();
    ON_SCOPE_EXIT(closeBzip2Adapter) {
        bzip2Adapter->Close();
    };
    int32_t ret = 0;
    for (size_t i = 0; i < controlDatas.size(); i++) {
        if (controlDatas[i].extraLength <= 0) {
            continue;
        }
        BlockBuffer srcData = {controlDatas[i].extraNewStart, static_cast<size_t>(controlDatas[i].extraLength)};
        ret = bzip2Adapter->WriteData(srcData);
        if (ret != 0) {
            PATCH_LOGE("WriteExtraData : Failed to write data");
            return ret;
        }
    }
    size_t dataSize = 0;
    ret = bzip2Adapter->FlushData(dataSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to FlushData %d", ret);
        return ret;
    }
    patchSize += dataSize;
    PATCH_DEBUG("WriteExtraData exit patchSize %zu", patchSize);
    return 0;
}

template<class DataType>
void SuffixArray<DataType>::Init(const BlockBuffer &oldInfo)
{
    std::vector<DataType> suffixArrayTemp;
    std::vector<DataType> buckets;
    InitBuckets(oldInfo, buckets, suffixArrayTemp);
    DataType i = 0;
    DataType h = 0;
    DataType len = 0;
    for (h = 1; suffixArray_[0] != -(static_cast<DataType>(oldInfo.length) + 1); h += h) {
        len = 0;
        for (i = 0; i < (static_cast<DataType>(oldInfo.length) + 1);) {
            if (suffixArray_[i] < 0) {
                len -= suffixArray_[i];
                i -= suffixArray_[i];
            } else {
                suffixArray_[i - len] = len != 0 ? -len : suffixArray_[i - len];
                len = suffixArrayTemp[suffixArray_[i]] + 1 - i;
                Split(suffixArrayTemp, i, len, h);
                i += len;
                len = 0;
            }
        }
        if (len != 0) {
            suffixArray_[i - len] = -len;
        }
    }

    for (i = 0; i < static_cast<DataType>(oldInfo.length) + 1; i++) {
        suffixArray_[suffixArrayTemp[i]] = i;
    }

    PATCH_DEBUG("SuffixArray::Init %d finish", static_cast<int>(oldInfo.length));
}

template<class DataType>
void SuffixArray<DataType>::SplitForLess(std::vector<DataType> &suffixArrayTemp,
    DataType start, DataType len, DataType h)
{
    DataType j = 0;
    for (DataType k = start; k < start + len; k += j) {
        j = 1;
        DataType x = suffixArrayTemp[suffixArray_[k] + h];
        for (DataType i = 1; k + i < start + len; i++) {
            if (suffixArrayTemp[suffixArray_[k + i] + h] < x) {
                x = suffixArrayTemp[suffixArray_[k + i] + h];
                j = 0;
            }
            if (suffixArrayTemp[suffixArray_[k + i] + h] == x) {
                SWAP(suffixArray_[k + j], suffixArray_[k + i]);
                j++;
            }
        }
        for (DataType i = 0; i < j; i++) {
            suffixArrayTemp[suffixArray_[k + i]] = k + j - 1;
        }
        if (j == 1) {
            suffixArray_[k] = -1;
        }
    }
}

template<class DataType>
void SuffixArray<DataType>::Split(std::vector<DataType> &suffixArrayTemp, DataType start, DataType len, DataType h)
{
    if (len < MIN_LENGTH) {
        SplitForLess(suffixArrayTemp, start, len, h);
        return;
    }

    DataType x = suffixArrayTemp[suffixArray_[start + len / MULTIPLE_TWO] + h];
    DataType jj = 0;
    DataType kk = 0;
    for (DataType i = start; i < (start + len); i++) {
        jj = (suffixArrayTemp[suffixArray_[i] + h] < x) ? (jj + 1) : jj;
        kk = (suffixArrayTemp[suffixArray_[i] + h] == x) ? (kk + 1) : kk;
    }
    jj += start;
    kk += jj;
    DataType i = start;
    DataType j = 0;
    DataType k = 0;
    while (i < jj) {
        if (suffixArrayTemp[suffixArray_[i] + h] < x) {
            i++;
        } else if (suffixArrayTemp[suffixArray_[i] + h] == x) {
            SWAP(suffixArray_[i], suffixArray_[jj + j]);
            j++;
        } else {
            SWAP(suffixArray_[i], suffixArray_[kk + k]);
            k++;
        }
    }
    while (jj + j < kk) {
        if (suffixArrayTemp[suffixArray_[jj + j] + h] == x) {
            j++;
        } else {
            SWAP(suffixArray_[jj + j], suffixArray_[kk + k]);
            k++;
        }
    }
    if (jj > start) {
        Split(suffixArrayTemp, start, jj - start, h);
    }

    for (i = 0; i < kk - jj; i++) {
        suffixArrayTemp[suffixArray_[jj + i]] = kk - 1;
    }
    if (jj == kk - 1) {
        suffixArray_[jj] = -1;
    }
    if (start + len > kk) {
        Split(suffixArrayTemp, kk, start + len - kk, h);
    }
}

template<class DataType>
int64_t SuffixArray<DataType>::MatchLength(const BlockBuffer &oldBuffer, const BlockBuffer &newBuffer) const
{
    int64_t i = 0;
    for (; (i < static_cast<int64_t>(oldBuffer.length)) && (i < static_cast<int64_t>(newBuffer.length)); i++) {
        if (oldBuffer.buffer[i] != newBuffer.buffer[i]) {
            break;
        }
    }
    return i;
}

template<class DataType>
int64_t SuffixArray<DataType>::Search(const BlockBuffer &newInfo,
    const BlockBuffer &oldInfo, int64_t start, int64_t end, int64_t &pos) const
{
    int64_t x = 0;
    int64_t y = 0;
    if ((end - start) < MULTIPLE_TWO) {
        BlockBuffer oldStart = {oldInfo.buffer + suffixArray_[start], oldInfo.length - suffixArray_[start]};
        BlockBuffer oldEnd = {oldInfo.buffer + suffixArray_[end], oldInfo.length - suffixArray_[end]};
        x = MatchLength(oldStart, newInfo);
        y = MatchLength(oldEnd, newInfo);
        if (x > y) {
            pos = suffixArray_[start];
            return x;
        } else {
            pos = suffixArray_[end];
            return y;
        }
    }
    x = start + (end - start) / MULTIPLE_TWO;
    if (memcmp(oldInfo.buffer + suffixArray_[x],
        newInfo.buffer, MIN(oldInfo.length - suffixArray_[x], newInfo.length)) < 0) {
        return Search(newInfo, oldInfo, x, end, pos);
    } else {
        return Search(newInfo, oldInfo, start, x, pos);
    }
}

template<class DataType>
void SuffixArray<DataType>::InitBuckets(const BlockBuffer &oldInfo,
    std::vector<DataType> &buckets, std::vector<DataType> &suffixArrayTemp)
{
    suffixArray_.resize(oldInfo.length + 1, 0);
    suffixArrayTemp.resize(oldInfo.length + 1, 0);
    buckets.resize(BUCKET_SIZE, 0);

    for (size_t i = 0; i < oldInfo.length; i++) {
        buckets[oldInfo.buffer[i]]++;
    }
    for (size_t i = 1; i < buckets.size(); i++) {
        buckets[i] += buckets[i - 1];
    }
    for (size_t i = buckets.size() - 1; i > 0; i--) {
        buckets[i] = buckets[i - 1];
    }
    buckets[0] = 0;

    DataType i;
    for (i = 0; i < static_cast<DataType>(oldInfo.length); i++) {
        suffixArray_[++buckets[oldInfo.buffer[i]]] = i;
    }
    suffixArray_[0] = oldInfo.length;

    for (i = 0; i < static_cast<DataType>(oldInfo.length); i++) {
        suffixArrayTemp[i] = buckets[oldInfo.buffer[i]];
    }
    suffixArrayTemp[oldInfo.length] = 0;

    for (i = 1; i < static_cast<DataType>(BUCKET_SIZE); i++) {
        if (buckets[i] == buckets[i - 1] + 1) {
            suffixArray_[buckets[i]] = -1;
        }
    }
    suffixArray_[0] = -1;
}
} // namespace UpdatePatch
