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

#include <benchmark/benchmark.h>
#include <thread>
#include "gtest/gtest.h"

#include "ring_buffer.h"

using namespace testing::ext;

namespace Updater {

constexpr uint32_t RING_MAX_LEN = 1024;
constexpr uint32_t BYTE_SIZE = 255;

class UpdaterBenchmarkTest : public benchmark::Fixture {
public:
     UpdaterBenchmarkTest() = default;
     ~UpdaterBenchmarkTest() override = default;
     void SetUp(const ::benchmark::State &state) override
     {}
     void TearDown(const ::benchmark::State &state) override
     {}
};

void ProducerTask(RingBuffer *ringBuffer)
{
    for (uint32_t i = 0; i < RING_MAX_LEN; i++) {
        uint8_t buf[4] {}; // 4: test buffer size
        buf[0] = i % BYTE_SIZE;
        buf[1] = i / BYTE_SIZE;
        ringBuffer->Push(buf, sizeof(buf));
    }
}

void ConsumerTask(RingBuffer *ringBuffer)
{
    for (uint32_t i = 0; i < RING_MAX_LEN; i++) {
        uint8_t buf[4] {}; // 4: test buffer size
        uint32_t len = 0;
        ringBuffer->Pop(buf, sizeof(buf), len);
    }
}

void TestRingBuffer()
{
    RingBuffer ringBuffer;
    bool ret = ringBuffer.Init(1024, 8);
    EXPECT_TRUE(ret);
    std::thread consumer(ConsumerTask, &ringBuffer);
    std::thread producer(ProducerTask, &ringBuffer);
    consumer.join();
    producer.join();
}

BENCHMARK_F(UpdaterBenchmarkTest, TestRingBuffer)(benchmark::State &state)
{
    for (auto _ : state) {
        TestRingBuffer();
    }
}

BENCHMARK_REGISTER_F(UpdaterBenchmarkTest, TestRingBuffer)->
    Iterations(5)->Repetitions(3)->ReportAggregatesOnly();

} // namespace Updater

// Run the benchmark
BENCHMARK_MAIN();
