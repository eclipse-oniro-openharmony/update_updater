/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include "fs_manager/mount.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "updater_main.h"
#include "utils.h"

using namespace Updater;

int main(int argc, char **argv)
{
    // prepare modes vector by macro DEFINE_MODE which subscribe UPDATER_MAIN_PRE_EVENT event
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_MAIN_PRE_EVENT);

    struct UpdateMessage boot {};
    if (!ReadUpdaterMiscMsg(boot)) {
        // read misc failed, default enter updater mode
        boot = {"boot_updater", "", ""};
    }

    // select modes by bootMode.cond which would check misc message
    auto bootMode = SelectMode(boot).value_or(BOOT_MODE(Updater, "updater.hdc.configfs"));

    // execute mode initialization
    bootMode.InitMode();

    LOG(INFO) << "################################";
    // mode entry
    return bootMode.entryFunc(argc, argv);
}
