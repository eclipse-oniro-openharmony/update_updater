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
#include "pkg_stream.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include "dump.h"
#include "pkg_manager.h"
#include "pkg_utils.h"
#include "securec.h"

#ifdef __APPLE__
#define off64_t off_t
#define fopen64 fopen
#define ftello64 ftello
#define fseeko64 fseek
#endif

namespace Hpackage {
const std::string PkgStreamImpl::GetFileName() const
{
    return fileName_;
}

PkgStreamPtr PkgStreamImpl::ConvertPkgStream(PkgManager::StreamPtr stream)
{
    return (PkgStreamPtr)stream;
}

void PkgStreamImpl::AddRef()
{
    refCount_++;
}

void PkgStreamImpl::DelRef()
{
    refCount_--;
}

bool PkgStreamImpl::IsRef() const
{
    return refCount_ == 0;
}

void PkgStreamImpl::PostDecodeProgress(int type, size_t writeDataLen, const void *context) const
{
    if (pkgManager_ != nullptr) {
        pkgManager_->PostDecodeProgress(type, writeDataLen, context);
    }
}

FileStream::~FileStream()
{
    if (stream_ != nullptr) {
        fflush(stream_);
        fclose(stream_);
        stream_ = nullptr;
    }
}

int32_t FileStream::Read(PkgBuffer &data, size_t start, size_t needRead, size_t &readLen)
{
    Updater::UPDATER_INIT_RECORD;
    if (stream_ == nullptr) {
        PKG_LOGE("Invalid stream");
        UPDATER_LAST_WORD(PKG_INVALID_STREAM);
        return PKG_INVALID_STREAM;
    }
    if (data.length < needRead) {
        PKG_LOGE("insufficient buffer capacity");
        UPDATER_LAST_WORD(PKG_INVALID_STREAM);
        return PKG_INVALID_STREAM;
    }
    readLen = 0;
    if (fseeko64(stream_, start, SEEK_SET) != 0) {
        PKG_LOGE("read data fail");
        UPDATER_LAST_WORD(PKG_INVALID_STREAM);
        return PKG_INVALID_STREAM;
    }
    if (start > GetFileLength()) {
        PKG_LOGE("Invalid start");
        UPDATER_LAST_WORD(PKG_INVALID_STREAM);
        return PKG_INVALID_STREAM;
    }
    if (data.buffer == nullptr) {
        data.data.resize(data.length);
        data.buffer = data.data.data();
    }
    readLen = fread(data.buffer, 1, needRead, stream_);
    if (readLen == 0) {
        PKG_LOGE("read data fail");
        UPDATER_LAST_WORD(PKG_INVALID_STREAM);
        return PKG_INVALID_STREAM;
    }
    return PKG_SUCCESS;
}

int32_t FileStream::Write(const PkgBuffer &data, size_t size, size_t start)
{
    if (streamType_ != PkgStreamType_Write) {
        PKG_LOGE("Invalid stream type");
        return PKG_INVALID_STREAM;
    }
    if (stream_ == nullptr) {
        PKG_LOGE("Invalid stream");
        return PKG_INVALID_STREAM;
    }
    if (fseeko64(stream_, start, SEEK_SET) != 0) {
        PKG_LOGE("write data fail");
        return PKG_INVALID_STREAM;
    }
    size_t len = fwrite(data.buffer, size, 1, stream_);
    if (len != 1) {
        PKG_LOGE("Write buffer fail");
        return PKG_INVALID_STREAM;
    }
    PostDecodeProgress(POST_TYPE_DECODE_PKG, size, nullptr);
    return PKG_SUCCESS;
}

size_t FileStream::GetFileLength()
{
    if (stream_ == nullptr) {
        PKG_LOGE("Invalid stream");
        return 0;
    }
    if (fileLength_ == 0) {
        if (Seek(0, SEEK_END) != 0) {
            PKG_LOGE("Invalid stream");
            return 0;
        }
        off64_t ret = ftello64(stream_);
        if (ret < 0) {
            PKG_LOGE("ftell64 failed");
            return 0;
        }
        fileLength_ = static_cast<size_t>(ret);
        if (fseek(stream_, 0, SEEK_SET) != 0) {
            PKG_LOGE("fseek failed");
            return 0;
        }
    }
    return fileLength_;
}

int32_t FileStream::Seek(long int offset, int whence)
{
    if (stream_ == nullptr) {
        PKG_LOGE("Invalid stream");
        return PKG_INVALID_STREAM;
    }
    return fseek(stream_, offset, whence);
}

int32_t FileStream::Flush(size_t size)
{
    if (stream_ == nullptr) {
        PKG_LOGE("Invalid stream");
        return PKG_INVALID_STREAM;
    }
    if (fileLength_ == 0) {
        fileLength_ = size;
    }
    if (fseek(stream_, 0, SEEK_END) != 0) {
        PKG_LOGE("fseek failed");
        return PKG_INVALID_STREAM;
    }
    off64_t ret = ftello64(stream_);
    if (ret < 0) {
        PKG_LOGE("ftell64 failed");
        return PKG_INVALID_STREAM;
    }
    fileLength_ = static_cast<size_t>(ret);
    if (size != fileLength_) {
        PKG_LOGE("Flush size %zu local size:%zu", size, fileLength_);
    }
    if (fflush(stream_) != 0) {
        PKG_LOGE("Invalid stream");
        return PKG_INVALID_STREAM;
    }
    return PKG_SUCCESS;
}

MemoryMapStream::~MemoryMapStream()
{
    if (memMap_ == nullptr) {
        PKG_LOGE("Invalid memory map");
        return;
    }
    if (streamType_ == PkgStreamType_MemoryMap) {
        ReleaseMemory(memMap_, memSize_);
    }
}

int32_t MemoryMapStream::Read(PkgBuffer &data, size_t start, size_t needRead, size_t &readLen)
{
    if (memMap_ == nullptr) {
        PKG_LOGE("Invalid memory map");
        return PKG_INVALID_STREAM;
    }
    if (start > memSize_) {
        PKG_LOGE("Invalid start");
        return PKG_INVALID_STREAM;
    }
    if (data.length < needRead) {
        PKG_LOGE("insufficient buffer capacity");
        return PKG_INVALID_STREAM;
    }
    size_t copyLen = GetFileLength() - start;
    readLen = ((copyLen > needRead) ? needRead : copyLen);
    if (data.data.size() == 0) {
        data.buffer = memMap_ + start;
    } else {
        if (memcpy_s(data.buffer, needRead, memMap_ + start, readLen) != EOK) {
            PKG_LOGE("Memcpy failed size:%zu, start:%zu copyLen:%zu %zu", needRead, start, copyLen, readLen);
            return PKG_NONE_MEMORY;
        }
    }
    return PKG_SUCCESS;
}

int32_t MemoryMapStream::Write(const PkgBuffer &data, size_t size, size_t start)
{
    if (memMap_ == nullptr) {
        PKG_LOGE("Invalid memory map");
        return PKG_INVALID_STREAM;
    }
    if (start > memSize_) {
        PKG_LOGE("Invalid start");
        return PKG_INVALID_STREAM;
    }

    currOffset_ = start;
    size_t copyLen = memSize_ - start;
    if (copyLen < size) {
        PKG_LOGE("Write fail copyLen %zu, %zu", copyLen, size);
        return PKG_INVALID_STREAM;
    }
    int32_t ret = memcpy_s(memMap_ + currOffset_, memSize_ - currOffset_, data.buffer, size);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Write fail");
        return PKG_INVALID_STREAM;
    }
    PostDecodeProgress(POST_TYPE_DECODE_PKG, size, nullptr);
    return PKG_SUCCESS;
}

int32_t MemoryMapStream::Seek(long int offset, int whence)
{
    if (whence == SEEK_SET) {
        if (offset < 0) {
            PKG_LOGE("Invalid offset");
            return PKG_INVALID_STREAM;
        }
        if (static_cast<size_t>(offset) > memSize_) {
            PKG_LOGE("Invalid offset");
            return PKG_INVALID_STREAM;
        }
        currOffset_ = static_cast<size_t>(offset);
    } else if (whence == SEEK_CUR) {
        if (static_cast<size_t>(offset) > (memSize_ - currOffset_)) {
            PKG_LOGE("Invalid offset");
            return PKG_INVALID_STREAM;
        }
        currOffset_ += static_cast<size_t>(offset);
    } else {
        if (offset > 0) {
            PKG_LOGE("Invalid offset");
            return PKG_INVALID_STREAM;
        }
        auto memSize = static_cast<long long>(memSize_);
        if (memSize + offset < 0) {
            PKG_LOGE("Invalid offset");
            return PKG_INVALID_STREAM;
        }
        currOffset_ = static_cast<size_t>(memSize + offset);
    }
    return PKG_SUCCESS;
}

int32_t FlowDataStream::Read(PkgBuffer &data, size_t start, size_t needRead, size_t &readLen)
{
    if (readOffset_ != start) {
        PKG_LOGE("Invalid start, readOffset_: %d, start: %d", readOffset_, start);
        return PKG_INVALID_STREAM;
    }

    if (data.length < needRead) {
        PKG_LOGE("Invalid need length");
        return PKG_INVALID_STREAM;
    }

    if (data.buffer == nullptr) {
        data.data.resize(needRead);
        data.buffer = data.data.data();
    }

    readLen = 0;
    uint8_t *buffer = nullptr;
    while (needRead - readLen > 0) {
        uint32_t readOnce = 0;
        if (ReadFromRingBuf(buffer, needRead - readLen, readOnce) != PKG_SUCCESS) {
            PKG_LOGE("Fail to read header");
            return PKG_INVALID_STREAM;
        }
        if (buffer == nullptr || readOnce == 0) {
            PKG_LOGE("Fail to read header, readOnce: %d", readOnce);
            return PKG_INVALID_STREAM;
        }
        if (memcpy_s(data.buffer + readLen, readOnce, buffer, readOnce) != EOK) {
            PKG_LOGE("Memcpy failed size:%zu, copyLen:%zu", needRead, readOnce);
            return PKG_NONE_MEMORY;
        }
        readLen += readOnce;
    }
    readOffset_ += needRead;
    return PKG_SUCCESS;
}

int32_t FlowDataStream::ReadFromRingBuf(uint8_t *&buff, const uint32_t needLen, uint32_t &readLen)
{
    if (ringBuf_ == nullptr) {
        PKG_LOGE("ringBuf_ is nullptr");
        buff = nullptr;
        return PKG_INVALID_STREAM;
    }

    // buf_ is empty, read from ringbuf
    if ((avail_ == 0) && !ringBuf_->Pop(buff_, MAX_FLOW_BUFFER_SIZE, avail_)) {
        PKG_LOGE("read data fail");
        buff = nullptr;
        return PKG_INVALID_STREAM;
    }

    buff = buff_ + bufOffset_;
    readLen = needLen <= avail_ ? needLen : avail_;
    avail_ -= readLen;
    bufOffset_ = avail_ == 0 ? 0 : bufOffset_ + readLen;
    return PKG_SUCCESS;
}

int32_t FlowDataStream::Write(const PkgBuffer &data, size_t size, size_t start)
{
    if (ringBuf_ == nullptr) {
        PKG_LOGE("ringBuf_ is nullptr");
        return PKG_INVALID_STREAM;
    }

    if (writeOffset_ != start) {
        PKG_LOGE("Invalid start, writeOffset: %zu, start: %zu", writeOffset_, start);
        return PKG_INVALID_STREAM;
    }

    if (ringBuf_->Push(data.buffer, size)) {
        writeOffset_ += size;
        return PKG_SUCCESS;
    }
    PKG_LOGE("Write ring buffer fail");
    return PKG_INVALID_STREAM;
}

void FlowDataStream::Stop()
{
    PKG_LOGI("FlowDataStream stop");
    if (ringBuf_ != nullptr) {
        ringBuf_->Stop();
    }
}
} // namespace Hpackage
