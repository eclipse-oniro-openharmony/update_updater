/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#ifndef UPDATER_HDC_DEFINE_H
#define UPDATER_HDC_DEFINE_H
#include "common.h"

namespace Hdc {
const string CMDSTR_UPDATE_SYSTEM = "update";
const string CMDSTR_FLASH_PARTITION = "flash";
const string CMDSTR_ERASE_PARTITION = "erase";
const string CMDSTR_FORMAT_PARTITION = "format";

// update
const int CMD_UPDATER_UPDATE_INIT = CMD_APP_INIT + 200;
const int CMD_UPDATER_FLASH_INIT = CMD_UPDATER_UPDATE_INIT + 1;
const int CMD_UPDATER_CHECK = CMD_UPDATER_UPDATE_INIT + 2;
const int CMD_UPDATER_BEGIN = CMD_UPDATER_UPDATE_INIT + 3;
const int CMD_UPDATER_DATA = CMD_UPDATER_UPDATE_INIT + 4;
const int CMD_UPDATER_FINISH = CMD_UPDATER_UPDATE_INIT + 5;
const int CMD_UPDATER_ERASE = CMD_UPDATER_UPDATE_INIT + 6;
const int CMD_UPDATER_FORMAT = CMD_UPDATER_UPDATE_INIT + 7;
const int CMD_UPDATER_PROGRESS = CMD_UPDATER_UPDATE_INIT + 8;

const int TASK_UPDATER = TASK_APP + 1;
}  // namespace Hdc
#endif  // UPDATER_HDC_DEFINE_H
