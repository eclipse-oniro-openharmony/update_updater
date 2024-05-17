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
#include <sys/stat.h>
#include <unistd.h>
#include "init_reboot.h"
#include "log.h"
#include "misc_info/misc_info.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "utils_fs.h"

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

static void WriteUpdaterResultFile(const std::string &result)
{
    if (access(UPDATER_PATH, 0) != 0) {
        if (Utils::MkdirRecursive(UPDATER_PATH, 0755) != 0) { // 0755: -rwxr-xr-x
            LOG(ERROR) << "Mkdir recursive error!";
            return;
        }
    }
    LOG(INFO) << "WriteUpdaterResultFile: " << result;
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    FILE *fp = fopen(resultPath.c_str(), "w+");
    if (fp == nullptr) {
        LOG(ERROR) << "open updater result file failed";
        return;
    }
    char buf[MAX_RESULT_BUFF_SIZE] = "Pass\n";
    if (sprintf_s(buf, MAX_RESULT_BUFF_SIZE - 1, "%s\n", result.c_str()) < 0) {
        LOG(WARNING) << "sprintf status fialed";
    }
    if (fwrite(buf, 1, strlen(buf) + 1, fp) <= 0) {
        LOG(WARNING) << "write updater result file failed, err:" << errno;
    }
    if (fclose(fp) != 0) {
        LOG(WARNING) << "close updater result file failed";
    }

    (void)chown(resultPath.c_str(), Utils::USER_ROOT_AUTHORITY, Utils::GROUP_UPDATE_AUTHORITY);
    (void)chmod(resultPath.c_str(), 0660); // 0660: -rw-rw----
}

static bool WriteToMiscAndResultFileRebootToUpdater(const struct UpdateMessage &updateMsg)
{
    // Write package name to misc, then trigger reboot.
    const char *bootCmd = "boot_updater";
    int ret = strncpy_s(const_cast<char*>(updateMsg.command), sizeof(updateMsg.command), bootCmd,
        sizeof(updateMsg.command) - 1);
    if (ret != 0) {
        return false;
    }
    // Flag before the misc in written
    std::string writeMiscBefore = "0x80000000";
    WriteUpdaterResultFile(writeMiscBefore);
#ifndef UPDATER_UT
    WriteUpdaterMiscMsg(updateMsg);
    // Flag after the misc in written
    std::string writeMiscAfter = "0x80000008";
    WriteUpdaterResultFile(writeMiscAfter);
    DoReboot("updater");
    while (true) {
        pause();
    }
#else
    return true;
#endif
}

static bool IsPackagePath(const std::string &path)
{
    if (path.find("--force_update_action=") != std::string::npos ||
        path.find("--night_update") != std::string::npos) {
            return false;
        }
    return true;
}

static bool AddPkgPath(struct UpdateMessage &msg, size_t updateOffset, const std::vector<std::string> &packageName)
{
    for (auto path : packageName) {
        if (updateOffset > sizeof(msg.update)) {
            LOG(ERROR) << "updaterkits: updateOffset > msg.update, return false";
            return false;
        }
        int ret;
        if (IsPackagePath(path)) {
            ret = snprintf_s(msg.update + updateOffset, sizeof(msg.update) - updateOffset,
                sizeof(msg.update) - 1 - updateOffset, "--update_package=%s\n", path.c_str());
        } else {
            ret = snprintf_s(msg.update + updateOffset, sizeof(msg.update) - updateOffset,
                sizeof(msg.update) - 1 - updateOffset, "%s\n", path.c_str());
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

bool RebootAndInstallUpgradePackage(const std::string &miscFile, const std::vector<std::string> &packageName,
    const std::string &upgradeType)
{
    if (packageName.size() == 0 && upgradeType == UPGRADE_TYPE_OTA) {
        LOG(ERROR) << "updaterkits: invalid argument. one of arugments is empty";
        return false;
    }

    for (auto path : packageName) {
        if (IsPackagePath(path)) {
            if (access(path.c_str(), R_OK) < 0) {
            LOG(ERROR) << "updaterkits: " << path << " is not readable";
            return false;
            }
        }
    }
    struct UpdateMessage updateMsg {};
    int ret = 0;
    if (upgradeType == UPGRADE_TYPE_SD) {
        ret = snprintf_s(updateMsg.update, sizeof(updateMsg.update), sizeof(updateMsg.update) - 1, "--sdcard_update\n");
    } else if (upgradeType == UPGRADE_TYPE_SD_INTRAL) {
        ret = snprintf_s(updateMsg.update, sizeof(updateMsg.update), sizeof(updateMsg.update) - 1, "--sdcard_intral_update\n");
    }
    if (ret < 0) {
        LOG(ERROR) << "updaterkits: copy updater message failed";
        return false;
    }

    if (!AddPkgPath(updateMsg, 0, packageName)) {
        return false;
    }
    if (upgradeType == UPGRADE_TYPE_OTA) {
        WriteToMiscAndResultFileRebootToUpdater(updateMsg);
    } else {
        WriteToMiscAndRebootToUpdater(updateMsg);
    }

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
