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
#include "factory_reset.h"
#include <string>
#include "log/dump.h"
#include "log/log.h"
#include "fs_manager/mount.h"
#include "scope_guard.h"

namespace Updater {
FactoryResetProcess &FactoryResetProcess::GetInstance()
{
    static FactoryResetProcess resetProcessor;
    return resetProcessor;
}

FactoryResetProcess::FactoryResetProcess() {}

static int CommonResetPost(bool flag)
{
    LOG(INFO) << "CommonResetPost";
    return 0;
}

void FactoryResetProcess::RegisterCommonResetPostFunc(CommonResetPostFunc ptr)
{
    CommonResetPostFunc_ = std::move(ptr);
}

static int FactoryResetPre(FactoryResetMode mode)
{
    LOG(INFO) << "FactoryResetPre";
    return 0;
}

void FactoryResetProcess::RegisterFactoryResetPreFunc(FactoryResetPreFunc ptr)
{
    FactoryResetPreFunc_ = std::move(ptr);
}

static int FactoryResetPost(FactoryResetMode mode, int status)
{
    LOG(INFO) << "FactoryResetPost";
    return 0;
}

void FactoryResetProcess::RegisterFactoryResetPostFunc(FactoryResetPostFunc ptr)
{
    FactoryResetPostFunc_ = std::move(ptr);
}

int FactoryResetProcess::DoFactoryReset(FactoryResetMode mode, const std::string &path)
{
    int resetStatus = 0;
    STAGE(UPDATE_STAGE_BEGIN) << "Factory FactoryReset";
    if (FactoryResetPreFunc_ == nullptr || FactoryResetPreFunc_(mode) != 0) {
        LOG(ERROR) << "FactoryResetPreFunc_ fail";
        return -1;
    }
    LOG(INFO) << "Begin erasing data";
    if (FormatPartition(path, true) != 0) {
        STAGE(UPDATE_STAGE_FAIL) << "Factory FactoryReset";
        ERROR_CODE(CODE_FACTORY_RESET_FAIL);
        resetStatus = 1;
    }
    if (resetStatus == 0 && (CommonResetPostFunc_ == nullptr || CommonResetPostFunc_(mode) != 0)) {
        LOG(ERROR) << "CommonResetPostFunc_ fail";
        resetStatus = -1;
    }
    if (FactoryResetPostFunc_ == nullptr || FactoryResetPostFunc_(mode, resetStatus) != 0) {
        LOG(ERROR) << "FactoryResetPostFunc_ fail";
        return -1;
    }

    LOG(INFO) << "Factory level FactoryReset status:" << resetStatus;
    return resetStatus;
}

extern "C" __attribute__((constructor)) void RegisterCommonResetPostFunc(void)
{
    FactoryResetProcess::GetInstance().RegisterCommonResetPostFunc(CommonResetPost);
}

extern "C" __attribute__((constructor)) void RegisterFactoryResetPreFunc(void)
{
    FactoryResetProcess::GetInstance().RegisterFactoryResetPreFunc(FactoryResetPre);
}

extern "C" __attribute__((constructor)) void RegisterFactoryResetPostFunc(void)
{
    FactoryResetProcess::GetInstance().RegisterFactoryResetPostFunc(FactoryResetPost);
}
} // Updater