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
#include "ptable_manager.h"
#include "ptable.h"
#include "ptable_process.h"

using namespace Updater;
using namespace testing;
using namespace testing::ext;

namespace {
class UTestPtableProcess : public ::testing::Test {
public:
    UTestPtableProcess() {}

    ~UTestPtableProcess() = default;

    void TestPtableProcess()
    {
        UpdaterParams upParams;
        bool ret = PtableProcess(upParams);
        ASSERT_EQ(ret, true);
        std::string path = "data/updater/ptable_parse/updater.zip";
        upParams.updatePackage.push_back(path);
        ret = PtableProcess(upParams);
        ASSERT_EQ(ret, true);
    }
protected:
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};


HWTEST_F(UTestPtableProcess, TestPtableProcess, TestSize.Level1)
{
    UTestPtableProcess{}.TestPtableProcess();
}
}