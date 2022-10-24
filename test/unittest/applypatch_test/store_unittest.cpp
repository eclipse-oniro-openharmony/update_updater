/*
* Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "store_unittest.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "applypatch/block_set.h"
#include "applypatch/store.h"
#include "log/log.h"
#include "utils.h"

using namespace testing::ext;
using namespace UpdaterUt;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
void StoreUnitTest::SetUp()
{
    cout << "SetUpTestCase" << endl;
}

void StoreUnitTest::TearDown()
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(StoreUnitTest, store_test_001, TestSize.Level1)
{
    std::string storePath = "/data/updater/ut_test";
    Store::CreateNewSpace(storePath, true);
    std::vector<uint8_t> buffer(4096, 0);
    std::string filename1 = "test_file1";
    std::string filename2 = "test_file2";
    Store::WriteDataToStore(storePath, filename1, buffer, 4096);
    Store::WriteDataToStore(storePath, filename2, buffer, 4096);
    Store::LoadDataFromStore(storePath, filename1, buffer);
    Store::DoFreeSpace(storePath);
    Store::WriteDataToStore(storePath, filename1, buffer, 4096);
    Store::WriteDataToStore(storePath, filename2, buffer, 4096);
    Store::FreeStore(storePath, filename2);
    EXPECT_EQ(Store::CreateNewSpace(storePath, true), 0);
}

HWTEST_F(StoreUnitTest, store_test_002, TestSize.Level1)
{
    std::vector<uint8_t> buffer(4096, 0);
    EXPECT_EQ(Store::WriteDataToStore("", "test_file1", buffer, buffer.size()), -1);
}

HWTEST_F(StoreUnitTest, store_test_003, TestSize.Level1)
{
    std::string storePath = "data/updater/ut_test";
    std::vector<uint8_t> buffer(4096, 0);
    std::string filename1 = "test_file1";
    EXPECT_EQ(Store::FreeStore("", filename1), -1);
    Store::CreateNewSpace(storePath, true);
    EXPECT_EQ(Store::WriteDataToStore(storePath, filename1, buffer, -1), -1);
}
}
