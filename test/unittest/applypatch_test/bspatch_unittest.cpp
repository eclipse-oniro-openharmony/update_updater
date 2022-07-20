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

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include "applypatch/transfer_manager.h"
#include "log/log.h"

using namespace testing::ext;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
class BspatchUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown();
};

void BspatchUnitTest::SetUpTestCase()
{
    cout << "Updater Unit BspatchUnitTest Setup!" << endl;
}

void BspatchUnitTest::SetUp()
{
    cout << "Updater Unit BspatchUnitTest Begin!" << endl;
}

void BspatchUnitTest::TearDown()
{
    cout << "Updater Unit BspatchUnitTest End!" << endl;
}

HWTEST_F(BspatchUnitTest, bspatch_test_001, TestSize.Level1)
{
    std::string partitionName;
    std::string transferName;
    std::string newDataName;
    std::string patchDataName;
}
} // updater_ut
