/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef UPDATER_FLASHD_DEFINE_H
#define UPDATER_FLASHD_DEFINE_H

#include "common.h"
#include "flashd/flashd.h"
#include "log/log.h"

namespace Flashd {
#define FLASHD_LOGE(format, ...) Logger(Updater::ERROR, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define FLASHD_LOGD(format, ...) Logger(Updater::DEBUG, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define FLASHD_LOGI(format, ...) Logger(Updater::INFO, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define FLASHD_LOGW(format, ...) Logger(Updater::WARNING, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
}

namespace Hdc {
constexpr const char *CMDSTR_UPDATE_SYSTEM = "update";
constexpr const char *CMDSTR_FLASH_PARTITION = "flash";
constexpr const char *CMDSTR_ERASE_PARTITION = "erase";
constexpr const char *CMDSTR_FORMAT_PARTITION = "format";
constexpr int TASK_UPDATER = TASK_APP + 1;

enum FlashdCommander {
    CMD_UPDATER_UPDATE_INIT = 4000,
    CMD_UPDATER_FLASH_INIT,
    CMD_UPDATER_CHECK,
    CMD_UPDATER_BEGIN,
    CMD_UPDATER_DATA,
    CMD_UPDATER_FINISH,
    CMD_UPDATER_ERASE,
    CMD_UPDATER_FORMAT,
    CMD_UPDATER_PROGRESS
};
}
#endif
