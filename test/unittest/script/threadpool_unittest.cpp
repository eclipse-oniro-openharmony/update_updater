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

#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "pkg_algorithm.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "script_utils.h"
#include "thread_pool.h"
#include "unittest_comm.h"

using namespace std;
using namespace Hpackage;
using namespace Uscript;
using namespace Updater;
using namespace testing::ext;

namespace {
const int32_t MAX_TASK_NUMBER = 5;

class ThreadPoolUnitTest : public ::testing::Test {
public:
    ThreadPoolUnitTest() : threadPool_(ThreadPool::CreateThreadPool(MAX_TASK_NUMBER)) {}

    ~ThreadPoolUnitTest()
    {
        ThreadPool::Destroy();
        threadPool_ = nullptr;
    }

    int TestThreadPoolCreate(const int32_t taskNumber)
    {
        USCRIPT_CHECK(threadPool_ != nullptr, return USCRIPT_INVALID_PARAM, "Fail to create thread pool");
        Task task;
        int32_t ret = USCRIPT_SUCCESS;
        size_t taskNumberRound = 20;
        task.workSize = taskNumber;
        task.processor = [&](int iter) {
            for (size_t i = iter; i < taskNumberRound; i += taskNumber) {
                LOG(INFO) << "Run thread " << i << " " << gettid();
            }
        };
        ThreadPool::AddTask(std::move(task));
        return ret;
    }

protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

private:
    ThreadPool* threadPool_;
};

HWTEST_F(ThreadPoolUnitTest, TestThreadPoolCreate, TestSize.Level0)
{
    ThreadPoolUnitTest test;
    for (size_t i = 0; i < MAX_TASK_NUMBER * 2; i++) {
        EXPECT_EQ(0, test.TestThreadPoolCreate(MAX_TASK_NUMBER + 2));
    }
}

HWTEST_F(ThreadPoolUnitTest, TestThreadOneCreate, TestSize.Level0)
{
    ThreadPoolUnitTest test;
    EXPECT_EQ(0, test.TestThreadPoolCreate(1));
}
}
