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

#include "blocks_patch.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include "diffpatch.h"

using namespace Hpackage;
using namespace std;

namespace UpdatePatch {
#define PATCH_MIN std::char_traits<char>::length(BSDIFF_MAGIC) + sizeof(int64_t) * 3
#define GET_BYTE_FROM_BUFFER(v, index, buffer)  (y) = (y) * 256; (y) += buffer[index]
constexpr uint8_t BUFFER_MASK = 0x80;

static int64_t ReadLE64(const uint8_t *buffer)
{
    if (buffer == nullptr) {
        return 0;
    }
    int64_t y = 0;
    int32_t index = static_cast<int32_t>(sizeof(int64_t)) - 1;
    y = buffer[index] & static_cast<uint8_t>(~BUFFER_MASK);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);
    index--;
    GET_BYTE_FROM_BUFFER(y, index, buffer);

    index = static_cast<int32_t>(sizeof(int64_t));
    if (buffer[index - 1] & BUFFER_MASK) {
        y = -y;
    }
    return y;
}

int32_t BlocksPatch::ApplyPatch()
{
    PATCH_LOGI("BlocksPatch::ApplyPatch");
    int64_t controlDataSize = 0;
    int64_t diffDataSize = 0;
    int32_t ret = ReadHeader(controlDataSize, diffDataSize, newSize_);
    if (ret != 0) {
        PATCH_LOGE("Failed to read header ");
        return -1;
    }

    while (newOffset_ < newSize_) {
        ControlData ctrlData {};
        ret = ReadControlData(ctrlData);
        if (ret != 0) {
            PATCH_LOGE("Failed to read control data");
            return ret;
        }
        if (newOffset_ + ctrlData.diffLength > newSize_) {
            PATCH_LOGE("Failed to check new offset %ld %zu", ctrlData.diffLength, newOffset_);
            return PATCH_INVALID_PATCH;
        }

        ret = RestoreDiffData(ctrlData);
        if (ret != 0) {
            PATCH_LOGE("Failed to read diff data");
            return ret;
        }
        oldOffset_ += ctrlData.diffLength;
        newOffset_ += ctrlData.diffLength;
        if (newOffset_ + ctrlData.extraLength > newSize_) {
            PATCH_LOGE("Failed to check new offset %ld %zu", ctrlData.diffLength, newOffset_);
            return PATCH_INVALID_PATCH;
        }

        ret = RestoreExtraData(ctrlData);
        if (ret != 0) {
            PATCH_LOGE("Failed to read extra data");
            return ret;
        }

        newOffset_ += ctrlData.extraLength;
        oldOffset_ += ctrlData.offsetIncrement;
    }
    controlDataReader_->Close();
    diffDataReader_->Close();
    extraDataReader_->Close();
    PATCH_LOGI("BlocksPatch::ApplyPatch %zu newSize: %zu", newOffset_, newSize_);
    return 0;
}

int32_t BlocksPatch::ReadHeader(int64_t &controlDataSize, int64_t &diffDataSize, int64_t &newSize)
{
    PATCH_LOGI("BlocksPatch::ApplyPatch %zu %zu", patchInfo_.start, patchInfo_.length);
    if (patchInfo_.buffer == nullptr || patchInfo_.length <= PATCH_MIN) {
        PATCH_LOGE("Invalid parm");
        return -1;
    }
    BlockBuffer patchData = {patchInfo_.buffer + patchInfo_.start, patchInfo_.length - patchInfo_.start};
    PATCH_LOGI("Restore patch hash %zu %s",
        patchInfo_.length - patchInfo_.start, GeneraterBufferHash(patchData).c_str());
    uint8_t *header = patchInfo_.buffer + patchInfo_.start;
    // Compare header
    if (memcmp(header, BSDIFF_MAGIC, std::char_traits<char>::length(BSDIFF_MAGIC)) != 0) {
        PATCH_LOGE("Corrupt patch, patch head != BSDIFF40");
        return -1;
    }

    /* Read lengths from header */
    size_t offset = std::char_traits<char>::length(BSDIFF_MAGIC);
    controlDataSize = ReadLE64(header + offset);
    offset += sizeof(int64_t);
    diffDataSize = ReadLE64(header + offset);
    offset += sizeof(int64_t);
    newSize = ReadLE64(header + offset);
    offset += sizeof(int64_t);

    if (controlDataSize < 0) {
        PATCH_LOGE("Invalid control data size");
        return -1;
    }
    if (newSize < 0) {
        PATCH_LOGE("Invalid new data size");
        return -1;
    }
    PATCH_CHECK(diffDataSize >= 0 && (diffDataSize + controlDataSize) <= static_cast<int64_t>(patchInfo_.length),
        return -1, "Invalid patch data size");

    BlockBuffer patchBuffer = {header, patchInfo_.length - patchInfo_.start};
    controlDataReader_.reset(new BZip2BufferReadAdapter(offset, static_cast<size_t>(controlDataSize), patchBuffer));
    offset += static_cast<size_t>(controlDataSize);
    diffDataReader_.reset(new BZip2BufferReadAdapter(offset, static_cast<size_t>(diffDataSize), patchBuffer));
    offset += static_cast<size_t>(diffDataSize);
    extraDataReader_.reset(new BZip2BufferReadAdapter(offset,
        patchInfo_.length - patchInfo_.start - offset, patchBuffer));
    PATCH_CHECK(controlDataReader_ != nullptr && diffDataReader_ != nullptr && extraDataReader_ != nullptr,
        return -1, "Failed to create reader");
    controlDataReader_->Open();
    diffDataReader_->Open();
    extraDataReader_->Open();
    return 0;
}

int32_t BlocksPatch::ReadControlData(ControlData &ctrlData)
{
    std::vector<uint8_t> data(sizeof(int64_t), 0);
    BlockBuffer info = {data.data(), sizeof(int64_t)};
    int32_t ret = controlDataReader_->ReadData(info);
    if (ret != 0) {
        PATCH_LOGE("Failed to read diffLength");
        return ret;
    }
    ctrlData.diffLength = ReadLE64(info.buffer);
    ret = controlDataReader_->ReadData(info);
    if (ret != 0) {
        PATCH_LOGE("Failed to read extraLength");
        return ret;
    }
    ctrlData.extraLength = ReadLE64(info.buffer);
    ret = controlDataReader_->ReadData(info);
    if (ret != 0) {
        PATCH_LOGE("Failed to read offsetIncrement");
        return ret;
    }
    ctrlData.offsetIncrement = ReadLE64(info.buffer);
    return 0;
}

int32_t BlocksBufferPatch::ReadHeader(int64_t &controlDataSize, int64_t &diffDataSize, int64_t &newSize)
{
    int32_t ret = BlocksPatch::ReadHeader(controlDataSize, diffDataSize, newSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to read header");
        return -1;
    }
    PATCH_LOGI("ReadHeader controlDataSize: %ld %ld %ld", controlDataSize, diffDataSize, newSize);
    newData_.resize(newSize);
    return 0;
}

int32_t BlocksBufferPatch::RestoreDiffData(const ControlData &ctrlData)
{
    if (ctrlData.diffLength <= 0) {
        return 0;
    }
    BlockBuffer diffData = {newData_.data() + newOffset_, static_cast<size_t>(ctrlData.diffLength)};
    int32_t ret = diffDataReader_->ReadData(diffData);
    if (ret != 0) {
        PATCH_LOGE("Failed to read diff data");
        return ret;
    }

    for (int64_t i = 0; i < ctrlData.diffLength; i++) {
        if (((oldOffset_ + i) >= 0) && (static_cast<size_t>(oldOffset_ + i) < oldInfo_.length)) {
            newData_[newOffset_ + i] += oldInfo_.buffer[oldOffset_ + i];
        }
    }
    return 0;
}

int32_t BlocksBufferPatch::RestoreExtraData(const ControlData &ctrlData)
{
    if (ctrlData.extraLength <= 0) {
        return 0;
    }
    BlockBuffer extraData = {newData_.data() + newOffset_, static_cast<size_t>(ctrlData.extraLength)};
    int32_t ret = extraDataReader_->ReadData(extraData);
    if (ret != 0) {
        PATCH_LOGE("Failed to read extra data");
        return ret;
    }
    return 0;
}

int32_t BlocksStreamPatch::RestoreDiffData(const ControlData &ctrlData)
{
    if (ctrlData.diffLength <= 0) {
        return 0;
    }
    std::vector<uint8_t> diffData(ctrlData.diffLength);
    BlockBuffer diffBuffer = {diffData.data(), diffData.size()};
    int32_t ret = diffDataReader_->ReadData(diffBuffer);
    if (ret != 0) {
        PATCH_LOGE("Failed to read diff data");
        return ret;
    }

    size_t oldOffset = static_cast<size_t>(oldOffset_);
    size_t oldLength = stream_->GetFileLength();
    PkgBuffer buffer {};
    if (stream_->GetStreamType() == PkgStream::PkgStreamType_MemoryMap ||
        stream_->GetStreamType() == PkgStream::PkgStreamType_Buffer) {
        ret = stream_->GetBuffer(buffer);
        if (ret != 0) {
            PATCH_LOGE("Failed to get old buffer");
            return ret;
        }
    } else {
        std::vector<uint8_t> oldData(ctrlData.diffLength);
        size_t readLen = 0;
        ret = stream_->Read(buffer, oldOffset_, ctrlData.diffLength, readLen);
        if (ret != 0 || readLen != static_cast<size_t>(ctrlData.diffLength)) {
            PATCH_LOGE("Failed to get old buffer");
            return ret;
        }
        oldOffset = 0;
    }
    for (int64_t i = 0; i < ctrlData.diffLength; i++) {
        if ((oldOffset_ + i >= 0) && (static_cast<size_t>(oldOffset_ + i) < oldLength)) {
            diffData[i] += buffer.buffer[static_cast<int64_t>(oldOffset) + i];
        }
    }
    // write
    return writer_->Write(newOffset_, diffBuffer, static_cast<size_t>(ctrlData.diffLength));
}

int32_t BlocksStreamPatch::RestoreExtraData(const ControlData &ctrlData)
{
    if (ctrlData.extraLength <= 0) {
        return 0;
    }
    std::vector<uint8_t> extraData(ctrlData.extraLength);
    BlockBuffer extraBuffer = {extraData.data(), static_cast<size_t>(ctrlData.extraLength)};
    int32_t ret = extraDataReader_->ReadData(extraBuffer);
    if (ret != 0) {
        PATCH_LOGE("Failed to read extra data");
        return ret;
    }
    // write
    return writer_->Write(newOffset_, extraBuffer, static_cast<size_t>(ctrlData.extraLength));
}
} // namespace UpdatePatch
