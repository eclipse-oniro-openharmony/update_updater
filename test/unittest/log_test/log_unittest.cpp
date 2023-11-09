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
#include "log_unittest.h"
#include <fstream>
#include <iostream>
#include <string>
#include "log/log.h"

using namespace testing::ext;
using namespace UpdaterUt;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
void LogUnitTest::SetUpTestCase(void)
{
    cout << "SetUpTestCase" << endl;
}

void LogUnitTest::TearDownTestCase(void)
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(LogUnitTest, log_test_001, TestSize.Level1)
{
    InitUpdaterLogger("UPDATER_UT", "", "", "");
    SetLogLevel(ERROR);
    LOG(ERROR) << "this is ut";
    STAGE(UPDATE_STAGE_BEGIN) << "this is ut";
    ERROR_CODE(CODE_VERIFY_FAIL);
    SUCCEED();

    SetLogLevel(INFO);
    LOG(ERROR) << "this is ut";
    STAGE(UPDATE_STAGE_BEGIN) << "this is ut";
    SUCCEED();

    SetLogLevel(ERROR);
    LOG(ERROR) << "this is ut";
    STAGE(UPDATE_STAGE_BEGIN) << "this is ut";
    InitUpdaterLogger("UPDATER_UT", "/data/updater/m_log.txt", "/data/updater/m_stage.txt", "/data/updater/m_code.txt");

    LOG(ERROR) << "this is ut";
    STAGE(UPDATE_STAGE_BEGIN) << "this is ut";
    ERROR_CODE(CODE_VERIFY_FAIL);
    fstream f;
    f.open("/data/updater/m_log.txt", ios::in);
    if (!f) {
        SUCCEED();
    };
    char ch[100];
    f.getline(ch, 100);
    string result = ch;
    auto ret = result.find("this is ut");
    if (ret != string::npos) {
        f.close();
        unlink("/data/updater/m_log.txt");
        unlink("/data/updater/m_stage.txt");
        EXPECT_NE(ret, string::npos);
        SUCCEED();
    } else {
        f.close();
        unlink("/data/updater/m_log.txt");
        unlink("/data/updater/m_stage.txt");
        FAIL();
    }
}
} // namespace updater_ut
