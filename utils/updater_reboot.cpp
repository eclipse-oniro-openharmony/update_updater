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

#include <cstdio>
#include <cstring>
#include <string>
#include "log/log.h"
#include "utils.h"

int main(int argc, char **argv)
{
    if (argc > Updater::Utils::ARGC_TWO_NUMS) {
        Updater::LOG(Updater::ERROR) << "param must be nullptr or updater.";
        return 1;
    }
    if (argc == 1) {
        Updater::LOG(Updater::INFO) << "Updater::Utils::DoReboot nullptr";
        Updater::Utils::DoReboot("");
    } else {
        std::string updaterStr = "updater";
        if (!memcmp(argv[1], updaterStr.c_str(), updaterStr.length())) {
            Updater::Utils::DoReboot(argv[1]);
        } else if (strcmp(argv[1], "flashd") == 0) {
            Updater::Utils::DoReboot(argv[1]);
        } else if (strncmp(argv[1], "flashd:", strlen("flashd:")) == 0) {
            Updater::Utils::DoReboot("flashd", argv[1] + strlen("flashd:"));
        } else {
            Updater::LOG(Updater::INFO) << "param must be updater/flashd!";
        }
    }
    return 0;
}

