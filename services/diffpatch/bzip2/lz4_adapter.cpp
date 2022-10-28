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

#include "lz4_adapter.h"
#include <iostream>
#include <vector>
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"
#include "pkg_manager.h"

using namespace Hpackage;

namespace UpdatePatch {
Lz4Adapter::Lz4Adapter(UpdatePatchWriterPtr outStream, size_t offset, const PkgManager::FileInfoPtr fileInfo)
    : DeflateAdapter(), outStream_(outStream), offset_(offset)
{
    const Hpackage::Lz4FileInfo *info = (const Hpackage::Lz4FileInfo *)fileInfo;
    compressionLevel_ = info->compressionLevel;
    blockIndependence_ = info->blockIndependence;
    contentChecksumFlag_ = info->contentChecksumFlag;
    blockSizeID_ = info->blockSizeID;
    autoFlush_ = info->autoFlush;
    if (compressionLevel_ < 1) {
        compressionLevel_ = LZ4HC_CLEVEL_MIN - 1;
    }
    if (compressionLevel_ >= LZ4HC_CLEVEL_MAX) {
        compressionLevel_ = LZ4HC_CLEVEL_MAX;
    }
}

int32_t Lz4FrameAdapter::Open()
{
    if (init_) {
        PATCH_LOGE("Has been open");
        return 0;
    }
    LZ4F_preferences_t preferences;
    LZ4F_errorCode_t errorCode = LZ4F_createCompressionContext(&compressionContext_, LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) {
        PATCH_LOGE("Failed to create compress context %s", LZ4F_getErrorName(errorCode));
        return -1;
    }
    if (memset_s(&preferences, sizeof(preferences), 0, sizeof(preferences)) != EOK) {
        PATCH_LOGE("Memset failed");
        return -1;
    }
    preferences.autoFlush = static_cast<uint32_t>(autoFlush_);
    preferences.compressionLevel = compressionLevel_;
    preferences.frameInfo.blockMode = ((blockIndependence_ == 0) ? LZ4F_blockLinked : LZ4F_blockIndependent);
    preferences.frameInfo.blockSizeID = (LZ4F_blockSizeID_t)blockSizeID_;
    preferences.frameInfo.contentChecksumFlag =
        ((contentChecksumFlag_ == 0) ? LZ4F_noContentChecksum : LZ4F_contentChecksumEnabled);

    size_t blockSize = LZ4_BLOCK_SIZE(blockSizeID_);
    blockSize = (blockSize > LZ4B_BLOCK_SIZE) ? LZ4B_BLOCK_SIZE : blockSize;
    inData_.resize(blockSize);
    size_t outBuffSize = LZ4F_compressBound(blockSize, &preferences);
    if (outBuffSize <= 0) {
        PATCH_LOGE("BufferSize must > 0");
        return -1;
    }
    buffer_.resize(outBuffSize);

    PATCH_LOGI("frameInfo blockSizeID %d compressionLevel_: %d blockIndependence_:%d %d %d blockSize %zu %zu %zu",
        static_cast<int32_t>(blockSizeID_), static_cast<int32_t>(compressionLevel_),
        static_cast<int32_t>(blockIndependence_), static_cast<int32_t>(contentChecksumFlag_), autoFlush_,
        blockSize, LZ4_BLOCK_SIZE(blockSizeID_), outBuffSize);

    /* write package header */
    size_t dataSize = LZ4F_compressBegin(compressionContext_, buffer_.data(), buffer_.size(), &preferences);
    if (LZ4F_isError(dataSize)) {
        PATCH_LOGE("Failed to generate header %s", LZ4F_getErrorName(dataSize));
        return -1;
    }
    int32_t ret = outStream_->Write(offset_, {buffer_.data(), dataSize}, dataSize);
    PATCH_CHECK(ret == 0, return -1, "Failed to deflate data");
    offset_ += dataSize;
    init_ = true;
    return ret;
}

int32_t Lz4FrameAdapter::Close()
{
    if (!init_) {
        PATCH_LOGE("Has been close");
        return 0;
    }
    LZ4F_errorCode_t errorCode = LZ4F_freeCompressionContext(compressionContext_);
    if (LZ4F_isError(errorCode)) {
        PATCH_LOGE("Failed to free compress context %s", LZ4F_getErrorName(errorCode));
        return -1;
    }
    init_ = false;
    return 0;
}

int32_t Lz4FrameAdapter::WriteData(const BlockBuffer &srcData)
{
    size_t blockSize = LZ4_BLOCK_SIZE(blockSizeID_);
    int32_t ret = 0;
    if ((currDataSize_ + srcData.length) < inData_.size()) {
        ret = memcpy_s(inData_.data() + currDataSize_, inData_.size(), srcData.buffer, srcData.length);
        if (ret != 0) {
            PATCH_LOGE("Failed to copy data ");
            return -1;
        }
        currDataSize_ += srcData.length;
    } else {
        size_t hasCopyLen = inData_.size() - currDataSize_;
        ret = memcpy_s(inData_.data() + currDataSize_, inData_.size(), srcData.buffer, hasCopyLen);
        if (ret != 0) {
            PATCH_LOGE("Failed to copy data ");
            return -1;
        }

        BlockBuffer data = {inData_.data(), inData_.size()};
        ret = CompressData(data);
        if (ret != 0) {
            PATCH_LOGE("Failed to compress data ");
            return -1;
        }

        size_t remainLen = srcData.length - hasCopyLen;
        currDataSize_ = 0;
        while (remainLen >= inData_.size()) {
            size_t length = (blockSize <= remainLen) ? blockSize : remainLen;
            BlockBuffer blockBuffer = {srcData.buffer + srcData.length - remainLen, length};
            ret = CompressData(blockBuffer);
            if (ret != 0) {
                PATCH_LOGE("Failed compress data ");
                return -1;
            }
            remainLen -= length;
        }
        if (remainLen > 0) {
            ret = memcpy_s(inData_.data(), inData_.size(), srcData.buffer + srcData.length - remainLen, remainLen);
            if (ret != 0) {
                PATCH_LOGE("Failed to copy data ");
                return -1;
            }
            currDataSize_ = remainLen;
        }
    }
    return ret;
}

int32_t Lz4FrameAdapter::CompressData(const BlockBuffer &srcData)
{
    int32_t ret = 0;
    size_t dataSize = LZ4F_compressUpdate(compressionContext_,
        buffer_.data(), buffer_.size(), srcData.buffer, srcData.length, nullptr);
    if (LZ4F_isError(dataSize)) {
        PATCH_LOGE("Failed to compress update %s", LZ4F_getErrorName(dataSize));
        return -1;
    }

    if (dataSize > 0) {
        ret = outStream_->Write(offset_, {buffer_.data(), dataSize}, dataSize);
        if (ret != 0) {
            PATCH_LOGE("Failed write data ");
            return -1;
        }
    }
    offset_ += dataSize;
    return ret;
}

int32_t Lz4FrameAdapter::FlushData(size_t &offset)
{
    if (currDataSize_ > 0) {
        BlockBuffer data = {inData_.data(), currDataSize_};
        int32_t ret = CompressData(data);
        if (ret != 0) {
            PATCH_LOGE("Failed to compress data ");
            return -1;
        }
    }
    size_t dataSize = LZ4F_compressEnd(compressionContext_, buffer_.data(), buffer_.size(), nullptr);
    if (LZ4F_isError(dataSize)) {
        PATCH_LOGE("Failed to compress update end %s", LZ4F_getErrorName(dataSize));
        return -1;
    }
    int32_t ret = outStream_->Write(offset_, {buffer_.data(), dataSize}, dataSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to deflate data");
        return -1;
    }
    offset_ += dataSize;
    offset = offset_;
    return ret;
}

int32_t Lz4BlockAdapter::Open()
{
    if (init_) {
        PATCH_LOGE("Has been open");
        return 0;
    }
    size_t blockSize = LZ4_BLOCK_SIZE(blockSizeID_);
    blockSize = (blockSize > LZ4B_BLOCK_SIZE) ? LZ4B_BLOCK_SIZE : blockSize;
    inData_.resize(blockSize);
    PATCH_LOGI("frameInfo blockSizeID %d compressionLevel_: %d blockIndependence_:%d %d blockSize %zu %zu %zu",
        static_cast<int32_t>(blockSizeID_), static_cast<int32_t>(compressionLevel_),
        static_cast<int32_t>(blockIndependence_), static_cast<int32_t>(contentChecksumFlag_),
        blockSize, LZ4_BLOCK_SIZE(blockSizeID_), LZ4B_BLOCK_SIZE);

    /* write package */
    int bufferSize = LZ4_compressBound(LZ4_BLOCK_SIZE(blockSizeID_));
    if (bufferSize < 0) {
        PATCH_LOGE("Buffer size should >= 0");
        return -1;
    }
    buffer_.resize(static_cast<size_t>(bufferSize));
    int32_t ret = memcpy_s(buffer_.data(), buffer_.size(), &LZ4B_MAGIC_NUMBER, sizeof(int32_t));
    if (ret != 0) {
        PATCH_LOGE("Failed to memcpy ");
        return -1;
    }
    ret = outStream_->Write(offset_, {buffer_.data(), sizeof(int32_t)}, sizeof(int32_t));
    if (ret != 0) {
        PATCH_LOGE("Failed to deflate data ");
        return -1;
    }
    offset_ += sizeof(int32_t);
    init_ = true;
    return ret;
}

int32_t Lz4BlockAdapter::Close()
{
    if (!init_) {
        PATCH_LOGE("Has been close");
        return 0;
    }
    init_ = false;
    return 0;
}

int32_t Lz4BlockAdapter::CompressData(const BlockBuffer &srcData)
{
    int32_t dataSize = 0;
    // Compress Block, reserve 4 bytes to store block size
    if (compressionLevel_ < LZ4HC_CLEVEL_MIN) { // hc 最小是3
        dataSize = LZ4_compress_default(reinterpret_cast<const char *>(srcData.buffer),
            reinterpret_cast<char *>(buffer_.data() + LZ4B_REVERSED_LEN),
            (int32_t)srcData.length, (int32_t)(buffer_.size() - LZ4B_REVERSED_LEN));
    } else {
        dataSize = LZ4_compress_HC(reinterpret_cast<const char *>(srcData.buffer),
            reinterpret_cast<char *>(buffer_.data() + LZ4B_REVERSED_LEN),
            (int32_t)srcData.length, (int32_t)(buffer_.size() - LZ4B_REVERSED_LEN),
            compressionLevel_);
    }
    if (dataSize <= 0) {
        PATCH_LOGE("Failed to compress data dataSize %d ", dataSize);
        return -1;
    }

    // Write block to buffer.
    // Buffer format: <block size> + <block contents>
    int32_t ret = memcpy_s(buffer_.data(), buffer_.size(), &dataSize, sizeof(int32_t));
    if (ret != 0) {
        PATCH_LOGE("Failed to memcpy ");
        return -1;
    }
    dataSize += LZ4B_REVERSED_LEN;

    ret = outStream_->Write(offset_, {buffer_.data(), static_cast<size_t>(dataSize)}, static_cast<size_t>(dataSize));
    if (ret != 0) {
        PATCH_LOGE("Failed to write data ");
        return -1;
    }
    offset_ += static_cast<size_t>(dataSize);
    return ret;
}

int32_t Lz4BlockAdapter::FlushData(size_t &offset)
{
    if (currDataSize_ > 0) {
        BlockBuffer data = {inData_.data(), currDataSize_};
        int32_t ret = CompressData(data);
        if (ret != 0) {
            PATCH_LOGE("Failed to compress data ");
            return -1;
        }
    }
    offset = offset_;
    return 0;
}
} // namespace UpdatePatch
