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

#include "commander.h"

#include "flashd_define.h"

namespace Flashd {
void Commander::NotifySuccess(CmdType type, const std::string &msg) const
{
    if (!IsCallbackVaild()) {
        return;
    }
    callback_(type, UpdaterState::SUCCESS, "[Success] " + msg);
}

void Commander::NotifyFail(CmdType type, const std::string &msg) const
{
    if (!IsCallbackVaild()) {
        return;
    }
    callback_(type, UpdaterState::FAIL, " " + msg);
}


void Commander::UpdateProgress(CmdType type) const
{
    if (!IsCallbackVaild()) {
        return;
    }
    if (fileSize_ == 0) {
        FLASHD_LOGE("file size is invaild");
        return;
    }
    uint8_t currentProcess = static_cast<uint8_t>(static_cast<int64_t>(currentSize_) * PERCENT_FINISH / fileSize_);
    if (currentProcess > PERCENT_FINISH) {
        FLASHD_LOGW("currSize_ = %u, fileSize_ = %u, currentProces = %u", currentSize_, fileSize_, currentProcess);
        return;
    }

    static uint8_t preProcess = 0;
    if (currentProcess == preProcess) {
        return;
    }
    preProcess = currentProcess;
    callback_(type, UpdaterState::DOING, std::to_string(currentProcess));
}

bool Commander::IsCallbackVaild() const
{
    if (callback_ == nullptr) {
        FLASHD_LOGE("callback_ is null");
        return false;
    }
    return true;
}
} // namespace Flashd