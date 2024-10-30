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

FactoryResetProcess::FactoryResetProcess()
{
    RegisterFunc(USER_WIPE_DATA, [this](FactoryResetMode mode, const std::string &path) {
        return DoUserReset(mode, path); });
    RegisterFunc(FACTORY_WIPE_DATA, [this](FactoryResetMode mode, const std::string &path) {
        return DoFactoryReset(mode, path); });
    RegisterFunc(MENU_WIPE_DATA, [this](FactoryResetMode mode, const std::string &path) {
        return DoUserReset(mode, path); });
}

void FactoryResetProcess::RegisterFunc(FactoryResetMode mode, ResetFunc func)
{
    if (!resetTab_.emplace(mode, func).second) {
        LOG(ERROR) << "emplace: " << mode << " fail";
    }
}

int FactoryResetProcess::FactoryResetFunc(FactoryResetMode mode, const std::string &path)
{
    auto iter = resetTab_.find(mode);
    if (iter == resetTab_.end() || iter->second == nullptr) {
        LOG(ERROR) << "Invalid factory reset tag: " << mode;
        return 1;
    }
    int resetStatus = iter->second(mode, path);
    // ON_SCOPE_EXIT(factoryResetPost) {
    //     if (mode == FACTORY_WIPE_DATA &&
    //         (FactoryResetPostFunc_ == nullptr || FactoryResetPostFunc_(resetStatus) != 0)) {
    //         LOG(ERROR) << "FactoryResetPostFunc_ fail";
    //     }
    // };
    if (resetStatus != 0) {
        LOG(ERROR) << "Do factory reset failed! tag: " << mode;
        return 1;
    }
    // if (CommonResetPostFunc_ == nullptr || CommonResetPostFunc_(mode) != 0) {
    //     resetStatus = 1;
    //     LOG(ERROR) << "CommonResetPostFunc_ fail";
    //     return -1;
    // }
    return 0;
}

static int CommonResetPost(bool flag)
{
    LOG(INFO) << "CommonResetPost";
    return 0;
}

void FactoryResetProcess::RegisterCommonResetPostFunc(CommonResetPostFunc ptr)
{
    CommonResetPostFunc_ = std::move(ptr);
}

static int FactoryResetPre()
{
    LOG(INFO) << "FactoryResetPre";
    return 0;
}

void FactoryResetProcess::RegisterFactoryResetPreFunc(FactoryResetPreFunc ptr)
{
    FactoryResetPreFunc_ = std::move(ptr);
}

static int FactoryResetPost(int status)
{
    LOG(INFO) << "FactoryResetPost";
    return 0;
}

void FactoryResetProcess::RegisterFactoryResetPostFunc(FactoryResetPostFunc ptr)
{
    FactoryResetPostFunc_ = std::move(ptr);
}

static int UserResetPre()
{
    LOG(INFO) << "UserResetPre";
    return 0;
}

void UserResetProcess::RegisterUserResetPreFunc(UserResetPreFunc ptr)
{
    UserResetPreFunc_ = std::move(ptr);
}

static int UserResetPost()
{
    LOG(INFO) << "UserResetPost";
    return 0;
}

void UserResetProcess::RegisterUserResetPostFunc(UserResetPostFunc ptr)
{
    UserResetPostFunc_ = std::move(ptr);
}

int FactoryResetProcess::DoUserReset(FactoryResetMode mode, const std::string &path)
{
    STAGE(UPDATE_STAGE_BEGIN) << "User FactoryReset";
    if (UserResetPreFunc_ == nullptr || UserResetPreFunc_() != 0) {
        LOG(ERROR) << "UserResetPreFunc_ fail";
        return -1;
    }
    LOG(INFO) << "Begin erasing data";
    int ret = FormatPartition(path, true);
    if (ret != 0) {
        LOG(ERROR) << "User level FactoryReset failed";
        STAGE(UPDATE_STAGE_FAIL) << "User FactoryReset";
        ERROR_CODE(CODE_FACTORY_RESET_FAIL);
    }
    if (UserResetPostFunc_ == nullptr || UserResetPostFunc_() != 0 || ret != 0) {
        LOG(ERROR) << "UserResetPostFunc_ fail";
        return -1;
    }
    if (CommonResetPostFunc_ == nullptr || CommonResetPostFunc_(mode) != 0) {
        LOG(ERROR) << "CommonResetPostFunc_ fail";
        return -1;
    }
    LOG(INFO) << "User level FactoryReset success";
    STAGE(UPDATE_STAGE_SUCCESS) << "User FactoryReset";

    return 0;
}

int FactoryResetProcess::DoFactoryReset(FactoryResetMode mode, const std::string &path)
{
    int resetStatus = 0;
    STAGE(UPDATE_STAGE_BEGIN) << "Factory FactoryReset";
    if (FactoryResetPreFunc_ == nullptr || FactoryResetPreFunc_() != 0) {
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
    if (FactoryResetPostFunc_ == nullptr || FactoryResetPostFunc_(resetStatus) != 0) {
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

extern "C" __attribute__((constructor)) void RegisterUserResetPreFunc(void)
{
    UserResetProcess::GetInstance().RegisterUserResetPreFunc(UserResetPre);
}

extern "C" __attribute__((constructor)) void RegisterUserResetPostFunc(void)
{
    UserResetProcess::GetInstance().RegisterUserResetPostFunc(UserResetPost);
}
} // Updater