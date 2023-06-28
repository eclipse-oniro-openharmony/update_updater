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
            (currentName.compare(UPDATER_LOCALE_FILE) == 0)) {
            continue;
        }
        if (sdcardTmp && (currentName.compare(SDCARD_FULL_PACKAGE) == 0 ||
            currentName.compare(SDCARD_CUST_PACKAGE) == 0 || currentName.compare(SDCARD_PRELOAD_PACKAGE) == 0)) {
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

    (void)SetupPartitions();
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

    SaveLogs();
}

std::vector<std::string> ParseParams(int argc, char **argv)
{
    struct UpdateMessage boot {};
    // read from misc
    if (!ReadUpdaterMiscMsg(boot)) {
        LOG(ERROR) << "ReadUpdaterMessage MISC_FILE failed!";
    }
    // if boot.update is empty, read from command.The Misc partition may have dirty data,
    // so strlen(boot.update) is not used, which can cause system exceptions.
    if (boot.update[0] == '\0' && !access(COMMAND_FILE, 0)) {
        if (!ReadUpdaterMessage(COMMAND_FILE, boot)) {
            LOG(ERROR) << "ReadUpdaterMessage COMMAND_FILE failed!";
        }
    }
    STAGE(UPDATE_STAGE_OUT) << "Init Params: " << boot.update;
    boot.update[sizeof(boot.update) - 1] = '\0';
    std::vector<std::string> parseParams = Utils::SplitString(boot.update, "\n");
    if (argc != 0 && argv != nullptr) {
        parseParams.insert(parseParams.begin(), argv, argv + argc);
    }
    return parseParams;
}

void BootMode::InitMode(void) const
{
    InitUpdaterLogger(modeName, TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    SetLogLevel(INFO);
    LoadFstab();
    STAGE(UPDATE_STAGE_OUT) << "Start " << modeName;
    Flashd::SetParameter(modePara.c_str(), "1");
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
