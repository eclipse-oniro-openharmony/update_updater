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

#include <dlfcn.h>
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

#ifndef UPDATER_UT
constexpr const char *HANDLE_MISC_LIB = "libupdater_handle_misc.z.so";
constexpr const char *HANDLE_MISC_INFO = "HandleUpdateMiscInfo";
constexpr const char *HANDLE_MISC_LIB_PATH = "/system/lib64/libupdater_handle_misc.z.so";

static void Handledlopen(const struct UpdateMessage &updateMsg, const std::string &upgradeType)
{
    if (!Utils::IsFileExist(HANDLE_MISC_LIB_PATH)) {
        LOG(WARNING) << "libupdater_handle_misc.z.so is not exist";
        return;
    }
    auto handle = Utils::LoadLibrary(HANDLE_MISC_LIB);
    if (handle == nullptr) {
        LOG(ERROR) << "load libupdater_handle_misc fail";
        return;
    }
    auto getFunc = (void(*)(const std::string &, const std::string &))Utils::GetFunction(handle, HANDLE_MISC_INFO);
    if (getFunc == nullptr) {
        LOG(ERROR) << "getFunc is nullptr";
        Utils::CloseLibrary(handle);
        return;
    }
    getFunc(updateMsg.update, upgradeType);
    Utils::CloseLibrary(handle);
}
#endif

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
    Handledlopen(updateMsg, "");
    WriteUpdaterMiscMsg(updateMsg);
    DoReboot("updater:reboot to updater to trigger update");
    while (true) {
        pause();
    }
#else
    return true;
#endif
}

static void WriteUpdaterResultFile(const std::string &pkgPath, const std::string &result)
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
    std::string resultInfo = pkgPath + "|fail|" + result + "||\n";
    if (fwrite(resultInfo.c_str(), resultInfo.size() + 1, 1, fp) <= 0) {
        LOG(WARNING) << "write updater result file failed, err:" << errno;
    }
    if (fsync(fileno(fp)) != 0) {
        LOG(WARNING) << "WriteUpdaterResultFile fsync failed" << strerror(errno);
    }
    if (fclose(fp) != 0) {
        LOG(WARNING) << "close updater result file failed";
    }

    (void)chown(resultPath.c_str(), Utils::USER_ROOT_AUTHORITY, Utils::GROUP_UPDATE_AUTHORITY);
    (void)chmod(resultPath.c_str(), 0660); // 0660: -rw-rw----
}

static std::string ParsePkgPath(const struct UpdateMessage &updateMsg)
{
    std::string pkgPath = "";
    std::string pathInfo(updateMsg.update, sizeof(updateMsg.update));
    std::string::size_type startPos = pathInfo.find("update_package=");
    std::string::size_type endPos = pathInfo.find(".zip");
    if (startPos != pathInfo.npos && endPos != pathInfo.npos) {
        startPos += strlen("update_package=");
        endPos += strlen(".zip");
        if (endPos > startPos) {
            pkgPath = pathInfo.substr(startPos, endPos - startPos);
        } else {
            LOG(ERROR) << "pkgPath invalid";
        }
    }
    return pkgPath;
}

static bool WriteToMiscAndResultFileRebootToUpdater(const struct UpdateMessage &updateMsg,
    const std::string &upgradeType)
{
    // Write package name to misc, then trigger reboot.
    const char *bootCmd = "boot_updater";
    int ret = strncpy_s(const_cast<char*>(updateMsg.command), sizeof(updateMsg.command), bootCmd,
        sizeof(updateMsg.command) - 1);
    if (ret != 0) {
        return false;
    }
    std::string pkgPath = ParsePkgPath(updateMsg);
    // Flag before the misc in written
    std::string writeMiscBefore = "0x80000000";
    WriteUpdaterResultFile(pkgPath, writeMiscBefore);
#ifndef UPDATER_UT
    Handledlopen(updateMsg, upgradeType);
    WriteUpdaterMiscMsg(updateMsg);
    // Flag after the misc in written
    std::string writeMiscAfter = "0x80000008";
    WriteUpdaterResultFile(pkgPath, writeMiscAfter);
    DoReboot("updater:reboot to updater to trigger update");
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
        path.find("--night_update") != std::string::npos ||
        path.find("--shrink_info=") != std::string::npos ||
        path.find("--virtual_shrink_info=") != std::string::npos) {
            return false;
        }
    return true;
}

static int AddPkgPath(struct UpdateMessage &msg, size_t updateOffset, const std::vector<std::string> &packageName)
{
    for (auto path : packageName) {
        if (updateOffset > sizeof(msg.update)) {
            LOG(ERROR) << "updaterkits: updateOffset > msg.update, return false";
            return 4; // 4 : path is too long
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
            return 5; // 5 : The library function is incorrect
        }
        updateOffset += static_cast<size_t>(ret);
    }
    return 0;
}

bool RebootAndInstallSdcardPackage(const std::string &miscFile, const std::vector<std::string> &packageName)
{
    struct UpdateMessage msg {};
    int ret = snprintf_s(msg.update, sizeof(msg.update), sizeof(msg.update) - 1, "--sdcard_update\n");
    if (ret < 0) {
        LOG(ERROR) << "updaterkits: copy updater message failed";
        return false;
    }

    if (packageName.size() != 0 && AddPkgPath(msg, static_cast<size_t>(ret), packageName) != 0) {
        LOG(ERROR) << "get sdcard pkg path fail";
        return false;
    }
    WriteToMiscAndRebootToUpdater(msg);

    // Never get here.
    return true;
}

int RebootAndInstallUpgradePackage(const std::string &miscFile, const std::vector<std::string> &packageName,
    const std::string &upgradeType)
{
    if (packageName.size() == 0 && upgradeType == UPGRADE_TYPE_OTA) {
        LOG(ERROR) << "updaterkits: invalid argument. one of arugments is empty";
        return 1; // 1 : Invalid input
    }

    for (auto path : packageName) {
        if (IsPackagePath(path)) {
            if (access(path.c_str(), R_OK) < 0) {
            LOG(ERROR) << "updaterkits: " << path << " is not readable";
            return 2; // 2 : pkg not exit
            }
        }
    }
    struct UpdateMessage updateMsg {};
    int ret = 0;
    if (upgradeType == UPGRADE_TYPE_SD) {
        ret = snprintf_s(updateMsg.update, sizeof(updateMsg.update), sizeof(updateMsg.update) - 1,
            "--sdcard_update\n");
    } else if (upgradeType == UPGRADE_TYPE_SD_INTRAL) {
        ret = snprintf_s(updateMsg.update, sizeof(updateMsg.update), sizeof(updateMsg.update) - 1,
            "--sdcard_intral_update\n");
    }
    if (ret < 0) {
        LOG(ERROR) << "updaterkits: copy updater message failed";
        return 3; // 3 : The library function is incorrect
    }
    int addRet = AddPkgPath(updateMsg, static_cast<size_t>(ret), packageName);
    if (addRet != 0) {
        return addRet;
    }
    if (upgradeType == UPGRADE_TYPE_OTA) {
        WriteToMiscAndResultFileRebootToUpdater(updateMsg, upgradeType);
    } else {
        WriteToMiscAndRebootToUpdater(updateMsg);
    }

    // Never get here.
    return 0;
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
