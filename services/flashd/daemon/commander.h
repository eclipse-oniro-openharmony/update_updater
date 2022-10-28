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

#ifndef FLASHD_COMMANDER_H
#define FLASHD_COMMANDER_H

#include "flashd/flashd.h"
#include "partition.h"

namespace Flashd {
enum class UpdaterState {
    DOING,
    SUCCESS,
    FAIL
};

using callbackFun = std::function<void(CmdType type, UpdaterState state, const std::string &msg)>;

class Commander {
public:
    explicit Commander(callbackFun callback) : callback_(callback) {}
    virtual ~Commander() = default;
    virtual void DoCommand([[maybe_unused]] const std::string &cmmParam, [[maybe_unused]] size_t fileSize) = 0;
    virtual void DoCommand(const uint8_t *payload, int payloadSize) = 0;
    virtual void PostCommand() = 0;

protected:
    void NotifySuccess(CmdType type, const std::string &msg = "") const;
    void NotifyFail(CmdType type, const std::string &msg = "") const;
    std::string_view GetCmdName() const
    {
        return cmdName_;
    }
    void SetCmdName(const std::string &cmdName)
    {
        cmdName_ = cmdName;
    }

    void UpdateProgress(CmdType type) const;

    bool IsCallbackVaild() const;

    size_t fileSize_ = 0;

    size_t currentSize_ = 0;

    int64_t startTime_ = 0;

    std::string cmdName_ = "";

private:
    callbackFun callback_ = nullptr;
};
} // namespace Flashd
#endif