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
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "updater/updater_const.h"
#include "updater_main.h"

using namespace updater;

int main(int argc, char **argv)
{
    struct UpdateMessage boot {};
    // read from misc
    bool ret = ReadUpdaterMessage(MISC_FILE, boot);
    if (!ret) {
        printf("ReadUpdaterMessage MISC_FILE failed!\n");
    }
    // if boot.update is empty, read from command.The Misc partition may have dirty data,
    // so strlen(boot.update) is not used, which can cause system exceptions.
    if (boot.update[0] == '\0' && !access(COMMAND_FILE.c_str(), 0)) {
        ret = ReadUpdaterMessage(COMMAND_FILE, boot);
        if (!ret) {
            printf("ReadUpdaterMessage COMMAND_FILE failed!\n");
        }
    }
    const int flashBootLength = 10;
    bool useFlash = memcmp(boot.command, "boot_flash", flashBootLength) == 0;
    InitUpdaterLogger(useFlash ? "FLASHD" : "UPDATER", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    SetLogLevel(VERBOSE);
    LoadFstab();
    STAGE(UPDATE_STAGE_OUT) << "Init Params: " << boot.update;
    LOG(INFO) << "boot.command " << boot.command;
    LOG(INFO) << "boot.update " << boot.update;
    return updater::UpdaterMain(argc, argv);
}
