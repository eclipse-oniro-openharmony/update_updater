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
public:
    static void Init();
private:
    static void InitDisplayDriver(); // input driver init
    static void InitEngine(); // Graphic UI engine init
    static void InitConfig(); // ui configs Init
    static void InitInputDriver(); // input driver init
    static void InitEvts(); // input event callback init

    static bool InitBrightness(const char *brightnessFile, const char *maxBrightnessFile); // init brightness
    static void InitRootView();
};
} // namespace Updater
#endif
