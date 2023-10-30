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
#ifndef UPDATER_MAIN_H
#define UPDATER_MAIN_H

#include <iostream>
#include <string>
#include "pkg_manager.h"
#include "updater/updater.h"
#include "updater_init.h"

namespace Updater {
enum FactoryResetMode {
    USER_WIPE_DATA = 0,
    FACTORY_WIPE_DATA,
};

int UpdaterMain(int argc, char **argv);

int FactoryReset(FactoryResetMode mode, const std::string &path);

UpdaterStatus InstallUpdaterPackage(UpdaterParams &upParams, Hpackage::PkgManager::PkgManagerPtr manager);

UpdaterStatus DoUpdatePackages(UpdaterParams &upParams);

UpdaterStatus StartUpdaterEntry(UpdaterParams &upParams);

UpdaterStatus DoUpdaterEntry(UpdaterParams &upParams);

UpdaterStatus UpdaterFromSdcard(UpdaterParams &upParams);

bool IsBatteryCapacitySufficient();

UpdaterStatus InstallUpdaterPackages(UpdaterParams &upParams);

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
bool IsSupportOption([[maybe_unused]] const std::string &option);
UpdaterStatus ProcessOtherOption([[maybe_unused]] const std::string &option,
    [[maybe_unused]] UpdaterParams &upParams);
bool PreStartBinaryEntry([[maybe_unused]] const std::string &path);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
} // namespace Updater
#endif // UPDATER_MAIN_H
