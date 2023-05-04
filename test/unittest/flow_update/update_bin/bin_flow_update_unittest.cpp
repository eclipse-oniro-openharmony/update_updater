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

#include "bin_flow_update.h"

using namespace testing::ext;
using namespace Updater;

namespace OHOS {
constexpr const char *PKG_PATH = "/data/updater/package/update.bin";
constexpr  int MAX_LOG_BUF_SIZE = 1 * 1024 * 1024;
constexpr  int BUF_SIZE = 4 * 1024 * 1024;
class BinFlowUpdateTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(BinFlowUpdateTest, binFlowUpdateTest01, TestSize.Level0)
{
    std::cout << "binFlowUpdateTest01 start\n";
    FILE* fp = fopen(PKG_PATH, "rb");
    if (fp == nullptr) {
        std::cout << "fopen /data/updater/package/update.bin failed" << " : " << strerror(errno);
    }
    EXPECT_NE(fp, nullptr);

    uint8_t buf[MAX_LOG_BUF_SIZE] {};
    size_t len;
    int ret = 0;
    BinFlowUpdate binFlowUpdate(BUF_SIZE);
    while ((len = fread(buf, 1, sizeof(buf), fp)) != 0) {
        ret = binFlowUpdate.StartBinFlowUpdate(buf, len);
        if (ret != 0) {
            break;
        }
    }

    EXPECT_EQ(ret, 0);
    std::cout << "binFlowUpdateTest01 end\n";
}
}  // namespace OHOS
