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
#ifndef UPDATER_UT
#include "language/language_ui.h"
#endif
#include "log/dump.h"
#include "log/log.h"
#include "fs_manager/mount.h"
#include "securec.h"
#ifndef UPDATER_UT
#include "ui/updater_ui_stub.h"
#endif
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


bool DoMountSdCard(std::vector<std::string> &sdCardStr, std::string &mountPoint, UpdaterParams &upParams)
{
#ifndef UPDATER_UT
    bool mountSuccess = false;
    unsigned int retryTimes = 20; // Wait 20s
    if (upParams.sdExtMode == SDCARD_MAINIMG || upParams.sdExtMode == SDCARD_NORMAL_UPDATE) {
        retryTimes = 60; // Wait 60s
    }
    for (unsigned int retryCount = 1; retryCount <= retryTimes; retryCount++) {
        LOG(INFO) << "the retry time is: " << retryCount;
        for (auto item : sdCardStr) {
            if (MountSdcard(item, mountPoint) == 0) {
                mountSuccess = true;
                LOG(INFO) << "mount " << item << " sdcard success!";
                break;
            }
        }
        if (mountSuccess) {
            break;
        }
        sleep(1); // sleep 1 second to wait for sd card recognition
    }
    return mountSuccess;
#else
    return true;
#endif
}

UpdaterStatus FindAndMountSdcard(UpdaterParams &upParams)
{
#ifndef UPDATER_UT
    std::string mountPoint = std::string(SDCARD_PATH);
    std::vector<std::string> sdcardStr = GetBlockDevicesByMountPoint(mountPoint);
    if (sdcardStr.empty()) {
        UPDATER_UI_INSTANCE.ShowLog(
            (errno == ENOENT) ? TR(LOG_SDCARD_NOTFIND) : TR(LOG_SDCARD_ABNORMAL), true);
        return UPDATE_ERROR;
    }
    if (!DoMountSdCard(sdcardStr, mountPoint, upParams)) {
        LOG(ERROR) << "mount sdcard fail!";
        return UPDATE_ERROR;
    }
#endif
    return UPDATE_SUCCESS;
}

UpdaterStatus GetPkgsFromSdcard(UpdaterParams &upParams)
{
    if (FindAndMountSdcard(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "mount sdcard fail!";
        return UPDATE_ERROR;
    }
    if (GetSdcardPkgsPath(upParams) != UPDATE_SUCCESS) {
        LOG(ERROR) << "there is no package in sdcard/updater, please check";
        return UPDATE_ERROR;
    }
    return UPDATE_SUCCESS;
}

__attribute__((weak)) UpdaterStatus MountAndGetPkgs(UpdaterParams &upParams)
{
    return GetPkgsFromSdcard(upParams);
}

UpdaterStatus CheckSdcardPkgs(UpdaterParams &upParams)
{
#ifndef UPDATER_UT
    auto sdParam = "updater.data.configs";
    Utils::SetParameter(sdParam, "1");
#endif
    return MountAndGetPkgs(upParams);
}
} // Updater
