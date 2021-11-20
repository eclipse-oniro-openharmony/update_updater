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

namespace updater {
using namespace hpackage;
using namespace updater::utils;
static constexpr int USER_ROOT_AUTHORITY = 0;
static constexpr int GROUP_SYS_AUTHORITY = 1000;

void CompressLogs(const std::string &name)
{
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "pkgManager is nullptr", return);
    std::vector<std::pair<std::string, ZipFileInfo>> files;
    // Build the zip file to be packaged
    std::vector<std::string> testFileNames;
    std::string realName = name.substr(name.find_last_of("/") + 1);
    testFileNames.push_back(realName);
    for (auto name : testFileNames) {
        ZipFileInfo file;
        file.fileInfo.identity = name;
        file.fileInfo.packMethod = PKG_COMPRESS_METHOD_ZIP;
        file.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        std::string fileName = "/data/updater/log/" + name;
        files.push_back(std::pair<std::string, ZipFileInfo>(fileName, file));
    }

    PkgInfo pkgInfo;
    pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
    pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;
    pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;

    char realTime[MAX_TIME_SIZE] = { 0 };
    auto sysTime = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(sysTime);
    struct tm *localTime = std::localtime(&currentTime);
    if (localTime != nullptr) {
        std::strftime(realTime, sizeof(realTime), "%H_%M_%S", localTime);
    }
    char pkgName[MAX_LOG_NAME_SIZE];
    UPDATER_CHECK_ONLY_RETURN(snprintf_s(pkgName, MAX_LOG_NAME_SIZE, MAX_LOG_NAME_SIZE - 1,
        "/data/updater/log/%s_%s.zip", realName.c_str(), realTime) != -1, return);
    int32_t ret = pkgManager->CreatePackage(pkgName, GetCertName(), &pkgInfo, files);
    UPDATER_CHECK_ONLY_RETURN(ret != 0, return);
    UPDATER_CHECK_ONLY_RETURN(DeleteFile(name) == 0, return);
}

bool CopyUpdaterLogs(const std::string &sLog, const std::string &dLog)
{
    UPDATER_WARING_CHECK(MountForPath(UPDATER_LOG_DIR) == 0, "MountForPath /data/log failed!", return false);
    if (access(UPDATER_LOG_DIR.c_str(), 0) != 0) {
        UPDATER_ERROR_CHECK(!MkdirRecursive(UPDATER_LOG_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH),
            "MkdirRecursive error!", return false);
        UPDATER_ERROR_CHECK(chown(UPDATER_PATH.c_str(), USER_ROOT_AUTHORITY, GROUP_SYS_AUTHORITY) == 0,
            "Chown failed!", return false);
        UPDATER_ERROR_CHECK(chmod(UPDATER_PATH.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0,
            "Chmod failed!", return false);
    }

    FILE *dFp = fopen(dLog.c_str(), "ab+");
    UPDATER_ERROR_CHECK(dFp != nullptr, "open log failed", return false);

    FILE *sFp = fopen(sLog.c_str(), "r");
    UPDATER_ERROR_CHECK(sFp != nullptr, "open log failed", fclose(dFp);
        return false);

    char buf[MAX_LOG_BUF_SIZE];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), sFp)) != 0) {
        int ret = fwrite(buf, 1, bytes, dFp);
        if (ret < 0) {
            break;
        }
    }
    (void)fseek(dFp, 0, SEEK_END);
    UPDATER_INFO_CHECK(ftell(dFp) < MAX_LOG_SIZE, "log size greater than 5M!", CompressLogs(dLog));
    sync();
    (void)fclose(sFp);
    (void)fclose(dFp);
    return true;
}

static bool IsDir(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) < 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool DeleteUpdaterPath(const std::string &path)
{
    auto pDir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    UPDATER_INFO_CHECK_NOT_RETURN(pDir != nullptr, "Can not open dir");
    if (pDir == nullptr) {
        return true;
    }
    struct dirent *dp = nullptr;
    while ((dp = readdir(pDir.get())) != nullptr) {
        std::string currentName(dp->d_name);
        if (currentName[0] != '.' && (currentName.compare("log") != 0)) {
            std::string tmpName(path);
            tmpName.append("/" + currentName);
            if (IsDir(tmpName)) {
                DeleteUpdaterPath(tmpName);
            }
#ifndef UPDATER_UT
            remove(tmpName.c_str());
#endif
        }
    }
    return true;
}

static bool ClearMisc()
{
    struct UpdateMessage cleanBoot {};
    UPDATER_ERROR_CHECK(WriteUpdaterMessage(MISC_FILE, cleanBoot) == true,
        "ClearMisc clear boot message to misc failed", return false);
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(MISC_FILE.c_str(), "rb+"), fclose);
    UPDATER_FILE_CHECK(fp != nullptr, "WriteVersionCode fopen failed", return false);
    (void)fseek(fp.get(), PARTITION_RECORD_OFFSET, SEEK_SET);
    off_t clearOffset = 0;
    UPDATER_FILE_CHECK(fwrite(&clearOffset, sizeof(off_t), 1, fp.get()) == 1,
        "ClearMisc write misc initOffset 0 failed", return false);

    struct PartitionRecordInfo cleanPartition {};
    for (size_t tmpOffset = 0; tmpOffset < PARTITION_UPDATER_RECORD_MSG_SIZE; tmpOffset +=
        sizeof(PartitionRecordInfo)) {
        (void)fseek(fp.get(), PARTITION_RECORD_START + tmpOffset, SEEK_SET);
        UPDATER_FILE_CHECK(fwrite(&cleanPartition, sizeof(PartitionRecordInfo), 1, fp.get()) == 1,
            "ClearMisc write misc cleanPartition failed", return false);
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

void PostUpdaterForSdcard(std::string &updaterLogPath, std::string &stageLogPath, std::string &errorCodePath)
{
    if (SetupPartitions() != 0) {
        ShowText(GetUpdateInfoLabel(), "Mount data failed.");
        LOG(ERROR) << "Mount for /data failed.";
        std::string sdcardPath = GetBlockDeviceByMountPoint(SDCARD_PATH);
        if (IsSDCardExist(sdcardPath)) {
            if (MountForPath(SDCARD_PATH) != 0) {
                int ret = mount(sdcardPath.c_str(), SDCARD_PATH.c_str(), "vfat", 0, NULL);
                UPDATER_WARING_CHECK(ret == 0, "Mount for /sdcard failed!", return);
            }
            updaterLogPath = "/sdcard/updater/log/updater_log";
            stageLogPath = "/sdcard/updater/log/updater_stage_log";
            errorCodePath = "/sdcard/updater/log/error_code.log";
        }
    }
    return;
}

void PostUpdater(bool clearMisc)
{
    STAGE(UPDATE_STAGE_BEGIN) << "PostUpdater";
    std::string updaterLogPath = "/data/updater/log/updater_log";
    std::string stageLogPath = "/data/updater/log/updater_stage_log";
    std::string errorCodePath = "/data/updater/log/error_code.log";
    PostUpdaterForSdcard(updaterLogPath, stageLogPath, errorCodePath);
    // clear update misc partition.
    if (clearMisc) {
        UPDATER_ERROR_CHECK_NOT_RETURN(ClearMisc() == true, "PostUpdater clear misc failed");
    }
    if (!access(COMMAND_FILE.c_str(), 0)) {
        UPDATER_ERROR_CHECK_NOT_RETURN(unlink(COMMAND_FILE.c_str()) == 0, "Delete command failed");
    }

    // delete updater tmp files
    if (access(UPDATER_PATH.c_str(), 0) == 0 && access(SDCARD_CARD_PATH.c_str(), 0) != 0) {
        UPDATER_ERROR_CHECK_NOT_RETURN(DeleteUpdaterPath(UPDATER_PATH), "DeleteUpdaterPath failed");
    }
    if (!access(SDCARD_CARD_PATH.c_str(), 0)) {
        UPDATER_ERROR_CHECK_NOT_RETURN(DeleteUpdaterPath(SDCARD_CARD_PATH), "Delete sdcard path failed");
    }
    if (access(flashd::FLASHD_FILE_PATH.c_str(), 0) == 0) {
        UPDATER_ERROR_CHECK_NOT_RETURN(DeleteUpdaterPath(flashd::FLASHD_FILE_PATH), "DeleteUpdaterPath failed");
    }

    // save logs
    bool ret = CopyUpdaterLogs(TMP_LOG, updaterLogPath);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy updater log failed!");
    ret = CopyUpdaterLogs(TMP_ERROR_CODE_PATH, errorCodePath);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy error code log failed!");
    ret = CopyUpdaterLogs(flashd::FLASHD_HDC_LOG_PATH, UPDATER_HDC_LOG);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy error hdc log failed!");

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    chmod(updaterLogPath.c_str(), mode);
    chmod(stageLogPath.c_str(), mode);
    chmod(errorCodePath.c_str(), mode);
    STAGE(UPDATE_STAGE_SUCCESS) << "PostUpdater";
    ret = CopyUpdaterLogs(TMP_STAGE_LOG, stageLogPath);
    UPDATER_ERROR_CHECK_NOT_RETURN(ret, "Copy stage log failed!");
}

int IsSpaceCapacitySufficient(const std::string &packagePath)
{
    hpackage::PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    UPDATER_ERROR_CHECK(pkgManager != nullptr, "pkgManager is nullptr", return UPDATE_CORRUPT);
    std::vector<std::string> fileIds;
    int ret = pkgManager->LoadPackageWithoutUnPack(packagePath, fileIds);
    UPDATER_ERROR_CHECK(ret == PKG_SUCCESS, "LoadPackageWithoutUnPack failed",
        PkgManager::ReleasePackageInstance(pkgManager); return UPDATE_CORRUPT);
    const FileInfo *info = pkgManager->GetFileInfo("update.bin");
    UPDATER_ERROR_CHECK(info != nullptr, "update.bin is not exist",
        PkgManager::ReleasePackageInstance(pkgManager); return UPDATE_CORRUPT);
    uint64_t needSpace = static_cast<uint64_t>(info->unpackedSize);
    PkgManager::ReleasePackageInstance(pkgManager);

    struct statvfs64 updaterVfs {};
    if (strncmp(packagePath.c_str(), SDCARD_CARD_PATH.c_str(), SDCARD_CARD_PATH.size()) == 0) { // for sdcard
        ret = statvfs64(SDCARD_PATH.c_str(), &updaterVfs);
        UPDATER_ERROR_CHECK(ret >= 0, "Statvfs read /sdcard error!", return UPDATE_ERROR);
    } else {
        needSpace += MAX_LOG_SPACE;
        ret = statvfs64(UPDATER_PATH.c_str(), &updaterVfs);
        UPDATER_ERROR_CHECK(ret >= 0, "Statvfs read /data error!", return UPDATE_ERROR);
    }
    auto freeSpaceSize = static_cast<uint64_t>(updaterVfs.f_bfree);
    auto blockSize = static_cast<uint64_t>(updaterVfs.f_bsize);
    uint64_t totalFreeSize = freeSpaceSize * blockSize;
    UPDATER_ERROR_CHECK(totalFreeSize > needSpace,
        "Can not update, free space is not enough", return UPDATE_SPACE_NOTENOUGH);
    return UPDATE_SUCCESS;
}

std::vector<std::string> ParseParams(int argc, char **argv)
{
    struct UpdateMessage boot {};
    // read from misc
    UPDATER_ERROR_CHECK_NOT_RETURN(ReadUpdaterMessage(MISC_FILE, boot) == true,
        "ReadUpdaterMessage MISC_FILE failed!");
    // if boot.update is empty, read from command.The Misc partition may have dirty data,
    // so strlen(boot.update) is not used, which can cause system exceptions.
    if (boot.update[0] == '\0' && !access(COMMAND_FILE.c_str(), 0)) {
        UPDATER_ERROR_CHECK_NOT_RETURN(ReadUpdaterMessage(COMMAND_FILE, boot) == true,
                                       "ReadUpdaterMessage COMMAND_FILE failed!");
    }
    STAGE(UPDATE_STAGE_OUT) << "Init Params: " << boot.update;
    std::vector<std::string> parseParams(argv, argv + argc);
    boot.update[sizeof(boot.update) - 1] = '\0';
    std::vector<std::string> parseParams1 = utils::SplitString(boot.update, "\n");
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
    bool ret = ReadUpdaterMessage(MISC_FILE, boot);
    if (!ret) {
        return -1;
    }
    // if boot.update is empty, read from command.The Misc partition may have dirty data,
    // so strlen(boot.update) is not used, which can cause system exceptions.
    if (boot.update[0] == '\0' && !access(COMMAND_FILE.c_str(), 0)) {
        ret = ReadUpdaterMessage(COMMAND_FILE, boot);
        if (!ret) {
            return -1;
        }
    }
    if (memcmp(boot.command, "boot_flash", strlen("boot_flash")) == 0) {
        mode = BOOT_FLASHD;
    }
    return 0;
}
} // namespace updater
