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
#include <iostream>
#include <thread>

#include "log.h"
#include "ring_buffer.h"

using namespace Updater;
constexpr uint32_t RING_MAX_LEN = 512;
void ProducerTask(RingBuffer *ringBuffer)
{
    LOG(INFO) << "ring buffer ProducerTask start";
    for (uint32_t i = 0; i < RING_MAX_LEN; i++) {
        uint8_t buf[4] {};
        buf[0] = i % 255;
        buf[1] = i / 255;
        ringBuffer->Push(buf, sizeof(buf));
    }
}

void ConsumerTask(RingBuffer *ringBuffer)
{
    LOG(INFO) << "ring buffer ConsumerTask start";
    uint32_t num = 0;
    while (1) {
        uint8_t buf[4] {};
        uint32_t len = 0;
        ringBuffer->Pop(buf, sizeof(buf), len);
        num++;
        if (num == RING_MAX_LEN) {
            break;
        }
    }
}

int main(int argc, char **argv)
{
    LOG(INFO) << "ring buffer test start";
    RingBuffer ringBuffer;
    ringBuffer.Init(1024, 16);
    std::thread consumer(ConsumerTask, &ringBuffer);
    std::thread producer(ProducerTask, &ringBuffer);
    consumer.join();
    producer.join();
    return 0;
}
