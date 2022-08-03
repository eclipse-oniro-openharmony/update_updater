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

#ifndef HDC_DAEMON_UPDATER_H
#define HDC_DAEMON_UPDATER_H

#include <unordered_map>

#include "commander_factory.h"
#include "transfer.h"

namespace Hdc {
class DaemonUpdater : public HdcTransferBase {
public:
    DISALLOW_COPY_MOVE(DaemonUpdater);
    explicit DaemonUpdater(HTaskInfo hTaskInfo);
    virtual ~DaemonUpdater();
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize) override;

private:
    void Init();
    void CheckCommand(const uint8_t *payload, int payloadSize);
    void DataCommand(const uint8_t *payload, int payloadSize) const;
    void EraseCommand(const uint8_t *payload, int payloadSize);
    void FormatCommand(const uint8_t *payload, int payloadSize);
    bool SendToHost(Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg);
    std::unique_ptr<Flashd::Commander> CreateCommander(const std::string &cmd);
    bool IsDeviceLocked() const;

    bool isInit_ = false;
    std::unique_ptr<Flashd::Commander> commander_ = nullptr;
    std::unordered_map<uint16_t, std::function<void(uint8_t *payload, int payloadSize)>> cmdFunc_;
    static std::atomic<bool> isRunning_;
};
} // namespace Hdc
#endif