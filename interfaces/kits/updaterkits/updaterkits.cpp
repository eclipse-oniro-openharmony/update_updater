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
#include "updaterkits/updaterkits.h"

#include <string>
#include <unistd.h>
#include "init_reboot.h"
#include "log.h"
#include "misc_info/misc_info.h"
#include "securec.h"
#include "utils.h"

using namespace Updater;
using Updater::Utils::SplitString;

static bool WriteToMiscAndRebootToUpdater(const struct UpdateMessage &updateMsg)
{
    // Write package name to misc, then trigger reboot.
    const char *bootCmd = "boot_updater";
    int ret = strncpy_s(const_cast<char*>(updateMsg.command), sizeof(updateMsg.command), bootCmd,
        sizeof(updateMsg.command) - 1);
    if (ret != 0) {
        return false;
    }
#ifndef UPDATER_UT
    WriteUpdaterMiscMsg(updateMsg);
    DoReboot("updater");
    while (true) {
        pause();
    }
#else
    return true;
#endif
}


bool IsUpdateCommand(const std::string &path)
{
    if (path.find("--force_update_action=") != std::string::npos ||
        path.find("--night_update") != std::string::npos) {
            return true;
        }
    return false;
}

static bool AddPkgPath(struct UpdateMessage &msg, size_t updateOffset, const std::vector<std::string> &packageName)
{
    for (auto path : packageName) {
        if (updateOffset > sizeof(msg.update)) {
            LOG(ERROR) << "updaterkits: updateOffset > msg.update, return false";
            return false;
        }
        int ret;
        if (IsUpdateCommand(path)) {
            ret = snprintf_s(msg.update + updateOffset, sizeof(msg.update) - updateOffset,
                sizeof(msg.update) - 1 - updateOffset, "%s\n", path.c_str());
        } else {
            ret = snprintf_s(msg.update + updateOffset, sizeof(msg.update) - updateOffset,
                sizeof(msg.update) - 1 - updateOffset, "--update_package=%s\n", path.c_str());
        }
        if (ret < 0) {
            LOG(ERROR) << "updaterkits: copy updater message failed";
            return false;
        }
        updateOffset += static_cast<size_t>(ret);
    }
    return true;
}

bool RebootAndInstallSdcardPackage(const std::string &miscFile, const std::vector<std::string> &packageName)
{
    struct UpdateMessage msg {};
    int ret = snprintf_s(msg.update, sizeof(msg.update), sizeof(msg.update) - 1, "--sdcard_update\n");
    if (ret < 0) {
        LOG(ERROR) << "updaterkits: copy updater message failed";
        return false;
    }

    if (packageName.size() != 0 && !AddPkgPath(msg, static_cast<size_t>(ret), packageName)) {
        LOG(ERROR) << "get sdcard pkg path fail";
        return false;
    }
    WriteToMiscAndRebootToUpdater(msg);

    // Never get here.
    return true;
}

bool RebootAndInstallUpgradePackage(const std::string &miscFile, const std::vector<std::string> &packageName)
{
    if (packageName.size() == 0) {
        LOG(ERROR) << "updaterkits: invalid argument. one of arugments is empty";
        return false;
    }

    for (auto path : packageName) {
        if (IsUpdateCommand(path)) {
            continue;
        }
        if (access(path.c_str(), R_OK) < 0) {
            LOG(ERROR) << "updaterkits: " << path << " is not readable";
            return false;
        }
    }
    struct UpdateMessage updateMsg {};
    if (!AddPkgPath(updateMsg, 0, packageName)) {
        return false;
    }

    WriteToMiscAndRebootToUpdater(updateMsg);

    // Never get here.
    return true;
}

bool RebootAndCleanUserData(const std::string &miscFile, const std::string &cmd)
{
    if (miscFile.empty() || cmd.empty()) {
        LOG(ERROR) << "updaterkits: invalid argument. one of arugments is empty";
        return false;
    }

    // Write package name to misc, then trigger reboot.
    struct UpdateMessage updateMsg {};
    if (strncpy_s(updateMsg.update, sizeof(updateMsg.update), cmd.c_str(), cmd.size()) != EOK) {
        LOG(ERROR) << "updaterkits: copy updater message failed";
        return false;
    }

    WriteToMiscAndRebootToUpdater(updateMsg);

    // Never get here.
    return true;
}
