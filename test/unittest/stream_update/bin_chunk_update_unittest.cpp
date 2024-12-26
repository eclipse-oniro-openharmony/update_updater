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

#include "bin_chunk_update.h"
#include "log.h"

using namespace testing::ext;
using namespace Hpackage;
using namespace Updater;

namespace OHOS {
constexpr const char *PKG_PATH = "/data/updater/package/update_stream.bin";
constexpr uint32_t BUFFER_SIZE = 50 * 1024;
class BinChunkUpdateTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

HWTEST_F(BinChunkUpdateTest, binChunkUpdateTest01, TestSize.Level0)
{
    LOG(INFO) << "binChunkUpdateTest01 start";
    uint32_t dealLen = 0;
    //错误数据
    uint8_t buffer[BUFFER_SIZE] = {0, 'a', 1, 'b', 2};
    BinChunkUpdate binChunkUpdate(2 * BUFFER_SIZE);
    UpdateResultCode ret = binChunkUpdate.StartBinChunkUpdate(buffer, 5, dealLen);
    EXPECT_EQ(ret, STREAM_UPDATE_FAILURE);
}

HWTEST_F(BinChunkUpdateTest, binChunkUpdateTest02, TestSize.Level0)
{
    LOG(INFO) << "binChunkUpdateTest02 start";
    uint32_t dealLen = 0;
    FILE* fp = fopen(PKG_PATH, "rb");
    if (fp == nullptr) {
        std::cout << "fopen /data/updater/package/update_stream.bin failed" << " : " << strerror(errno);
        ASSERT_NE(fp, nullptr) << "Failed to open file: " << PKG_PATH;
    }
    EXPECT_NE(fp, nullptr);

    uint8_t buffer[BUFFER_SIZE]{0};
    size_t len;
    int ret = 0;
    BinChunkUpdate binChunkUpdate(2 * BUFFER_SIZE);
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) != 0) {
        ret = binChunkUpdate.StartBinChunkUpdate(buffer, len, dealLen);
        
        if (ret != STREAM_UPDATE_SUCCESS) {
            break;
        }
    }

    EXPECT_EQ(ret, STREAM_UPDATE_COMPLETE);
}
}  // namespace OHOS
