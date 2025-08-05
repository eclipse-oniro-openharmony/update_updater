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

#include "updater/hardware_fault_retry.h"
#include <unistd.h>
#include "init_reboot.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "securec.h"

namespace Updater {
HardwareFaultRetry &HardwareFaultRetry::GetInstance()
{
    static HardwareFaultRetry instance;
    return instance;
}

HardwareFaultRetry::HardwareFaultRetry()
{
    RetryFunc rebootFunc = [this]() {
                                return this->RebootRetry();
                            };
    RegisterFunc(VERIFY_FAILED_REBOOT, rebootFunc);
    RegisterFunc(IO_FAILED_REBOOT, rebootFunc);
    RegisterFunc(BLOCK_UPDATE_FAILED_REBOOT, rebootFunc);
}

void HardwareFaultRetry::RegisterFunc(const std::string &faultInfo, RetryFunc func)
{
    if (!retryMap_.emplace(faultInfo, func).second) {
        LOG(ERROR) << "emplace: " << faultInfo.c_str() << " fail";
    }
}

void HardwareFaultRetry::RegisterDefaultFunc(const std::string &faultInfo)
{
    if (!retryMap_.emplace(faultInfo, [this]() {
                                return this->RebootRetry();
                            }).second) {
        LOG(ERROR) << "emplace: " << faultInfo.c_str() << " fail";
    }
}

void HardwareFaultRetry::RemoveFunc(const std::string &faultInfo)
{
    if (retryMap_.erase(faultInfo) == 0) {
        LOG(ERROR) << "erase " << faultInfo.c_str() << " fail";
    }
}

void HardwareFaultRetry::DoRetryAction()
{
    auto it = retryMap_.find(faultInfo_);
    if (it == retryMap_.end() || it->second == nullptr) {
        LOG(ERROR) << "GetRepair func for: " << faultInfo_.c_str() << " fail";
        return;
    }
    return (it->second)();
}

void HardwareFaultRetry::SetFaultInfo(const std::string &faultInfo)
{
    faultInfo_ = faultInfo;
}

void HardwareFaultRetry::SetRetryCount(const uint32_t count)
{
    retryCount_ = count;
}

void HardwareFaultRetry::SetEffectiveValue(bool value)
{
    effective_ = value;
}

void HardwareFaultRetry::SetRebootCmd(const std::string &rebootCmd)
{
    rebootCmd_ = rebootCmd;
}

bool HardwareFaultRetry::IsRetry(void)
{
    return isRetry_;
}

void HardwareFaultRetry::RebootRetry()
{
    if (!effective_) {
        LOG(WARNING) << "Special scenarios do not take effect, not need retry.";
        return;
    }
    if (retryCount_ >= MAX_RETRY_COUNT) {
        LOG(INFO) << "retry more than 3 times, no need retry";
        return;
    }
    LOG(INFO) << "enter into reboot retry";
    Utils::AddUpdateInfoToMisc("retry_count", retryCount_ + 1);
    Utils::SetFaultInfoToMisc(faultInfo_);
    isRetry_ = true;

    PostUpdater(false);
    sync();
#ifndef UPDATER_UT
    if (rebootCmd_.empty()) {
        DoReboot("updater:Updater fault retry");
    } else {
        DoReboot(rebootCmd_.c_str());
    }
    while (true) {
        pause();
    }
#endif
}
} // Updater
