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

#include "updatermain_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "log/log.h"
#include "updater_main.h"
#include "misc_info/misc_info.h"
#include "updater/updater_const.h"
#include "securec.h"
#include "utils.h"

using namespace Updater;
using namespace std;
constexpr uint32_t MAX_ARG_SIZE = 10;

static void ParseParamsFuzzTest()
{
    UpdateMessage boot {};
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    if (fp == nullptr) {
        return;
    }
    const std::string commandMsg = "boot_updater";
    if (strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()) == 0) {
        return;
    }
    if (strncpy_s(boot.update, sizeof(boot.update), "", sizeof(boot.update)) != 0) {
        return;
    }
    WriteUpdaterMessage(commandFile, boot);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    if (strncpy_s(argv[0], MAX_ARG_SIZE, "./main", MAX_ARG_SIZE) != 0) {
        return;
    }
    int argc = 1;
    Utils::ParseParams(argc, argv);
    PostUpdater(true);
    delete argv[0];
    delete []argv;
}

// static void UpdaterMainFuzzTest()
// {
//     // UpdaterUiInit();
//     auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen("/data/updater/updater.zip", "wb"), fclose);
//     if (fp == nullptr) {
//         return;
//     }

//     UpdateMessage boot {};
//     const std::string command1 = "boot_updater";
//     if (strncpy_s(boot.command, sizeof(boot.command) - 1, command1.c_str(), command1.size()) != 0) {
//         return;
//     }
//     const std::string command2 = "--update_package=/data/updater/updater.zip\n--retry_count=0";
//     if (strncpy_s(boot.update, sizeof(boot.update) - 1, command2.c_str(), command2.size()) != 0) {
//         return;
//     }
//     bool ret = WriteUpdaterMessage(MISC_FILE, boot);
//     if (!ret) {
//         return;
//     }

//     char **argv = new char*[MAX_ARG_SIZE];
//     argv[0] = new char[10];
//     if (strncpy_s(argv[0], MAX_ARG_SIZE, "./main", MAX_ARG_SIZE) != 0) {
//         return;
//     }
//     int argc = 1;
//     int lRet = UpdaterMain(argc, argv);
//     if (lRet != 0) {
//         return;
//     }

//     if (memset_s(boot.update, sizeof(boot.update), 0, sizeof(boot.update)) != 0) {
//         return;
//     }
//     const std::string command3 = "--user_wipe_data";
//     if (strncpy_s(boot.update, sizeof(boot.update) - 1, command3.c_str(), command3.size()) != 0) {
//         return;
//     }
//     ret = WriteUpdaterMessage(MISC_FILE, boot);
//     if (!ret) {
//         return;
//     }
//     UpdaterMain(argc, argv);

//     if (memset_s(boot.update, sizeof(boot.update), 0, sizeof(boot.update)) != 0) {
//         return;
//     }
//     const std::string command4 = "--factory_wipe_data";
//     if (strncpy_s(boot.update, sizeof(boot.update) - 1, command4.c_str(), command4.size()) != 0) {
//         return;
//     }
//     WriteUpdaterMessage(MISC_FILE, boot);
//     UpdaterMain(argc, argv);

//     ReadUpdaterMessage(MISC_FILE, boot);
//     delete argv[0];
//     delete []argv;
//     // DeleteView();
// }

namespace OHOS {
    void FuzzUpdater(const uint8_t* data, size_t size)
    {
        ParseParamsFuzzTest();
        // UpdaterMainFuzzTest();
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzUpdater(data, size);
    return 0;
}

