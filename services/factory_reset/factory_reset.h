/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

#include <iostream>
#include <string>
#include <functional>
#include "updater/updater.h"
#include "updater_main.h"

namespace Updater {
using CommonResetPostFunc = std::function<int(FactoryResetMode)>;
using FactoryResetPreFunc = std::function<int(void)>;
using FactoryResetPostFunc = std::function<int(int)>;
using UserResetPreFunc = std::function<int(FactoryResetMode)>;
using UserResetPostFunc = std::function<int(FactoryResetMode)>;
class FactoryResetProcess {
    DISALLOW_COPY_MOVE(FactoryResetProcess);
public:
    FactoryResetProcess();
    virtual ~FactoryResetProcess() = default;

    static FactoryResetProcess &GetInstance();
    using ResetFunc = std::function<int(FactoryResetMode, const std::string &)>;
    void RegisterCommonResetPostFunc(CommonResetPostFunc ptr);
    void RegisterFactoryResetPreFunc(FactoryResetPreFunc ptr);
    void RegisterFactoryResetPostFunc(FactoryResetPostFunc ptr);
    void RegisterUserResetPreFunc(UserResetPreFunc ptr);
    void RegisterUserResetPostFunc(UserResetPostFunc ptr);
    int FactoryResetFunc(FactoryResetMode mode, const std::string &path);

private:
    CommonResetPostFunc CommonResetPostFunc_ = nullptr;
    FactoryResetPreFunc FactoryResetPreFunc_ = nullptr;
    FactoryResetPostFunc FactoryResetPostFunc_ = nullptr;
    UserResetPreFunc UserResetPreFunc_ = nullptr;
    UserResetPostFunc UserResetPostFunc_ = nullptr;
    std::unordered_map<FactoryResetMode, ResetFunc> resetTab_;

    int DoUserReset(FactoryResetMode mode, const std::string &path);
    int DoFactoryReset(FactoryResetMode mode, const std::string &path);
    void RegisterFunc(FactoryResetMode mode, ResetFunc func);
};

} // namespace Updater
#endif // FACTORY_RESET_H