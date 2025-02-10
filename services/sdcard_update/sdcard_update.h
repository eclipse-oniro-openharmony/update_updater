/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef SDCARD_UPDATE_H
#define SDCARD_UPDATE_H

#include <iostream>
#include <string>
#include "updater/updater.h"

namespace Updater {
UpdaterStatus CheckSdcardPkgs(UpdaterParams &upParams);
UpdaterStatus FindAndMountSdcard(UpdaterParams &upParams);
UpdaterStatus GetPkgsFromSdcard(UpdaterParams &upParams);

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
UpdaterStatus GetSdcardPkgsPath(UpdaterParams &upParams);
UpdaterStatus GetSdcardPkgsFromDev(UpdaterParams &upParams);
UpdaterStatus GetSdcardInternalPkgs(UpdaterParams &upParams);
UpdaterStatus MountAndGetPkgs(UpdaterParams &upParams);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

bool DoMountSdCard(std::vector<std::string> &sdCardStr, std::string &mountPoint, UpdaterParams &upParams);
} // namespace Updater
#endif // SDCARD_UPDATE_H