/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "updater/hwfault_retry.h"
#include <unistd.h>
#include "init_reboot.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "securec.h"

namespace Updater {
HwFaultRetry &HwFaultRetry::GetInstance()
{
    static HwFaultRetry instance;
    return instance;
}

HwFaultRetry::HwFaultRetry()
{
    RetryFunc rebootFunc = [this]() {
                                return this->RebootRetry();
                            };
    RegisterFunc(VERIFY_FAILED_REBOOT, rebootFunc);
    RegisterFunc(IO_FAILED_REBOOT, rebootFunc);
}

void HwFaultRetry::RegisterFunc(const std::string &faultInfo, RetryFunc func)
{
    if (!retryMap_.emplace(faultInfo, func).second) {
        LOG(ERROR) << "emplace: " << faultInfo.c_str() << " fail";
    }
}

void HwFaultRetry::DoRetryAction()
{
    auto it = retryMap_.find(faultInfo_);
    if (it == retryMap_.end() || it->second == nullptr) {
        LOG(ERROR) << "GetRepair func for: " << faultInfo_.c_str() << " fail";
        return;
    }
    return (it->second)();
}

void HwFaultRetry::SetFaultInfo(const std::string &faultInfo)
{
    faultInfo_ = faultInfo;
}

void HwFaultRetry::SetRetryCount(const uint32_t count)
{
    retryCount_ = count;
}

void HwFaultRetry::RebootRetry()
{
    if (retryCount_ >= MAX_RETRY_COUNT) {
        LOG(INFO) << "retry more than 3 times, no need retry";
        return;
    }

    Utils::SetMessageToMisc("boot_updater", retryCount_ + 1, "retry_count");
    if (!SetInfoToMisc()) {
        LOG(WARNING) << "UpdaterDoReboot set misc failed";
    }

    PostUpdater(false);
    sync();
#ifndef UPDATER_UT
    DoReboot("updater");
    while (true) {
        pause();
    }
#endif
}

bool HwFaultRetry::SetInfoToMisc()
{
    struct UpdateMessage msg = {};
    if (!ReadUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "UpdaterDoReboot read misc failed";
        return false;
    }

    (void)memset_s(msg.faultinfo, sizeof(msg.faultinfo), 0, sizeof(msg.faultinfo));
    if (strcpy_s(msg.faultinfo, sizeof(msg.faultinfo) - 1, faultInfo_.c_str()) != EOK) {
        LOG(ERROR) << "failed to copy update";
        return false;
    }
    LOG(INFO) << "UpdaterDoReboot msg keyinfo: " << msg.faultinfo;

    if (!WriteUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "UpdaterDoReboot: WriteUpdaterMiscMsg error";
        return false;
    }
    return true;
}
} // Updater
