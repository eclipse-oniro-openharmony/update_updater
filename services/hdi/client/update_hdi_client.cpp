/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "update_hdi_client.h"

#include <cerrno>
#include <dlfcn.h>

#include "log/log.h"

namespace Updater {
constexpr const char *HDI_LIB_NAME = "libupdate_hdi_impl.z.so";

UpdateHdiClient &UpdateHdiClient::GetInstance()
{
    static UpdateHdiClient instance;
    return instance;
}

UpdateHdiClient::~UpdateHdiClient()
{
    CloseLibrary();
}

int32_t UpdateHdiClient::GetLockStatus(bool &status)
{
    if (updateInterface_ == nullptr && !Init()) {
        LOG(ERROR) << "updateInterface_ is null";
        return -1;
    }
    return updateInterface_->GetLockStatus(status);
}

bool UpdateHdiClient::Init()
{
    if (updateInterface_ != nullptr) {
        LOG(INFO) << "updateInterface_ has loaded";
        return true;
    }
    return LoadLibrary();
}

bool UpdateHdiClient::LoadLibrary()
{
    constexpr const char *getInstance = "GetInterfaceInstance";
    if (updateInterface_ != nullptr) {
        LOG(INFO) << "updateInterface_ has loaded";
        return true;
    }

    handler_ = dlopen(HDI_LIB_NAME, RTLD_LAZY);
    if (handler_ == nullptr) {
        LOG(ERROR) << "open " << HDI_LIB_NAME << " fail";
        return false;
    }

    auto getInterface = reinterpret_cast<IUpdateInterface *(*)()>(dlsym(handler_, getInstance));
    if (getInterface == nullptr) {
        LOG(ERROR) << "can not find " << getInstance;
        CloseLibrary();
        return false;
    }

    updateInterface_ = getInterface();
    if (updateInterface_ == nullptr) {
        LOG(ERROR) << "updateInterface_ is null";
        CloseLibrary();
        return false;
    }
    LOG(INFO) << "load " << HDI_LIB_NAME << " success";
    return true;
}

void UpdateHdiClient::CloseLibrary()
{
    if (handler_ == nullptr) {
        LOG(INFO) << HDI_LIB_NAME << "is already closed";
        return;
    }
    updateInterface_ = nullptr;
    dlclose(handler_);
    handler_ = nullptr;
    LOG(INFO) << HDI_LIB_NAME << " is closed";
}
}
