/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>
#include "fs_manager/mount.h"
#include "log.h"
#include "updater/updater_const.h"
#include "update_processor.h"
#include "utils.h"

using namespace Updater;
#ifndef UPDATER_UT
int main(int argc, char **argv)
{
    InitLogger("UPDATER_BINARY");
    if (argc < MINIMAL_ARGC_LIMIT) {
        LOG(ERROR) << "Invalid arguments.";
        return EXIT_INVALID_ARGS;
    }
    bool retry = false;
    int pipeFd;
    std::string packagePath;
    if (argc == 2) { // 2: package, pipe
        packagePath = argv[0];
        pipeFd = static_cast<int>(std::strtol(argv[1], nullptr, DECIMAL));
    } else if (argc == 3) { // 3 a: package, pipe, retry
        if (strcmp(argv[2], "retry") == 0) { // 2: retry index
            retry = true;
        }
        packagePath = argv[0];
        pipeFd = static_cast<int>(std::strtol(argv[1], nullptr, DECIMAL));
    } else { // 4 a: binary, package, pipe, retry=1/retry=0
        packagePath = argv[1];
        pipeFd = static_cast<int>(std::strtol(argv[2], nullptr, DECIMAL)); // 2: pipe index
        retry = strcmp(argv[3], "retry=0") == 0 ? false : true; // 3: retry index
    }

    // Re-load fstab to child process.
    LoadFstab();
    return ProcessUpdater(retry, pipeFd, packagePath, Utils::GetCertName());
}
#endif
