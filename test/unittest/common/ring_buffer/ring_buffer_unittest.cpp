/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <thread>

#include "ring_buffer.h"

using namespace testing::ext;
using namespace Updater;

namespace OHOS {
constexpr uint32_t RING_MAX_LEN = 10240;
constexpr uint32_t BYTE_SIZE = 255;
uint32_t g_num;
bool g_result = true;
class RingBufferTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

void ProducerTask(RingBuffer *ringBuffer)
{
    std::cout << "ring buffer ProducerTask start\n";
    for (uint32_t i = 0; i < RING_MAX_LEN; i++) {
        uint8_t buf[4] {}; // 4: test buffer size
        buf[0] = i % BYTE_SIZE;
        buf[1] = i / BYTE_SIZE;
        ringBuffer->Push(buf, sizeof(buf));
    }
}

void ConsumerTask(RingBuffer *ringBuffer)
{
    std::cout << "ring buffer ConsumerTask start\n";
    while (1) {
        uint8_t buf[4] {}; // 4: test buffer size
        uint32_t len = 0;
        ringBuffer->Pop(buf, sizeof(buf), len);
        if (buf[0] != g_num % BYTE_SIZE || buf[1] != g_num / BYTE_SIZE) {
            g_result = false;
        }
        g_num++;
        if (g_num == RING_MAX_LEN) {
            break;
        }
    }
}

HWTEST_F(RingBufferTest, ringBufferTest01, TestSize.Level0)
{
    std::cout << "ringBufferTest01 start\n";
    g_num = 0;
    RingBuffer ringBuffer;
    bool ret = ringBuffer.Init(1024, 8);
    EXPECT_TRUE(ret);
    std::thread consumer(ConsumerTask, &ringBuffer);
    std::thread producer(ProducerTask, &ringBuffer);
    consumer.join();
    producer.join();
    EXPECT_EQ(g_num, RING_MAX_LEN);
    std::cout << "ringBufferTest01 end\n";
}


HWTEST_F(RingBufferTest, ringBufferTest02, TestSize.Level0)
{
    std::cout << "ringBufferTest01 start";
    RingBuffer ringBuffer;
    bool ret = ringBuffer.Init(1024, 3);
    EXPECT_FALSE(ret);
    EXPECT_TRUE(g_result);
    std::cout << "ringBufferTest02 end";
}
}  // namespace OHOS
