/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "bzip2_adapter.h"
#include <iostream>
#include "bzlib.h"

using namespace Hpackage;

namespace UpdatePatch {
int32_t BZip2Adapter::Open()
{
    (void)memset_s(&stream_, sizeof(bz_stream), 0, sizeof(bz_stream));
    int32_t ret = BZ2_bzCompressInit(&stream_, BLOCK_SIZE_BEST, 0, 0);
    if (ret != BZ_OK) {
        PATCH_LOGE("Failed to bzcompressinit %d", ret);
        return ret;
    }
    init_ = true;
    return ret;
}

int32_t BZip2Adapter::Close()
{
    if (!init_) {
        return PATCH_SUCCESS;
    }
    int32_t ret = BZ2_bzCompressEnd(&stream_);
    if (ret != BZ_OK) {
        PATCH_LOGE("Failed to bz_compressEnd %d", ret);
        return ret;
    }
    init_ = false;
    return ret;
}

int32_t BZipBuffer2Adapter::WriteData(const BlockBuffer &srcData)
{
    stream_.next_in = reinterpret_cast<char*>(srcData.buffer);
    stream_.avail_in = srcData.length;

    if (offset_ + dataSize_ + srcData.length > buffer_.size()) {
        buffer_.resize(buffer_.size() + srcData.length);
    }
    char *next = reinterpret_cast<char*>(buffer_.data() + offset_ + dataSize_);
    stream_.avail_out = buffer_.size() - offset_ - dataSize_;
    stream_.next_out = next;
    int32_t ret = BZ_RUN_OK;
    do {
        ret = BZ2_bzCompress(&stream_, BZ_RUN);
        if (stream_.avail_in == 0) {
            break;
        }
    } while (ret == BZ_RUN_OK);
    if (ret != BZ_RUN_OK) {
        PATCH_LOGE("BZipBuffer2Adapter::WriteData : Failed to write data ret %d", ret);
        return ret;
    }
    if (stream_.avail_in != 0) {
        PATCH_LOGE("BZipBuffer2Adapter::WriteData : Failed to write data");
        return ret;
    }
    dataSize_ += stream_.next_out - next;
    return PATCH_SUCCESS;
}

int32_t BZipBuffer2Adapter::FlushData(size_t &dataSize)
{
    dataSize = 0;
    PATCH_DEBUG("FlushData dataSize_ %d ", dataSize_);
    stream_.next_in = 0;
    stream_.avail_in = 0;
    stream_.avail_out = buffer_.size() - offset_ - dataSize_;
    char *next = reinterpret_cast<char*>(buffer_.data() + offset_ + dataSize_);
    stream_.next_out = next;
    int ret = BZ_FINISH_OK;
    while (ret == BZ_FINISH_OK) {
        ret = BZ2_bzCompress(&stream_, BZ_FINISH);
        if (stream_.avail_out == 0) {
            dataSize_ += stream_.next_out - next;
            buffer_.resize(buffer_.size() + IGMDIFF_LIMIT_UNIT);
            stream_.avail_out = buffer_.size() - offset_ - dataSize_;
            next = reinterpret_cast<char*>(buffer_.data() + offset_ + dataSize_);
            stream_.next_out = next;
        }
    }
    if (ret != BZ_RUN_OK && ret != BZ_STREAM_END) {
        PATCH_LOGE("Failed to write data %d", ret);
        return ret;
    }
    dataSize_ += stream_.next_out - next;
    PATCH_DEBUG("FlushData offset_ %zu dataSize_ %zu ", offset_, dataSize_);
    dataSize = dataSize_;
    return 0;
}

int32_t BZip2StreamAdapter::Open()
{
    buffer_.resize(IGMDIFF_LIMIT_UNIT);
    return BZip2Adapter::Open();
}

int32_t BZip2StreamAdapter::WriteData(const BlockBuffer &srcData)
{
    stream_.next_in = reinterpret_cast<char*>(srcData.buffer);
    stream_.avail_in = srcData.length;

    stream_.avail_out = buffer_.size();
    stream_.next_out = reinterpret_cast<char*>(buffer_.data());
    int32_t ret = BZ_RUN_OK;
    do {
        ret = BZ2_bzCompress(&stream_, BZ_RUN);
        if (stream_.avail_out == 0) {
            outStream_.write(buffer_.data(), buffer_.size());
            dataSize_ += stream_.next_out - reinterpret_cast<char*>(buffer_.data());
            stream_.avail_out = buffer_.size();
            stream_.next_out = reinterpret_cast<char*>(buffer_.data());
        }
        if (stream_.avail_in == 0) {
            break;
        }
    } while (ret == BZ_RUN_OK);
    if (ret != BZ_RUN_OK) {
        PATCH_LOGE("BZip2StreamAdapter::WriteData : Failed to write data ret %d", ret);
        return ret;
    }
    if (stream_.avail_in != 0) {
        PATCH_LOGE("BZip2StreamAdapter::WriteData : Failed to write data");
        return ret;
    }
    if (stream_.avail_out != buffer_.size()) {
        outStream_.write(buffer_.data(), stream_.next_out - reinterpret_cast<char*>(buffer_.data()));
    }
    dataSize_ += stream_.next_out - reinterpret_cast<char*>(buffer_.data());
    return PATCH_SUCCESS;
}
int32_t BZip2StreamAdapter::FlushData(size_t &dataSize)
{
    dataSize = 0;
    PATCH_DEBUG("FlushData dataSize_ %d ", dataSize_);
    stream_.next_in = 0;
    stream_.avail_in = 0;
    stream_.avail_out = buffer_.size();
    stream_.next_out = reinterpret_cast<char*>(buffer_.data());
    int ret = BZ_FINISH_OK;
    while (ret == BZ_FINISH_OK) {
        ret = BZ2_bzCompress(&stream_, BZ_FINISH);
        if (stream_.avail_out == 0) {
            outStream_.write(buffer_.data(), stream_.next_out - reinterpret_cast<char*>(buffer_.data()));
            dataSize_ += stream_.next_out - reinterpret_cast<char*>(buffer_.data());
            stream_.avail_out = buffer_.size();
            stream_.next_out = reinterpret_cast<char*>(buffer_.data());
        }
    }
    if (ret != BZ_RUN_OK && ret != BZ_STREAM_END) {
        PATCH_LOGE("Failed to write data %d", ret);
        return ret;
    }
    if (stream_.avail_out != buffer_.size()) {
        outStream_.write(buffer_.data(), stream_.next_out - reinterpret_cast<char*>(buffer_.data()));
    }
    dataSize_ += stream_.next_out - reinterpret_cast<char*>(buffer_.data());
    PATCH_DEBUG("FlushData dataSize %zu %zu", dataSize_, static_cast<size_t>(outStream_.tellp()));
    dataSize = dataSize_;
    return 0;
}

int32_t BZip2BufferReadAdapter::Open()
{
    if (init_) {
        PATCH_LOGE("State error %d", init_);
        return -1;
    }
    if (dataLength_ > buffer_.length) {
        PATCH_LOGE("Invalid buffer length");
        return -1;
    }

    PATCH_DEBUG("BZip2BufferReadAdapter::Open %zu dataLength_ %zu", offset_, dataLength_);
    (void)memset_s(&stream_, sizeof(bz_stream), 0, sizeof(bz_stream));
    int32_t ret = BZ2_bzDecompressInit(&stream_, 0, 0);
    if (ret != BZ_OK) {
        PATCH_LOGE("Failed to open read mem ret %d", ret);
        return -1;
    }
    stream_.avail_in = static_cast<unsigned int>(dataLength_);
    stream_.next_in  = reinterpret_cast<char*>(buffer_.buffer + offset_);

    init_ = true;
    return PATCH_SUCCESS;
}

int32_t BZip2BufferReadAdapter::Close()
{
    if (!init_) {
        return PATCH_SUCCESS;
    }
    int32_t ret = 0;
    ret = BZ2_bzDecompressEnd(&stream_);
    if (ret != BZ_OK) {
        PATCH_LOGE("Failed to close read mem ret %d", ret);
        return -1;
    }
    init_ = false;
    return PATCH_SUCCESS;
}

int32_t BZip2BufferReadAdapter::ReadData(BlockBuffer &info)
{
    if (!init_) {
        PATCH_LOGE("State error %d", init_);
        return -1;
    }
    int32_t ret = 0;
    size_t readLen = 0;
    stream_.next_out = reinterpret_cast<char*>(info.buffer);
    stream_.avail_out = info.length;
    while (1) {
        ret = BZ2_bzDecompress(&stream_);
        if (ret == BZ_STREAM_END) {
            readLen = info.length - stream_.avail_out;
            break;
        }
        if (ret != BZ_OK) {
            PATCH_LOGE("Failed to decompress ret %d", ret);
            return -1;
        }
        if (stream_.avail_out == 0) {
            readLen = info.length;
            break;
        }
        if (stream_.avail_in == 0) {
            PATCH_LOGE("Not enough buffer to decompress");
            return -1;
        }
    }
    if (readLen < info.length) {
        PATCH_LOGE("Failed to read mem ret %zu length %zu", readLen, info.length);
        return -1;
    }
    return 0;
}
} // namespace UpdatePatch
