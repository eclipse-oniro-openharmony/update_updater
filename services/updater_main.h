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
#include "package/pkg_manager.h"
#include "pkg_manager.h"
#include "updater/updater.h"
#include "updater_init.h"

namespace Updater {
using namespace Hpackage;
enum FactoryResetMode {
    USER_WIPE_DATA = 0,
    FACTORY_WIPE_DATA,
    MENU_WIPE_DATA,
    INVALID_MODE,
};

int UpdaterMain(int argc, char **argv);

int FactoryReset(FactoryResetMode mode, const std::string &path);

void RebootAfterUpdateSuccess(const UpdaterParams &upParams);

UpdaterStatus InstallUpdaterPackage(UpdaterParams &upParams, Hpackage::PkgManager::PkgManagerPtr manager);

UpdaterStatus DoUpdatePackages(UpdaterParams &upParams);

UpdaterStatus StartUpdaterEntry(UpdaterParams &upParams);

UpdaterStatus DoUpdaterEntry(UpdaterParams &upParams);

UpdaterStatus DoFactoryRstEntry(UpdaterParams &upParams);

UpdaterStatus UpdaterFromSdcard(UpdaterParams &upParams);

bool IsBatteryCapacitySufficient();

void DeleteInstallTimeFile();

void WriteInstallTime(UpdaterParams &upParams);

void ReadInstallTime(UpdaterParams &upParams);

bool IsDouble(const std::string& str);

UpdaterStatus InstallUpdaterPackages(UpdaterParams &upParams);

int OtaUpdatePreCheck(PkgManager::PkgManagerPtr pkgManager, const std::string &packagePath);

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
bool IsSupportOption([[maybe_unused]] const std::string &option);
UpdaterStatus ProcessOtherOption([[maybe_unused]] const std::string &option,
    [[maybe_unused]] UpdaterParams &upParams, PackageUpdateMode &mode);
bool PreStartBinaryEntry([[maybe_unused]] const std::string &path);
int32_t VerifySpecialPkgs([[maybe_unused]]UpdaterParams &upParams);
void UpdaterVerifyFailEntry(bool verifyret);
bool IsSpareBoardBoot(void);
bool IsNeedWipe();
void NotifyAutoReboot(PackageUpdateMode &mode);
bool NotifySdUpdateReboot(const UpdaterParams &upParams);
void NotifyReboot(const std::string& rebootTarget, const std::string &rebootReason, const std::string &extData = "");
UpdaterStatus NotifyActionResult(UpdaterParams &upParams,
    UpdaterStatus &status, const std::vector<NotifyAction> &notifyActionVec);
UpdaterStatus UpdateSubPkg(UpdaterParams &upParams);
void NotifyPreCheck(UpdaterStatus &status, UpdaterParams &upParams);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
} // namespace Updater
#endif // UPDATER_MAIN_H
