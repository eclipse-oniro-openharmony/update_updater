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
#include <iostream>
#include "fs_manager/mount.h"
#include "misc_info/misc_info.h"
#include "securec.h"
#include "updater/updater_const.h"

using namespace std;
using namespace Updater;

static void PrintPrompts()
{
    cout << "Please input correct command, examples :" << endl;
    cout << "updater       :  write_updater updater /data/updater/updater.zip" << endl;
    cout << "factory_reset :  write_updater user_factory_reset" << endl;
    cout << "sdcard_update :  write_updater sdcard_update" << endl;
    cout << "clear command :  write_updater clear" << endl;
    cout << "updater_para  : write_updater updater_para zh-Hans" << endl;
}

static int ExceptionUpdater(int argc, char **argv, UpdateMessage &boot)
{
    if (argc < BINARY_MAX_ARGS) {
        cout << "Please input correct updater command!" << endl;
        return -1;
    }
    if (argv[WRITE_SECOND_CMD] != nullptr) {
        if (snprintf_s(boot.update, sizeof(boot.update), sizeof(boot.update) - 1, "--update_package=%s",
            argv[WRITE_SECOND_CMD]) == -1) {
            cout << "WriteUpdaterMessage snprintf_s failed!" << endl;
            return -1;
        }
    }
    return 0;
}

static int ExceptionWriteUpdaterPara(int argc, char **argv, UpdaterPara &para)
{
    if (argc != 3) { // 3 : Less than 3 is an invalid input
        cout << "please input correct updater command!" << endl;
        return -1;
    }
    if (strncpy_s(para.language, sizeof(para.language), argv[2], sizeof(para.language) -1) != 0) { // 2 : argv 2
        cout << "strncpy_s failed!" << endl;
        return -1;
    }
    if (!WriteUpdaterParaMisc(para)) {
        cout << "WriteUpdaterParaMisc failed!" << endl;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        PrintPrompts();
        return -1;
    }

    const std::string miscFile = "/dev/block/by-name/misc";
    struct UpdateMessage boot {};
    struct UpdaterPara para {};
    if (strcmp(argv[1], "updater") == 0) {
        if (ExceptionUpdater(argc, argv, boot) == -1) {
            return -1;
        }
    } else if (strcmp(argv[1], "user_factory_reset") == 0) {
        if (strncpy_s(boot.update, sizeof(boot.update), "--user_wipe_data", sizeof(boot.update) - 1) != 0) {
            cout << "strncpy_s failed!" << endl;
            return -1;
        }
    } else if (strcmp(argv[1], "boot_flash") == 0) {
        if (strncpy_s(boot.update, sizeof(boot.update), "boot_flash", sizeof(boot.update) - 1) != 0) {
            cout << "strncpy_s failed!" << endl;
            return -1;
        }
    } else if (strcmp(argv[1], "clear") == 0) {
        cout << "clear misc" << endl;
    } else if (strcmp(argv[1], "sdcard_update") == 0) {
        if (strncpy_s(boot.update, sizeof(boot.update), "--sdcard_update", sizeof(boot.update) - 1) != 0) {
            cout << "strncpy_s failed!" << endl;
            return -1;
        }
    } else if (strcmp(argv[1], "updater_para") == 0) {
        if (ExceptionWriteUpdaterPara(argc, argv, para) == -1) {
            return -1;
        }
        return 0;
    } else {
        cout << "Please input correct command!" << endl;
        return -1;
    }
    bool ret = WriteUpdaterMessage(miscFile, boot);
    if (!ret) {
        cout << "WriteUpdaterMessage failed!" << endl;
        return -1;
    }
    return 0;
}
