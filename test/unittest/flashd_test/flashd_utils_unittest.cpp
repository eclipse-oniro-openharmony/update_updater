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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <sys/ioctl.h>
#include "securec.h"
#include "daemon/flashd_utils.h"
#include "updater/updater.h"
#include "updater/updater_const.h"

using namespace std;
using namespace Flashd;
using namespace Updater;
using namespace testing::ext;

namespace {

class FLashServiceUnitTest : public testing::Test {
public:
    FLashServiceUnitTest()
    {
        std::cout<<"FLashServiceUnitTest()";
    }
    ~FLashServiceUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(FLashServiceUnitTest, Split, TestSize.Level1)
{
    std::string input = "";
    std::vector<std::string> output = Split(input, { "-f" });
    std::string res = "";
    for (auto s : output) {
        res += s;
    }
    EXPECT_EQ("", res);
    input = "flash updater updater.img";
    output = Split(input, { "-f" });
    res = "";
    for (auto s : output) {
        res += s;
    }
    EXPECT_EQ("flashupdaterupdater.img", res);
}

HWTEST_F(FLashServiceUnitTest, GetPathRootTest, TestSize.Level1)
{
    std::string path = "/";
    std::string root = GetPathRoot(path);
    EXPECT_EQ("/", root);

    path = "";
    root = GetPathRoot(path);
    EXPECT_EQ("", root);

    path = "/usr/local/bin";
    root = GetPathRoot(path);
    EXPECT_EQ("/usr", root);
}

HWTEST_F(FLashServiceUnitTest, GetFileNameTest, TestSize.Level1)
{
    std::string testStr = "data/test/test.zip";
    std::string res = GetFileName(testStr);
    EXPECT_EQ("test.zip", res);

    testStr = "D:\\test\\test.zip";
    res = GetFileName(testStr);
    EXPECT_EQ("test.zip", res);

    testStr = "test.zip";
    res = GetFileName(testStr);
    EXPECT_EQ("", res);
}
} // namespace