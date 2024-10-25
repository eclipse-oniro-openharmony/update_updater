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
#include <chrono>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <regex>

#include "applypatch/partition_record.h"
#include "flashd/flashd.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "package/pkg_manager.h"
#include "securec.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "updater_main.h"
#include "utils.h"

namespace Updater {
using namespace Hpackage;
using namespace Updater::Utils;

void DeleteInstallTimeFile()
{
    const std::string installTimeFilePath = std::string(UPDATER_PATH) + "/" + std::string(INSTALL_TIME_FILE);
    if (access(installTimeFilePath.c_str(), F_OK) != -1) {
        (void)DeleteFile(installTimeFilePath);
        LOG(INFO) << "delete install time file";
    }
}

bool IsDouble(const std::string& str)
{
    std::regex pattern("^[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?$");
    return std::regex_match(str, pattern);
}
 
void WriteInstallTime(UpdaterParams &upParams)
{
    std::ofstream ofs;
    ofs.open(std::string(UPDATER_PATH) + "/" + std::string(INSTALL_TIME_FILE), std::ios::app | std::ios::out);
    if (!ofs.is_open()) {
        LOG(ERROR) << "open install time file fail";
        return;
    }
    ofs << DurationToString(upParams.installTime, upParams.pkgLocation) << "\n";
}
 
void ReadInstallTime(UpdaterParams &upParams)
{
    std::ifstream ifs;
    std::string buf;
    ifs.open(std::string(UPDATER_PATH) + "/" + std::string(INSTALL_TIME_FILE), std::ios::in);
    if (!ifs.is_open()) {
        LOG(ERROR) << "read install time file fail";
        return;
    }
    unsigned int index = 0;
    while (getline(ifs, buf)) {
        if (index >= upParams.pkgLocation) {
            break;
        }
        if (IsDouble(buf)) {
            upParams.installTime[index++] = std::chrono::duration<double>(std::stod(buf));
        } else {
            LOG(ERROR) << "read install time is invalid";
        }
    }
}

bool DeleteUpdaterPath(const std::string &path)
{
    auto pDir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    if (pDir == nullptr) {
        LOG(INFO) << "Can not open dir";
        return true;
    }
    bool sdcardTmp = false;
    if (path.find("sdcard") != std::string::npos) {
        sdcardTmp = true;
    }
    struct dirent *dp = nullptr;
    while ((dp = readdir(pDir.get())) != nullptr) {
        std::string currentName(dp->d_name);
        if (currentName[0] == '.' || (currentName.compare("log") == 0) ||
            (currentName.compare(UPDATER_RESULT_FILE) == 0) ||
            (currentName.compare(UPDATER_LOCALE_FILE) == 0) ||
            (currentName.compare(MODULE_UPDATE_RESULT_FILE) == 0) ||
            (currentName.compare(UPLOAD_LOG_TIME_FILE) == 0)) {
            continue;
        }
        if (sdcardTmp && currentName.find(SDCARD_PACKAGE_SUFFIX) != std::string::npos) {
            continue;
        }
        std::string tmpName(path);
        tmpName.append("/" + currentName);
        if (IsDirExist(tmpName)) {
            DeleteUpdaterPath(tmpName);
        }
#ifndef UPDATER_UT
        remove(tmpName.c_str());
#endif
    }
    return true;
}

bool ClearMisc()
{
    struct UpdateMessage cleanBoot {};
    if (!WriteUpdaterMiscMsg(cleanBoot)) {
        LOG(ERROR) << "ClearMisc clear boot message to misc failed";
        return false;
    }
    auto miscBlockDev = GetBlockDeviceByMountPoint(MISC_PATH);
    if (miscBlockDev.empty()) {
        LOG(INFO) << "cannot get block device of partition";
        miscBlockDev = MISC_FILE;
    }
    LOG(INFO) << "ClearMisc::misc path : " << miscBlockDev;
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(miscBlockDev.c_str(), "rb+"), fclose);
    if (fp == nullptr) {
        LOG(ERROR) << "WriteVersionCode fopen failed" << " : " << strerror(errno);
        return false;
    }
    if (fseek(fp.get(), PARTITION_RECORD_OFFSET, SEEK_SET) != 0) {
        LOG(ERROR) << "ClearMisc fseek failed";
        return false;
    }
    off_t clearOffset = 0;
    if (fwrite(&clearOffset, sizeof(off_t), 1, fp.get()) != 1) {
        LOG(ERROR) << "ClearMisc write misc initOffset 0 failed" << " : " << strerror(errno);
        return false;
    }

    struct PartitionRecordInfo cleanPartition {};
    for (size_t tmpOffset = 0; tmpOffset < PARTITION_UPDATER_RECORD_MSG_SIZE; tmpOffset +=
        sizeof(PartitionRecordInfo)) {
        if (fseek(fp.get(), PARTITION_RECORD_START + tmpOffset, SEEK_SET) != 0) {
            LOG(ERROR) << "ClearMisc fseek failed";
            return false;
        }
        if (fwrite(&cleanPartition, sizeof(PartitionRecordInfo), 1, fp.get()) != 1) {
            LOG(ERROR) << "ClearMisc write misc cleanPartition failed" << " : " << strerror(errno);
            return false;
        }
    }
    return true;
}

bool IsSDCardExist(const std::string &sdcardPath)
{
    // Record system error codes.
    int save_errno = errno;
    struct stat st {};
    if (stat(sdcardPath.c_str(), &st) < 0) {
        return false;
    } else {
        errno = save_errno;
        return true;
    }
}

void PostUpdater(bool clearMisc)
{
    STAGE(UPDATE_STAGE_BEGIN) << "PostUpdater";

    if ((!(CheckUpdateMode(SDCARD_MODE) || CheckUpdateMode(USB_MODE)) &&
        (CheckUpdateMode(OTA_MODE) || GetMountStatusForMountPoint("/log") != MountStatus::MOUNT_MOUNTED)
        || CheckUpdateMode(SDCARD_INTRAL_MODE)) {
        (void)SetupPartitions();
    } else {
        (void)SetupPartitions(false);
    }
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_POST_INIT_EVENT);
    // clear update misc partition.
    if (clearMisc && !ClearMisc()) {
        LOG(ERROR) << "PostUpdater clear misc failed";
    }
    if (!access(COMMAND_FILE, 0) && unlink(COMMAND_FILE) != 0) {
        LOG(ERROR) << "Delete command failed";
    }

    // delete updater tmp files
    if (access(UPDATER_PATH, 0) == 0 && access(SDCARD_CARD_PATH, 0) != 0 && !DeleteUpdaterPath(UPDATER_PATH)) {
        LOG(ERROR) << "DeleteUpdaterPath failed";
    }
    if (access(SDCARD_CARD_PATH, 0) == 0 && !DeleteUpdaterPath(SDCARD_CARD_PATH)) {
        LOG(ERROR) << "Delete sdcard path failed";
    }
    if (access(Flashd::FLASHD_FILE_PATH, 0) == 0 && !DeleteUpdaterPath(Flashd::FLASHD_FILE_PATH)) {
        LOG(ERROR) << "DeleteUpdaterPath failed";
    }
    if (!CheckUpdateMode(SDCARD_MODE) && !CheckUpdateMode(USB_MODE) &&
        GetMountStatusForMountPoint("/log") != MountStatus::MOUNT_MOUNTED) {
        SaveLogs();
    }
}

void BootMode::InitMode(void) const
{
    InitLogger(modeName);
#ifdef UPDATER_BUILD_VARIANT_USER
    SetLogLevel(INFO);
#else
    SetLogLevel(DEBUG);
#endif
    LoadFstab();
    STAGE(UPDATE_STAGE_OUT) << "Start " << modeName;
    SetParameter(modePara.c_str(), "1");
}

bool IsUpdater(const UpdateMessage &boot)
{
    return !IsFlashd(boot) && strncmp(boot.command, "boot_updater", sizeof("boot_updater") - 1) == 0;
}

bool IsFlashd(const UpdateMessage &boot)
{
    return strncmp(boot.update, "boot_flash", sizeof("boot_flash") - 1) == 0;
}

std::vector<BootMode> &GetBootModes(void)
{
    static std::vector<BootMode> bootModes {};
    return bootModes;
}

void RegisterMode(const BootMode &mode)
{
    GetBootModes().push_back(mode);
}

std::optional<BootMode> SelectMode(const UpdateMessage &boot)
{
    const auto &modes = GetBootModes();

    // select modes by bootMode.cond which would check misc message
    auto it = std::find_if(modes.begin(), modes.end(), [&boot] (const auto &bootMode) {
        if (bootMode.cond != nullptr && bootMode.cond(boot)) {
            LOG(INFO) << "condition for mode " << bootMode.modeName << " is satisfied";
            return true;
        }
        LOG(WARNING) << "condition for mode " << bootMode.modeName << " is not satisfied";
        return false;
    });
    // misc check failed for each mode, then enter updater mode
    if (it == modes.end() || it->entryFunc == nullptr) {
        LOG(WARNING) << "find valid mode failed, enter updater Mode";
        return std::nullopt;
    }

    LOG(INFO) << "enter " << it->modeName << " mode";
    return *it;
}
} // namespace Updater
