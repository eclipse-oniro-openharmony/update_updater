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

#include "zip_adapter.h"
#include <iostream>
#include <vector>
#include "zlib.h"

using namespace Hpackage;

namespace UpdatePatch {
ZipAdapter::ZipAdapter(UpdatePatchWriterPtr outStream, size_t offset, const PkgManager::FileInfoPtr fileInfo)
    : DeflateAdapter(), outStream_(outStream), offset_(offset)
{
    const Hpackage::ZipFileInfo *info = (const Hpackage::ZipFileInfo *)fileInfo;
    method_ = info->method;
    level_ = info->level;
    windowBits_ = info->windowBits;
    memLevel_ = info->memLevel;
    strategy_ = info->strategy;
}

int32_t ZipAdapter::Open()
{
    if (init_) {
        PATCH_LOGE("Has been open");
        return 0;
    }
    if (memset_s(&zstream_, sizeof(zstream_), 0, sizeof(z_stream)) != EOK) {
        PATCH_LOGE("Failed to memset stream");
        return -1;
    }
    PATCH_DEBUG("Open level_:%d method_:%d windowBits_:%d memLevel_:%d strategy_:%d",
        level_, method_, windowBits_, memLevel_, strategy_);
    int32_t ret = deflateInit2(&zstream_, level_, method_, windowBits_, memLevel_, strategy_);
    if (ret != Z_OK) {
        PATCH_LOGE("Failed to deflateInit2 ret %d", ret);
        return -1;
    }
    buffer_.resize(BUFFER_SIZE);
    init_ = true;
    return ret;
}

int32_t ZipAdapter::Close()
{
    if (!init_) {
        PATCH_LOGE("Has been close");
        return 0;
    }
    int32_t ret = deflateEnd(&zstream_);
    if (ret != Z_OK) {
        PATCH_LOGE("Failed to deflateEnd %d", ret);
        return -1;
    }
    init_ = false;
    return ret;
}

int32_t ZipAdapter::WriteData(const BlockBuffer &srcData)
{
    zstream_.next_in = srcData.buffer;
    zstream_.avail_in = static_cast<uint32_t>(srcData.length);
    zstream_.avail_out =  static_cast<uint32_t>(buffer_.size());
    zstream_.next_out = reinterpret_cast<unsigned char*>(buffer_.data());
    size_t deflateLen = 0;
    int32_t ret = Z_OK;
    do {
        ret = deflate(&zstream_, Z_NO_FLUSH);
        deflateLen = buffer_.size() -  zstream_.avail_out;
        if (deflateLen > 0) {
            ret = outStream_->Write(offset_, {buffer_.data(), deflateLen}, deflateLen);
            if (ret != 0) {
                PATCH_LOGE("Failed to deflate data");
                return -1;
            }
            offset_ += deflateLen;

            zstream_.next_out = reinterpret_cast<unsigned char*>(buffer_.data());
            zstream_.avail_out = buffer_.size();
        }
        if (zstream_.avail_in == 0) {
            break;
        }
    } while (ret == Z_OK);
    if (ret != Z_OK) {
        PATCH_LOGE("WriteData : Failed to write data ret %d", ret);
        return ret;
    }
    if (zstream_.avail_in != 0) {
        PATCH_LOGE("WriteData : Failed to write data");
        return ret;
    }
    return ret;
}

int32_t ZipAdapter::FlushData(size_t &offset)
{
    zstream_.next_in = nullptr;
    zstream_.avail_in = 0;
    zstream_.avail_out =  buffer_.size();
    zstream_.next_out = reinterpret_cast<unsigned char*>(buffer_.data());
    size_t deflateLen = 0;
    int32_t ret = Z_OK;
    do {
        ret = deflate(&zstream_, Z_FINISH);
        deflateLen = buffer_.size() -  zstream_.avail_out;
        if (deflateLen > 0) {
            ret = outStream_->Write(offset_, {buffer_.data(), deflateLen}, deflateLen);
            if (ret != 0) {
                PATCH_LOGE("Failed to deflate data");
                return 1;
            }
            offset_ += deflateLen;

            zstream_.next_out = reinterpret_cast<unsigned char*>(buffer_.data());
            zstream_.avail_out = buffer_.size();
        }
        if (ret == Z_STREAM_END) {
            ret = Z_OK;
            break;
        }
    } while (ret == Z_OK);
    if (ret != Z_OK) {
        PATCH_LOGE("FlushData : Failed to write data ret %d", ret);
        return ret;
    }
    if (zstream_.avail_in != 0) {
        PATCH_LOGE("FlushData : Failed to write data");
        return ret;
    }
    offset = offset_;
    return ret;
}
} // namespace UpdatePatch
