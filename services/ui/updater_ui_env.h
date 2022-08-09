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
#ifndef UPDATER_UI_ENV_H
#define UPDATER_UI_ENV_H

#include <memory>
#include <vector>

#include "driver/surface_dev.h"
#include "macros.h"
#include "updater_ui.h"
#include "view_api.h"

namespace Updater {
class UpdaterUiEnv {
    DISALLOW_COPY_MOVE(UpdaterUiEnv);
public:
    static UpdaterUiEnv &GetInstance();
    void Init();
protected:
    UpdaterUiEnv() = default;
    ~UpdaterUiEnv() = default;
    void InitDisplayDriver(); // input driver init
    void InitEngine() const; // Graphic UI engine init
    void InitConfig() const; // ui configs Init
    void InitInputDriver() const; // input driver init
    void InitEvts() const; // input event callback init

    bool InitBrightness(const char *brightnessFile, const char *maxBrightnessFile) const; // init brightness
    void InitRootView() const;
    UpdaterMode mode_;
    int screenW_;
    int screenH_;
};
}
#endif
