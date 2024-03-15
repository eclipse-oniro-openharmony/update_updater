/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef UPDATER_UPDATER_H
#define UPDATER_UPDATER_H
#include <chrono>
#include <optional>
#include <string>
#include "misc_info/misc_info.h"
#include "package/packages_info.h"
#include "package/pkg_manager.h"

namespace Updater {
enum UpdaterStatus {
    UPDATE_ERROR = -1,
    UPDATE_SUCCESS,
    UPDATE_CORRUPT, /* package or verify failed, something is broken. */
    UPDATE_SKIP, /* skip update because of condition is not satisfied, e.g, battery is low */
    UPDATE_RETRY,
    UPDATE_SPACE_NOTENOUGH,
    UPDATE_UNKNOWN
};

using PostMessageFunction = std::function<void(const char *cmd, const char *content)>;

enum PackageUpdateMode {
    HOTA_UPDATE = 0,
    SDCARD_UPDATE,
    UNKNOWN_UPDATE,
};

struct UpdaterParams {
    bool forceUpdate = false;
    bool forceReboot = false;
    bool mainUpdate = false;
    bool sdUpdate = false;
    std::string factoryMode {};
    PackageUpdateMode updateMode = HOTA_UPDATE;
    int retryCount = 0;
    float initialProgress = 0; /* The upgrade starts at the progress bar location */
    float currentPercentage = 0; /* The proportion of progress bars occupied by the upgrade process */
    unsigned int pkgLocation = 0;
    std::string miscCmd {"boot_updater"};
    std::vector<std::string> updatePackage {};
    std::vector<std::chrono::duration<double>> installTime {};
    std::function<void(float)> callbackProgress {};
};

using CondFunc = std::function<bool(const UpdateMessage &)>;

using EntryFunc = std::function<int(int, char **)>;

struct BootMode {
    CondFunc cond {nullptr}; // enter mode condition
    std::string modeName {}; // mode name
    std::string modePara {}; // parameter config of mode
    EntryFunc entryFunc {nullptr}; // mode entry
    void InitMode(void) const; // mode related initialization
};

int32_t ExtractUpdaterBinary(Hpackage::PkgManager::PkgManagerPtr manager, std::string &packagePath,
    const std::string &updaterBinary);

int GetTmpProgressValue();
void SetTmpProgressValue(int value);

void ProgressSmoothHandler(int beginProgress, int endProgress);

UpdaterStatus DoInstallUpdaterPackage(Hpackage::PkgManager::PkgManagerPtr pkgManager,
    UpdaterParams &upParams, PackageUpdateMode updateMode);

UpdaterStatus StartUpdaterProc(Hpackage::PkgManager::PkgManagerPtr pkgManager,
    UpdaterParams &upParams, int &maxTemperature);

int GetUpdatePackageInfo(Hpackage::PkgManager::PkgManagerPtr pkgManager, const std::string& path);

int ExecUpdate(Hpackage::PkgManager::PkgManagerPtr pkgManager, int retry, const std::string &pkgPath,
    PostMessageFunction postMessage);

UpdaterStatus IsSpaceCapacitySufficient(const UpdaterParams &upParams);

std::vector<uint64_t> GetStashSizeList(const UpdaterParams &upParams);

void HandleChildOutput(const std::string &buffer, int32_t bufferLen, bool &retryUpdate, UpdaterParams &upParams);

int CheckStatvfs(const uint64_t totalPkgSize);

bool IsSDCardExist(const std::string &sdcard_path);

void PostUpdater(bool clearMisc);

bool DeleteUpdaterPath(const std::string &path);

bool ClearMisc();

std::string GetWorkPath();

bool IsUpdater(const UpdateMessage &boot);

bool IsFlashd(const UpdateMessage &boot);

void RegisterMode(const BootMode &mode);

std::vector<BootMode> &GetBootModes(void);

std::optional<BootMode> SelectMode(const UpdateMessage &boot);
} // Updater
#endif /* UPDATER_UPDATER_H */
