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
#ifndef HDC_DAEMON_UPDATER_H
#define HDC_DAEMON_UPDATER_H
#include <cstdlib>

#include "securec.h"
#include "transfer.h"

namespace Hdc {
class DaemonUpdater : public HdcTransferBase {
public:
    explicit DaemonUpdater(HTaskInfo hTaskInfo);
    virtual ~DaemonUpdater();
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize) override;
    bool ReadyForRelease();
#ifdef UPDATER_UT
    void DoTransferFinish();
#endif
private:
    virtual void WhenTransferFinish(CtxFile *context) override;
    void ProcessUpdateCheck(const uint8_t *payload, const int payloadSize);
    void RunUpdateShell(uint8_t type, const std::string &options, const std::string &package);
    void AsyncUpdateFinish(uint8_t type, int32_t ret, const string &result);
    void SendProgress(size_t dataLen);
#ifdef UPDATER_UT
    bool SendToAnother(const uint16_t command, uint8_t *bufPtr, const int size)
    {
        WRITE_LOG(LOG_DEBUG, "SendToAnother %d size %d", command, size);
        return true;
    }
    void TaskFinish()
    {
        WRITE_LOG(LOG_DEBUG, "TaskFinish ");
        return;
    }
#endif
private:
    double currSize_ = 0;
    double totalSize_ = 0;
    int32_t percentage_ = 0;
    void* flashHandle_ = nullptr;
    std::string errorMsg_ {};
};
} // namespace Hdc
#endif