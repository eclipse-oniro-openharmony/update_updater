/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#ifndef UPDATE_HDI_CLIENT_H
#define UPDATE_HDI_CLIENT_H

#include "hdi/iupdate_interface.h"
#include "macros.h"

namespace Updater {
class UpdateHdiClient {
    DISALLOW_COPY_MOVE(UpdateHdiClient);
public:
    static UpdateHdiClient &GetInstance();
    int32_t GetLockStatus(bool &status);

private:
    UpdateHdiClient() {};
    ~UpdateHdiClient();
    bool Init();
    bool LoadLibrary();
    void CloseLibrary();
    void *handler_ = nullptr;
    IUpdateInterface *updateInterface_ = nullptr;
};
}
#endif
