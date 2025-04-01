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
#ifndef UPDATER_KITS_H
#define UPDATER_KITS_H
#include <string>

constexpr const char *UPGRADE_TYPE_OTA = "ota";
constexpr const char *UPGRADE_TYPE_SD = "sdcard";
constexpr const char *UPGRADE_TYPE_OTA_INTRAL = "ota_intral";
constexpr const char *UPGRADE_TYPE_SD_INTRAL = "sdcard_intral";
constexpr const char *UPGRADE_TYPE_SUBPKG_UPDATE = "subpkg_update";

// Reboot system to updater mode and trigger installing update package.
// @param packageName update package file name.
// @return returns true if trigger update package installing success, else returns false.
extern int RebootAndInstallUpgradePackage(const std::string &miscFile, const std::vector<std::string> &packageName,
    const std::string &upgradeType = UPGRADE_TYPE_OTA);

// Reboot system to sdcard update mode and trigger installing update package.
// @return returns true if trigger update package installing success, else returns false.
extern bool RebootAndInstallSdcardPackage(const std::string &miscFile, const std::vector<std::string> &packageName);

// Reboot system to updater mode and trigger userdata clean up.
// @return returns true if trigger userdata cleanup success, else returns false.
extern bool RebootAndCleanUserData(const std::string &miscFile, const std::string &cmd);
#endif /* UPDATER_KITS_H */
