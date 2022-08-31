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
#include "fs_manager/mount.h"
#include "flashd/flashd.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "updater/updater_const.h"
#include "updater_main.h"
#include "utils.h"

using namespace Updater;

int main(int argc, char **argv)
{
    int mode = BOOT_UPDATER;
    int ret = GetBootMode(mode);
    if (ret != 0) {
        printf("Failed to get boot mode, start updater mode \n");
        mode = BOOT_UPDATER;
    }

    std::string modeStr = (mode == BOOT_FLASHD) ? "FLASHD" : "UPDATER";
    InitUpdaterLogger(modeStr, TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    SetLogLevel(INFO);
    LoadFstab();
    STAGE(UPDATE_STAGE_OUT) << "Start " << ((mode == BOOT_FLASHD) ? "flashd" : "updater");
    auto modePara = (mode == BOOT_FLASHD) ? "updater.flashd.configfs" : "updater.hdc.configfs";
    Flashd::SetParameter(modePara, "1");

    if (mode == BOOT_FLASHD) {
        Utils::UsSleep(3000 * 1000); // 3000 * 1000 : wait 3s
        return Flashd::flashd_main(argc, argv);
    }
    return Updater::UpdaterMain(argc, argv);
}
