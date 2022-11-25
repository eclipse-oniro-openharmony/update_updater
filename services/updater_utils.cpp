/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "updater_ui.h"
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
    struct dirent *dp = nullptr;
    while ((dp = readdir(pDir.get())) != nullptr) {
        std::string currentName(dp->d_name);
        if (currentName[0] != '.' && (currentName.compare("log") != 0) &&
            (currentName.compare(UPDATER_RESULT_FILE) != 0) &&
            (currentName.compare(UPDATER_LOCALE_FILE) != 0)) {
            std::string tmpName(path);
            tmpName.append("/" + currentName);
            if (IsDirExist(tmpName)) {
                DeleteUpdaterPath(tmpName);
            }
#ifndef UPDATER_UT
            remove(tmpName.c_str());
#endif
        }
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
    std::string updaterLogPath = UPDATER_LOG;
    std::string stageLogPath = UPDATER_STAGE_LOG;
    std::string errorCodePath = ERROR_CODE_PATH;

    (void)SetupPartitions();
    // clear update misc partition.
    if (clearMisc) {
        UPDATER_ERROR_CHECK_NOT_RETURN(ClearMisc() == true, "PostUpdater clear misc failed");
    }
    if (!access(COMMAND_FILE, 0)) {
        UPDATER_ERROR_CHECK_NOT_RETURN(unlink(COMMAND_FILE) == 0, "Delete command failed");
    }

    // delete updater tmp files
    if (access(UPDATER_PATH, 0) == 0 && access(SDCARD_CARD_PATH, 0) != 0) {
        UPDATER_ERROR_CHECK_NOT_RETURN(DeleteUpdaterPath(UPDATER_PATH), "DeleteUpdaterPath failed");
    }
    if (!access(SDCARD_CARD_PATH, 0)) {
        UPDATER_ERROR_CHECK_NOT_RETURN(DeleteUpdaterPath(SDCARD_CARD_PATH), "Delete sdcard path failed");
    }
    if (access(Flashd::FLASHD_FILE_PATH, 0) == 0) {
        UPDATER_ERROR_CHECK_NOT_RETURN(DeleteUpdaterPath(Flashd::FLASHD_FILE_PATH), "DeleteUpdaterPath failed");
    }

    // save logs
    bool ret = CopyUpdaterLogs(TMP_LOG, updaterLogPath);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy updater log failed!");
    ret = CopyUpdaterLogs(TMP_ERROR_CODE_PATH, errorCodePath);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy error code log failed!");
    ret = CopyUpdaterLogs(Flashd::FLASHD_HDC_LOG_PATH, UPDATER_HDC_LOG);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy error hdc log failed!");

    mode_t mode = 0640;
    chmod(updaterLogPath.c_str(), mode);
    chmod(UPDATER_HDC_LOG, mode);
    chmod(errorCodePath.c_str(), mode);
    STAGE(UPDATE_STAGE_SUCCESS) << "PostUpdater";
    ret = CopyUpdaterLogs(TMP_STAGE_LOG, stageLogPath);
    chmod(stageLogPath.c_str(), mode);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy stage log failed!");
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
    std::vector<std::string> parseParams(argv, argv + argc);
    boot.update[sizeof(boot.update) - 1] = '\0';
    std::vector<std::string> parseParams1 = Utils::SplitString(boot.update, "\n");
    parseParams.insert(parseParams.end(), parseParams1.begin(), parseParams1.end());
    return parseParams;
}

int GetBootMode(int &mode)
{
#ifndef UPDATER_UT
    mode = BOOT_UPDATER;
#else
    mode = BOOT_FLASHD;
#endif
    struct UpdateMessage boot {};
    // read from misc
    bool ret = ReadUpdaterMiscMsg(boot);
    if (!ret) {
        return -1;
    }

    LOG(INFO) << "GetBootMode, boot.update" << boot.update;
    if (strncmp(boot.update, "boot_flash", strlen("boot_flash")) == 0) {
        mode = BOOT_FLASHD;
    }
    return 0;
}
} // namespace Updater
