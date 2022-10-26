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

#include "erase_commander.h"

#include "flashd_define.h"
#include "flashd_utils.h"
#include "updater/updater.h"


namespace Flashd {
namespace {
constexpr size_t CMD_PARAM_COUNT_MIN = 2;
}

void EraseCommander::DoCommand([[maybe_unused]] const std::string &cmmParam, [[maybe_unused]] size_t fileSize)
{
    FLASHD_LOGI("unsupport");
}

void EraseCommander::DoCommand(const uint8_t *payload, int payloadSize)
{
    FLASHD_LOGI("start to erase");
    if (payload == nullptr || payloadSize <= 0) {
        NotifyFail(CmdType::ERASE);
        FLASHD_LOGI("payload is null or payloadSize is invaild");
        return;
    }

    std::string cmdParam(reinterpret_cast<const char *>(payload), payloadSize);
    auto params = Split(cmdParam, { "-f" });
    if (params.size() < CMD_PARAM_COUNT_MIN) {
        NotifyFail(CmdType::ERASE);
        FLASHD_LOGE("param is invaild");
        return;
    }
    if (!DoErase(params[1])) {
        NotifyFail(CmdType::ERASE);
        return;
    }
    NotifySuccess(CmdType::ERASE);
}

bool EraseCommander::DoErase(const std::string &partitionName) const
{
    FLASHD_LOGI("start to erase %s", partitionName.c_str());
    Partition part(partitionName);
    if (auto ret = part.DoErase(); ret != 0) {
        FLASHD_LOGE("DoErase fail, ret = %d", ret);
        return false;
    }
    return true;
}

void EraseCommander::PostCommand()
{
    Updater::PostUpdater(false);
}
} // namespace Flashd