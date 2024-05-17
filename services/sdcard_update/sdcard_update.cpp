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
#include "sdcard_update.h"
#include <chrono>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "language/language_ui.h"
#include "log/dump.h"
#include "log/log.h"
#include "fs_manager/mount.h"
#include "securec.h"
#include "ui/updater_ui_stub.h"
#include "updater/updater_const.h"
#include "utils.h"

namespace Updater {
__attribute__((weak)) UpdaterStatus GetSdcardPkgsPath(UpdaterParams &upParams)
{
    if (upParams.updatePackage.size() != 0) {
        LOG(INFO) << "get sdcard packages from misc";
        return UPDATE_SUCCESS;
    }
    LOG(INFO) << "get sdcard packages from sdcard path";
    std::vector<std::string> sdcardPkgs = Utils::SplitString(SDCARD_CARD_PKG_PATH, ", ");
    for (auto pkgPath : sdcardPkgs) {
        if (access(pkgPath.c_str(), 0) == 0) {
            LOG(INFO) << "find sdcard package : " << pkgPath;
            upParams.updatePackage.push_back(pkgPath);
        }
    }
    if (upParams.updatePackage.size() == 0) {
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

__attribute__((weak)) UpdaterStatus GetSdcardPkgsFromDev(UpdaterParams &upParams)
{
    LOG(INFO) << "not implemented get sdcard pkgs from dev";
    return UPDATE_ERROR;
}

bool CheckPathNeedMountSD(UpdaterParams &upParams)
{
    for (auto pkgPath : upParams.updatePackage) {
        if (pkgPath.find("/sdcard") != 0) {
            return false;
        }
    }
    return true;
}

static bool MountSdcard(std::vector<std::string> &sdcardStr)
{
#ifndef UPDATER_UT
    bool mountSuccess = false;
    unsigned int retryTimes = 20;
    for (unsigned int retryCount = 1; retryCount <= retryTimes; retryCount++) {
        LOG(INFO) << "the retry time is: " << retryCount;
        for (auto item : sdcardStr) {
            if (MountSdcard(item, mountPoint) == 0) {
                mountSuccess = true;
                LOG(INFO) << "mount " << item << " sdcard success!";
                break;
            }
        }
        if (mountSuccess) {
            break;
        }
        sleep(1); // sleep 1 second to wait for sd card recongnition
    }
#endif
    return mountSuccess;
}

UpdaterStatus CheckSdcardPkgs(UpdaterParams &upParams)
{
#ifndef UPDATER_UT
    auto sdParam = "updater.data.configs";
    Utils::SetParameter(sdParam, "1");
    if (upParams.sdExtMode == SDCARD_UPDATE_FROM_DEV && GetSdcardPkgsFromDev(upParams) == UPDATE_SUCCESS) {
        LOG(INFO) << "get sd card from dev succeed, skip get package from sd card";
        return UPDATE_SUCCESS;
    }
    std::string mountPoint = std::string(SDCARD_PATH);
    std::vector<std::string> sdcardStr = GetBlockDevicesByMountPoint(mountPoint);
    if (sdcardStr.empty()) {
        UPDATER_UI_INSTANCE.ShowLog(
            (errno == ENOENT) ? TR(LOG_SDCARD_NOTFIND) : TR(LOG_SDCARD_ABNORMAL), true);
        return UPDATE_ERROR;
    }
    if (Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE)) {
        if (MountForPath("/data") != 0) {
            LOG(ERROR) << "data partition mount fail";
            return UPDATE_ERROR;
        }
    }
    if ((Utils::CheckUpdateMode(Updater::SDCARD_MODE) && !Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE)) ||
        (Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE) && CheckPathNeedMountSD(upParams))) {
            if (!MountSdcard(sdcardStr)) {
                LOG(ERROR) << "mount sdcard fail!";
                return UPDATE_ERROR;
            }
        }
#endif
    if (GetSdcardPkgsPath(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "there is no package in sdcard/updater, please check";
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}
} // Updater
