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
#ifndef UPDATER_HWFAULT_RETRY_H
#define UPDATER_HWFAULT_RETRY_H

#include "log/log.h"
#include <unordered_map>
#include <vector>
#include <sys/time.h>
#include <unistd.h>

namespace Updater {
class HwFaultRetry {
public:
    HwFaultRetry();
    ~HwFaultRetry() = default;

    static HwFaultRetry &GetInstance();
    using RetryFunc = std::function<void ()>;
    void DoRetryAction();
    void RegisterFunc(const std::string &faultInfo, RetryFunc func);
    void SetFaultInfo(const std::string &faultInfo);
    void SetRetryCount(const uint32_t count);

private:
    void RebootRetry();
    bool SetInfoToMisc();

    std::unordered_map<std::string, RetryFunc> retryMap_;
    std::string faultInfo_;
    uint32_t retryCount_ {};
};
} // Updater
#endif // UPDATER_HWFAULT_RETRY_H
