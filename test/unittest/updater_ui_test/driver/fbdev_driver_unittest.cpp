/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "gtest/gtest.h"
#include "fbdev_driver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include "securec.h"
#include "ui_rotation.h"
#include "updater_ui_const.h"

using namespace testing::ext;
using namespace Updater;

namespace UpdaterUt {
class FbdevDriverUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(FbdevDriverUnitTest, test_fbdev_driver_init, TestSize.Level0)
{
    auto drv = std::make_unique<FbdevDriver>();
    drv->SetDevPath("");
    EXPECT_FALSE(drv->Init());
    drv->SetDevPath("/fakepath/fakefb0");
    EXPECT_FALSE(drv->Init());
    drv->SetDevPath(FB_DEV_PATH);
    EXPECT_TRUE(drv->Init());
}
}