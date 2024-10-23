/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef UPDATE_UI_UPDATER_UI_CONST_H
#define UPDATE_UI_UPDATER_UI_CONST_H

namespace Updater {
constexpr const char *DEFAULT_FONT_FILENAME = "HarmonyOS_Sans_SC_Regular_Small.ttf";
constexpr const char *FB_DEV_PATH = "/dev/graphics/fb0";
constexpr const char *DRM_DEV_PATH = "/dev/dri/card0";

constexpr const char *UPDATERMODE_SDCARD = "sdcard";
constexpr const char *UPDATERMODE_FACTORYRST = "factoryRst";
constexpr const char *UPDATERMODE_REBOOTFACTORYRST = "rebootFactoryRst";
constexpr const char *UPDATERMODE_ATFACTORYRST = "atFactoryRst";
constexpr const char *UPDATERMODE_OTA = "ota";
constexpr const char *UPDATERMODE_RECOVER = "recover";
constexpr const char *UPDATERMODE_NIGHTUPDATE = "night_update";
constexpr const char *UPDATERMODE_USBUPDATE = "usb";

constexpr float UPDATER_UI_FONT_HEIGHT_RATIO = 1.3; // 1.3 : line height / font size ratio
} // namespace Updater
#endif /* UPDATE_UI_HOS_UPDATER_H */
