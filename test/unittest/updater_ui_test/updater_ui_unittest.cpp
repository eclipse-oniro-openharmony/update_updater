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

#include "updater_ui_unittest.h"
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include "log/log.h"
#include "securec.h"
#include "updater_ui.h"
#include "text_label.h"
#include "frame.h"
#include "surface_dev.h"
#include "utils.h"
#include "input_event.h"

using namespace updater;
using namespace testing::ext;
using namespace std;
using namespace updater::utils;
static constexpr int EV_VALUE_15 = 15;
static constexpr int EV_VALUE_80 = 80;
static constexpr int EV_VALUE_500 = 500;

namespace updater_ut {
// do something at the each function begining
void UpdaterUiUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdaterUiUnitTest Begin!" << endl;
}

// do something at the each function end
void UpdaterUiUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdaterUiUnitTest End!" << endl;
}

// init
void UpdaterUiUnitTest::SetUpTestCase(void)
{
    cout << "SetUpTestCase" << endl;
}

// end
void UpdaterUiUnitTest::TearDownTestCase(void)
{
    cout << "TearDownTestCase" << endl;
    unlink("/data/updater.log");
    unlink("/data/updater_stage.log");
}

HWTEST_F(UpdaterUiUnitTest, updater_ui_test_HandleInputEvent, TestSize.Level1)
{
    UpdaterUiInit();
    struct input_event ev {};
    ev.value = EV_VALUE_15;
    HandleInputEvent(&ev);
    ev.value = EV_VALUE_80;
    HandleInputEvent(&ev);
    ev.value = EV_VALUE_500;
    HandleInputEvent(&ev);
    DeleteView();
}
} // namespace updater_ut
