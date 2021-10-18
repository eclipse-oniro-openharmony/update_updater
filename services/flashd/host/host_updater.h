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
#ifndef HDC_HOST_UPDATER_H
#define HDC_HOST_UPDATER_H
#include "common.h"
#include "transfer.h"

namespace Hdc {
class HostUpdater : public HdcTransferBase {
public:
    explicit HostUpdater(HTaskInfo hTaskInfo);
    virtual ~HostUpdater();
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize) override;

private:
    void CheckMaster(CtxFile *context) override;

    bool BeginTransfer(CtxFile &context,
        const std::string &function, const char *payload, int minParam, int fileIndex);
    bool CheckUpdateContinue(const uint16_t command, const uint8_t *payload, int payloadSize);
    void RunQueue(CtxFile &context);
    std::string GetFileName(const std::string &fileName) const;
    void ProcessProgress(uint32_t percentage);
#ifdef UPDATER_UT
    void LogMsg(MessageLevel level, const char *msg, ...)
    {
        va_list vaArgs;
        va_start(vaArgs, msg);
        string log = Base::StringFormat(msg, vaArgs);
        va_end(vaArgs);
        WRITE_LOG(LOG_DEBUG, "LogMsg %d %s", level, log.c_str());
        return;
    }
    void SendRawData(uint8_t *bufPtr, const int size)
    {
        WRITE_LOG(LOG_DEBUG, "SendRawData %d", size);
    }
    bool SendToAnother(const uint16_t command, uint8_t *bufPtr, const int size)
    {
        WRITE_LOG(LOG_DEBUG, "SendToAnother command %d size %d", command, size);
        return true;
    }
#endif
private:
    bool CheckCmd(const std::string &function, const char *payload, int param);

    bool bSendProgress = false;
};
}
#endif