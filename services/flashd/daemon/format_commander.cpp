/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include "format_commander.h"

#include "flashd_define.h"
#include "flashd_utils.h"
#include "fs_manager/mount.h"
#include "updater/updater.h"

namespace Flashd {
namespace {
const std::vector<std::string> ERASE_ONLY_LIST = { "boot", "fastboot", "kernel", "misc", "system" };
constexpr size_t CMD_PARAM_COUNT_MIN = 2;
}

void FormatCommander::DoCommand([[maybe_unused]] const std::string &cmmParam, [[maybe_unused]] size_t fileSize)
{
    FLASHD_LOGI("unsupport");
}

void FormatCommander::DoCommand(const uint8_t *payload, int payloadSize)
{
    FLASHD_LOGI("start to format");
    if (payload == nullptr || payloadSize <= 0) {
        NotifyFail(CmdType::FORMAT);
        FLASHD_LOGE("payload is null or payloadSize is invaild");
        return;
    }

    std::string cmdParam(reinterpret_cast<const char *>(payload), payloadSize);
    auto params = Split(cmdParam, { "-f" });
    if (params.size() < CMD_PARAM_COUNT_MIN) {
        NotifyFail(CmdType::FORMAT);
        FLASHD_LOGE("param is invaild");
        return;
    }

    if (auto ret = DoFormat(params[1]); ret != 0) {
        NotifyFail(CmdType::FORMAT);
        FLASHD_LOGE("DoFormat fail, ret = %d", ret);
        return;
    }

    NotifySuccess(CmdType::FORMAT);
    FLASHD_LOGI("format success");
}

bool FormatCommander::IsOnlySupportErase(const std::string &partitionName) const
{
    auto iter = std::find(ERASE_ONLY_LIST.begin(), ERASE_ONLY_LIST.end(), partitionName);
    return iter != ERASE_ONLY_LIST.end();
}

int FormatCommander::DoFormat(const std::string &partitionName) const
{
    FLASHD_LOGI("start to format %s", partitionName.c_str());
    Partition part(partitionName);
    if (IsOnlySupportErase(partitionName)) {
        return part.DoErase();
    }
    return part.DoFormat();
}

void FormatCommander::PostCommand()
{
    Updater::PostUpdater(false);
}
} // namespace Flashd