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
#include <dlfcn.h>
#include <iostream>
#include "fs_manager/mount.h"
#include "misc_info/misc_info.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "parameter.h"
#include "utils.h"
#include "utils_fs.h"

using namespace std;
using namespace Updater;

constexpr const char *HANDLE_MISC_LIB = "libupdater_handle_misc.z.so";
constexpr const char *NOTIFY_MISC_INFO = "NotifyWriteMiscInfo";
constexpr const char *HANDLE_MISC_LIB_PATH = "/system/lib64/libupdater_handle_misc.z.so";
constexpr const char *LIB_PATH = "/system/lib64/";

static void PrintPrompts()
{
    cout << "Please input correct command, examples :" << endl;
    cout << "bin           :  write_updater bin /data/updater/update.bin" <<endl;
    cout << "updater       :  write_updater updater /data/updater/updater.zip" << endl;
    cout << "factory_reset :  write_updater user_factory_reset" << endl;
    cout << "sdcard_update :  write_updater sdcard_update" << endl;
    cout << "clear command :  write_updater clear" << endl;
    cout << "updater_para  :  write_updater updater_para" << endl;
    cout << "intral_update :  write_updater ota_intral_update /data/updater/updater.zip" << endl;
    cout << "subpkg_update :  write_updater subpkg_update" << endl;
    cout << "notify_update :  write_updater notify_update" << endl;
}

static int ExceptionBin(int argc, char **argv, UpdateMessage &boot)
{
    if (argc < BINARY_MAX_ARGS) {
        cout << "Please input correct updater command!" << endl;
        return -1;
    }
    if (argv[WRITE_SECOND_CMD] != nullptr) {
        // 加入新字段
        if (snprintf_s(boot.update, sizeof(boot.update), sizeof(boot.update) - 1, "--update_bin=%s",
            argv[WRITE_SECOND_CMD]) == -1) {
            cout << "WriteUpdaterMessage snprintf_s failed!" << endl;
            return -1;
        }
    }
    return 0;
}

static int WriteUpdaterLanguage(UpdaterPara &para)
{
    char language[MAX_PARA_SIZE + 1] {};
    int res = GetParameter("persist.global.locale", "", language, MAX_PARA_SIZE);
    if (res <= 0) {
        cout << "Get persist.global.locale parameter failed" << endl;
        res = GetParameter("const.global.locale", "", language, MAX_PARA_SIZE);
        if (res <= 0) {
            cout << "Get const.global.locale parameter failed" << endl;
            return -1;
        }
    }
    (void)memset_s(para.language, MAX_PARA_SIZE, 0, MAX_PARA_SIZE);
    res = memcpy_s(para.language, MAX_PARA_SIZE, language, sizeof(language) - 1);
    para.language[MAX_PARA_SIZE - 1] = '\0';
    if (res != 0) {
        cout << "memcpy_s para.language failed" << endl;
        return -1;
    }
    return 0;
}

static int WriteUpdaterVersionSuffix(UpdaterPara &para)
{
    char osVersionSuffix[MAX_PARA_SIZE + 1] {};
    int res = GetParameter("const.settings.os_version_suffix", "", osVersionSuffix, MAX_PARA_SIZE);
    if (res < 0) {
        cout << "Get const.settings.os_version_suffix parameter failed" << endl;
        return -1;
    }
    (void)memset_s(para.osVersionSuffix, MAX_PARA_SIZE, 0, MAX_PARA_SIZE);
    res = memcpy_s(para.osVersionSuffix, MAX_PARA_SIZE, osVersionSuffix, sizeof(osVersionSuffix) - 1);
    para.osVersionSuffix[MAX_PARA_SIZE - 1] = '\0';
    if (res != 0) {
        cout << "memcpy_s para.osVersionSuffix failed" << endl;
        return -1;
    }
    return 0;
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

static int WriteUpdaterPara(int argc, UpdaterPara &para)
{
    if (argc != 2) { // 2 : Not 2 is an invalid input
        cout << "please input correct updater command!" << endl;
        return -1;
    }
    if (!ReadUpdaterParaMisc(para)) {
        cout << "ReadUpdaterParaMisc failed" << endl;
        return -1;
    }
    int resLanguage = WriteUpdaterLanguage(para);
    int resVersionSuffix = WriteUpdaterVersionSuffix(para);
    if (!WriteUpdaterParaMisc(para)) {
        cout << "WriteUpdaterParaMisc failed!" << endl;
        return -1;
    }
    if (resLanguage != 0 || resVersionSuffix != 0) {
        cout << "WriteUpdaterLanguage or WriteUpdaterVersionSuffix fail" << endl;
        return -1;
    }
    return 0;
}

static void HandleMiscInfo(int argc, char **argv)
{
    if (!Utils::IsFileExist(HANDLE_MISC_LIB_PATH)) {
        return;
    }
    auto handle = Utils::LoadLibrary(HANDLE_MISC_LIB, LIB_PATH);
    if (handle == nullptr) {
        cout << "load libupdater_handle_misc fail";
        return;
    }
    auto getFunc = (void(*)(int, char **))Utils::GetFunction(handle, NOTIFY_MISC_INFO);
    if (getFunc == nullptr) {
        cout << "getFunc is nullptr";
        Utils::CloseLibrary(handle);
        return;
    }
    getFunc(argc, argv);
    Utils::CloseLibrary(handle);
}

static int HandleCommand(int argc, char** argv, struct UpdateMessage& boot, struct UpdaterPara& para)
{
    if (strcmp(argv[1], "bin") == 0) {
        // 执行流式bin文件升级
        return ExceptionBin(argc, argv, boot);
    } else if (strcmp(argv[1], "updater") == 0) {
        return ExceptionUpdater(argc, argv, boot);
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
    } else if (strcmp(argv[1], "ota_intral_update") == 0) {
        if (ExceptionUpdater(argc, argv, boot) == -1 ||
            strcat_s(boot.update, sizeof(boot.update), "\n--ota_intral_update") != 0) {
            return -1;
        }
    } else if (strcmp(argv[1], "subpkg_update") == 0) {
        if (strncpy_s(boot.update, sizeof(boot.update), "--subpkg_update", sizeof(boot.update) - 1) != 0) {
            cout << "strncpy_s failed!" << endl;
            return -1;
        }
    } else if (strcmp(argv[1], "updater_para") == 0) {
        return WriteUpdaterPara(argc, para) != 0 ? -1 : 0;
    } else if (strcmp(argv[1], "notify_update") == 0) {
        return 0;
    } else {
        cout << "Please input correct command!" << endl;
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

    int cmdResult = HandleCommand(argc, argv, boot, para);
    if (cmdResult == -1) {
        cout << "HandleCommand failed!" << endl;
        return -1;
    }
    if (strcmp(argv[1], "notify_update") != 0) {
        bool ret = WriteUpdaterMessage(miscFile, boot);
        if (!ret) {
            cout << "WriteUpdaterMessage failed!" << endl;
            return -1;
        }
    }
    HandleMiscInfo(argc, argv);
    _exit(-1);
    return 0;
}
