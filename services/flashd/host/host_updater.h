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
#define HOSTUPDATER_CHECK(retCode, exper, ...) \
    if (!(retCode)) { \
        LogMsg(MSG_FAIL, __VA_ARGS__); \
        exper; \
    }

class HostUpdater : public HdcTransferBase {
public:
    explicit HostUpdater(HTaskInfo hTaskInfo);
    virtual ~HostUpdater();
    bool CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize) override;

    static bool CheckMatchUpdate(const std::string &input, std::string &stringError, uint16_t &cmdFlag, bool &bJumpDo);
    static bool ConfirmCommand(const string &commandIn, bool &closeInput);
#ifdef UPDATER_UT
    void OpenFile()
    {
        CheckMaster(&ctxNow);
    }
    static void SetInput(const std::string &input);
#endif
private:
    void CheckMaster(CtxFile *context) override;
    bool BeginTransfer(CtxFile &context,
        const std::string &function, const char *payload, int minParam, int fileIndex);
    bool CheckUpdateContinue(const uint16_t command, const uint8_t *payload, int payloadSize);
    void RunQueue(CtxFile &context);
    std::string GetFileName(const std::string &fileName) const;
    void ProcessProgress(uint32_t percentage);
    void SendRawData(uint8_t *bufPtr, const int size);
#ifdef UPDATER_UT
    void LogMsg(MessageLevel level, const char *msg, ...)
    {
        return;
    }
    bool SendToAnother(const uint16_t command, uint8_t *bufPtr, const int size)
    {
        std::string s((char *)bufPtr, size);
        WRITE_LOG(LOG_DEBUG, "SendToAnother command %d size %d %s", command, size, s.c_str());
        return true;
    }
#endif
private:
    bool CheckCmd(const std::string &function, const char *payload, int param);
    bool sendProgress = false;
};
}
#endif