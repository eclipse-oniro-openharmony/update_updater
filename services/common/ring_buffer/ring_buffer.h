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
#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <condition_variable>
#include <mutex>

#include "macros.h"

namespace Updater {
// for singleProducer singleConsumer
class RingBuffer {
    DISALLOW_COPY_MOVE(RingBuffer);
public:
    RingBuffer() = default;
    virtual ~RingBuffer();
    [[nodiscard]] bool Init(uint32_t singleSize, uint32_t num);
    bool Push(uint8_t *buf, uint32_t len);
    bool Pop(uint8_t *buf, uint32_t maxLen, uint32_t &len);
    void Stop();
private:
    inline bool IsFull();
    inline bool IsEmpty();
    void Release();

    uint32_t writeIndex_ = 0; // Producer
    uint32_t readIndex_ = 0; // Consumer
    uint32_t singleSize_ = 0; // single buf size
    uint32_t num_ = 0;  // buffer num
    uint8_t **bufArray_ = nullptr;
    uint32_t *lenArray_ = nullptr;
    bool isStop = false;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    std::mutex notifyMtx_;
};
} // namespace Updater
#endif // __RING_BUFFER_H__