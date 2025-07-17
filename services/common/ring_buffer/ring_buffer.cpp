/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <iostream>
#include <cstring>
#include <string>
#include <thread>

#include "log.h"
#include "securec.h"
#include "ring_buffer.h"

namespace Updater {
RingBuffer::~RingBuffer()
{
    Release();
}

bool RingBuffer::Init(uint32_t singleSize, uint32_t num)
{
    if (singleSize == 0 || num == 0 || (num & (num - 1)) != 0) { // power of 2
        LOG(ERROR) << "singleSize:" <<  singleSize << " num:" << num << " error";
        return false;
    }
    bufArray_ = new (std::nothrow) uint8_t* [num] {};
    lenArray_ = new (std::nothrow) uint32_t [num] {};
    if (bufArray_ == nullptr || lenArray_ == nullptr) {
        LOG(ERROR) << "new buf or len " << num << " error";
        return false;
    }
    for (uint32_t i = 0; i < num; i++) {
        bufArray_[i] = new (std::nothrow) uint8_t [singleSize] {};
        if (bufArray_[i] == nullptr) {
            LOG(ERROR) << "new buf " << i << " size " << singleSize << " error";
            return false;
        }
    }

    writeIndex_ = 0;
    readIndex_ = 0;
    num_ = num;
    singleSize_ = singleSize;
    return true;
}

void RingBuffer::Reset()
{
    isStop_ = false;
    writeIndex_ = 0;
    readIndex_ = 0;
    for (uint32_t i = 0; i < num_; ++i) {
        lenArray_[i] = 0;
    }
}

bool RingBuffer::IsFull()
{
    // writeIndex readIndex real size: 0 ~ num_ -1, logic size: 0 ~ 2num_ - 1
    // when writeIndex_ - readIndex_ == n means full
    return writeIndex_ == (readIndex_ ^ num_);
}

bool RingBuffer::IsEmpty()
{
    // writeIndex readIndex real size: 0 ~ num_ -1, logic size: 0 ~ 2num_ - 1
    // when same means empty
    return writeIndex_ == readIndex_;
}

bool RingBuffer::Push(uint8_t *buf, uint32_t len)
{
    if (buf == nullptr || len == 0 || len > singleSize_) {
        LOG(ERROR) << "RingBuffer push error, len:" << len << " singleSize:" << singleSize_;
        return false;
    }
    if (IsFull()) {
        std::unique_lock<std::mutex> pushLock(notifyMtx_);
        while (IsFull()) {
            if (isStop_) {
                LOG(WARNING) << "RingBuffer push stopped";
                return false;
            }
            LOG(DEBUG) << "RingBuffer full, wait !!!";
            notFull_.wait(pushLock);
        }
    }

 
    uint32_t index = writeIndex_ & (num_ - 1);
    if (memcpy_s(bufArray_[index], singleSize_, buf, len) != EOK) {
        LOG(ERROR) << "memcpy error, len:" << len;
        return false;
    }
    lenArray_[index] = len;
    writeIndex_ = (writeIndex_ + 1) & (2 * num_ - 1); // 2: logic buffer size

    std::unique_lock<std::mutex> popLock(notifyMtx_);
    notEmpty_.notify_all();
    return true;
}

bool RingBuffer::Pop(uint8_t *buf, uint32_t maxLen, uint32_t &len)
{
    if (buf == nullptr) {
        LOG(ERROR) << "RingBuffer pop para error";
        return false;
    }
    if (IsEmpty()) {
        std::unique_lock<std::mutex> popLock(notifyMtx_);
        while (IsEmpty()) {
            if (isStop_) {
                LOG(WARNING) << "RingBuffer pop stopped";
                return false;
            }
            LOG(DEBUG) << "RingBuffer empty, wait !!!";
            notEmpty_.wait(popLock);
        }
    }

    uint32_t index = readIndex_ & (num_ - 1);
    if (memcpy_s(buf, maxLen, bufArray_[index], lenArray_[index]) != EOK) {
        LOG(ERROR) << "memcpy error, len:" << lenArray_[index];
        return false;
    }
    len = lenArray_[index];
    readIndex_ = (readIndex_ + 1) & (2 * num_ - 1); // 2: logic buffer size

    std::unique_lock<std::mutex> popLock(notifyMtx_);
    notFull_.notify_all();
    return true;
}

void RingBuffer::Stop()
{
    isStop_ = true;
    notFull_.notify_all();
    notEmpty_.notify_all();
}

void RingBuffer::StopPush()
{
    {
        std::unique_lock<std::mutex> pushLock(notifyMtx_);
        isStop_ = true;
    }
    notFull_.notify_all();
}

void RingBuffer::StopPop()
{
    {
        std::unique_lock<std::mutex> popLock(notifyMtx_);
        isStop_ = true;
    }
    notEmpty_.notify_all();
}

void RingBuffer::Release()
{
    if (lenArray_ != nullptr) {
        delete[] lenArray_;
        lenArray_ = nullptr;
    }

    if (bufArray_ != nullptr) {
        for (uint32_t i = 0; i < num_ && bufArray_[i] != nullptr; i++) {
            delete[] bufArray_[i];
            bufArray_[i] = nullptr;
        }
        delete[] bufArray_;
        bufArray_ = nullptr;
    }
}
} // namespace Updater
