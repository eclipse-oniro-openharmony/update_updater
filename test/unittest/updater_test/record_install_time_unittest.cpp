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
#include <gtest/gtest.h>
#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "updater_main.h"
#include <thread>

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
class RecordInstallTimeUnittest : public testing::Test {
public:
    RecordInstallTimeUnittest()
    {
        InitUpdaterLogger("UPDATER", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    }
    ~RecordInstallTimeUnittest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(RecordInstallTimeUnittest, UpdaterRecordInstallTime, TestSize.Level1)
{
    UpdaterParams params;
    params.installTime.resize(3);  // 3:number of upgrade packages
    for (params.pkgLocation = 0; params.pkgLocation < params.installTime.size(); params.pkgLocation++) {
        auto startTime = std::chrono::system_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(params.pkgLocation));
        auto endTime = std::chrono::system_clock::now();
        params.installTime[params.pkgLocation] = endTime - startTime;
        WriteInstallTime(params);
    }
    UpdaterParams newParams;
    newParams.installTime.resize(params.installTime.size());
    newParams.pkgLocation = params.pkgLocation;
    ReadInstallTime(newParams);
    for (int i = 0; i < params.installTime.size(); i++) {
        EXPECT_EQ(
            Utils::DurationToString(params.installTime, i, 2), // 2:precision
            Utils::DurationToString(params.installTime, i, 2)); // 2:precision
    }
}

HWTEST_F(RecordInstallTimeUnittest, UpdaterTestIsDouble, TestSize.Level1)
{
    std::string installTime = "12.11";
    std::string exceptionData = "12.11.11";
    EXPECT_EQ(IsDouble(installTime), true);
    EXPECT_EQ(IsDouble(exceptionData), false);
}
}  // namespace