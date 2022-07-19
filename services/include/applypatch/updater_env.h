/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef UPDATE_ENV_H
#define UPDATE_ENV_H

#include <cstdio>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include "package/pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"
#include "updater/updater.h"

using Uscript::UScriptEnv;
using Uscript::UScriptInstructionFactory;
using Uscript::UScriptInstructionFactoryPtr;

namespace Updater {
class UpdaterEnv : public UScriptEnv {
public:
    UpdaterEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager, PostMessageFunction postMessage, bool retry)
        : UScriptEnv(pkgManager), postMessage_(postMessage), isRetry_(retry) {}
    virtual ~UpdaterEnv();

    virtual void PostMessage(const std::string &cmd, std::string content);
    virtual UScriptInstructionFactoryPtr GetInstructionFactory();
    virtual const std::vector<std::string> GetInstructionNames() const;
    virtual bool IsRetry() const
    {
        return isRetry_;
    }
private:
    UScriptInstructionFactoryPtr factory_ = nullptr;
    PostMessageFunction postMessage_ = nullptr;
    bool isRetry_ = false;
};
}
#endif /* UPDATE_ENV_H */
