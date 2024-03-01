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

#include "utils.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <linux/reboot.h>
#include <string>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "fs_manager/mount.h"
#include "init_reboot.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#include "selinux/selinux.h"
#endif
#include "package/pkg_manager.h"
#include "securec.h"
#include "updater/updater_const.h"

namespace Updater {
using namespace Hpackage;

namespace Utils {
constexpr uint8_t SHIFT_RIGHT_FOUR_BITS = 4;
constexpr int USECONDS_PER_SECONDS = 1000000; // 1s = 1000000us
constexpr int NANOSECS_PER_USECONDS = 1000; // 1us = 1000ns
constexpr int MAX_TIME_SIZE = 20;
constexpr const char *PREFIX_PARTITION_NODE = "/dev/block/by-name/";

namespace {
void UpdateInfoInMisc(const std::string headInfo, const std::optional<int> message, bool isRemove)
{
    if (headInfo.empty()) {
        return;
    }
    std::vector<std::string> args = Utils::ParseParams(0, nullptr);
    struct UpdateMessage msg {};
    if (!ReadUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "SetMessageToMisc read misc failed";
        return;
    }

    (void)memset_s(msg.update, sizeof(msg.update), 0, sizeof(msg.update));
    for (const auto& arg : args) {
        if (arg.find(headInfo) == std::string::npos) {
            if (strncat_s(msg.update, sizeof(msg.update), arg.c_str(), strlen(arg.c_str()) + 1) != EOK) {
                LOG(ERROR) << "SetMessageToMisc strncat_s failed";
                return;
            }
            if (strncat_s(msg.update, sizeof(msg.update), "\n", strlen("\n") + 1) != EOK) {
                LOG(ERROR) << "SetMessageToMisc strncat_s failed";
                return;
            }
        }
    }
    char buffer[128] {}; // 128 : set headInfo size
    if (isRemove) {
        LOG(INFO) << "remove --" << headInfo << " from misc";
    } else if (!message.has_value()) {
        if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s", headInfo.c_str()) == -1) {
            LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
            return;
        }
    } else {
        if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s=%d",
            headInfo.c_str(), message.value()) == -1) {
            LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
            return;
        }
    }
    if (strncat_s(msg.update, sizeof(msg.update), buffer, strlen(buffer) + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncat_s failed";
        return;
    }
    if (WriteUpdaterMiscMsg(msg) != true) {
        LOG(ERROR) << "Write command to misc failed.";
    }
}
} // namespace

void SaveLogs()
{
    std::string updaterLogPath = std::string(UPDATER_LOG);
    std::string stageLogPath = std::string(UPDATER_STAGE_LOG);

    // save logs
    bool ret = CopyUpdaterLogs(TMP_LOG, updaterLogPath);
    if (!ret) {
        LOG(ERROR) << "Copy updater log failed!";
    }

    mode_t mode = 0660;
#ifndef __WIN32
    SetFileAttributes(updaterLogPath, USER_UPDATE_AUTHORITY, GROUP_UPDATE_AUTHORITY, mode);
#endif

    STAGE(UPDATE_STAGE_SUCCESS) << "PostUpdater";
    ret = CopyUpdaterLogs(TMP_STAGE_LOG, stageLogPath);
    chmod(stageLogPath.c_str(), mode);
    if (!ret) {
        LOG(ERROR) << "Copy stage log failed!";
    }
}

int32_t DeleteFile(const std::string& filename)
{
    if (filename.empty()) {
        LOG(ERROR) << "Invalid filename";
        return -1;
    }
    if (unlink(filename.c_str()) == -1 && errno != ENOENT) {
        LOG(ERROR) << "unlink " << filename << " failed";
        return -1;
    }
    return 0;
}

int MkdirRecursive(const std::string &pathName, mode_t mode)
{
    size_t slashPos = 0;
    struct stat info {};
    while (true) {
        slashPos = pathName.find_first_of("/", slashPos);
        if (slashPos == std::string::npos) {
            break;
        }
        if (slashPos == 0) {
            slashPos++;
            continue;
        }
        if (slashPos > PATH_MAX) {
            LOG(ERROR) << "path too long for mkdir";
            return -1;
        }
        auto subDir = pathName.substr(0, slashPos);
        LOG(INFO) << "subDir : " << subDir;
        if (stat(subDir.c_str(), &info) != 0) {
            int ret = mkdir(subDir.c_str(), mode);
            if (ret && errno != EEXIST) {
                return ret;
            }
        }
        slashPos++;
    }
    int ret = mkdir(pathName.c_str(), mode);
    if (ret && errno != EEXIST) {
        return ret;
    }
    return 0;
}

int64_t GetFilesFromDirectory(const std::string &path, std::vector<std::string> &files,
    bool isRecursive)
{
    struct stat sb {};
    if (stat(path.c_str(), &sb) == -1) {
        LOG(ERROR) << "Failed to stat";
        return -1;
    }
    DIR *dirp = opendir(path.c_str());
    struct dirent *dp;
    int64_t totalSize = 0;
    while ((dp = readdir(dirp)) != nullptr) {
        std::string fileName = path + "/" + dp->d_name;
        struct stat st {};
        if (stat(fileName.c_str(), &st) == 0) {
            std::string tmpName = dp->d_name;
            if (tmpName == "." || tmpName == "..") {
                continue;
            }
            if (isRecursive && S_ISDIR(st.st_mode)) {
                totalSize += GetFilesFromDirectory(fileName, files, isRecursive);
            }
            files.push_back(fileName);
            totalSize += st.st_size;
        }
    }
    closedir(dirp);
    return totalSize;
}

std::vector<std::string> SplitString(const std::string &str, const std::string del)
{
    std::vector<std::string> result;
    size_t found = std::string::npos;
    size_t start = 0;
    while (true) {
        found = str.find_first_of(del, start);
        result.push_back(str.substr(start, found - start));
        if (found == std::string::npos) {
            break;
        }
        start = found + 1;
    }
    return result;
}

std::string Trim(const std::string &str)
{
    if (str.empty()) {
        LOG(ERROR) << "str is empty";
        return str;
    }
    size_t start = 0;
    size_t end = str.size() - 1;
    while (start < str.size()) {
        if (!isspace(str[start])) {
            break;
        }
        start++;
    }
    while (start < end) {
        if (!isspace(str[end])) {
            break;
        }
        end--;
    }
    if (end < start) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

std::string ConvertSha256Hex(const uint8_t* shaDigest, size_t length)
{
    const std::string hexChars = "0123456789abcdef";
    std::string haxSha256 = "";
    unsigned int c;
    for (size_t i = 0; i < length; ++i) {
        auto d = shaDigest[i];
        c = (d >> SHIFT_RIGHT_FOUR_BITS) & 0xf;     // last 4 bits
        haxSha256.push_back(hexChars[c]);
        haxSha256.push_back(hexChars[d & 0xf]);
    }
    return haxSha256;
}

bool SetRebootMisc(const std::string& rebootTarget, const std::string &extData, struct UpdateMessage &msg)
{
    static const int32_t maxCommandSize = 16;
    int result = 0;
    if (rebootTarget == "updater" && strcmp(msg.command, "boot_updater") != 0) {
        result = strcpy_s(msg.command, maxCommandSize, "boot_updater");
    } else if (rebootTarget == "flashd" && strcmp(msg.command, "flashd") != 0) {
        result = strcpy_s(msg.command, maxCommandSize, "boot_flash");
    } else if (rebootTarget == "bootloader" && strcmp(msg.command, "boot_loader") != 0) {
        result = strcpy_s(msg.command, maxCommandSize, "boot_loader");
    }
    if (result != EOK) {
        LOG(ERROR) << "reboot set misc strcpy failed";
        return false;
    }
    msg.command[maxCommandSize] = 0;
    if (extData.empty()) {
        (void)memset_s(msg.update, sizeof(msg.update), 0, sizeof(msg.update));
        return true;
    }
    if (strcpy_s(msg.update, sizeof(msg.update) - 1, extData.c_str()) != EOK) {
        LOG(ERROR) << "failed to copy update";
        return false;
    }
    msg.update[sizeof(msg.update) - 1] = 0;
    return true;
}

void UpdaterDoReboot(const std::string& rebootTarget, const std::string &extData)
{
    LOG(INFO) << ", rebootTarget: " << rebootTarget;
    LoadFstab();
    struct UpdateMessage msg = {};
    if (rebootTarget.empty()) {
        if (WriteUpdaterMiscMsg(msg) != true) {
            LOG(INFO) << "UpdaterDoReboot: WriteUpdaterMessage empty error";
            return;
        }
    } else {
        if (!ReadUpdaterMiscMsg(msg)) {
            LOG(ERROR) << "UpdaterDoReboot read misc failed";
            return;
        }
        if (!SetRebootMisc(rebootTarget, extData, msg)) {
            LOG(ERROR) << "UpdaterDoReboot set misc failed";
            return;
        }
        if (!WriteUpdaterMiscMsg(msg)) {
            LOG(INFO) << "UpdaterDoReboot: WriteUpdaterMiscMsg error";
            return;
        }
    }
    sync();
#ifndef UPDATER_UT
    DoReboot(rebootTarget.c_str());
    while (true) {
        pause();
    }
#else
    return;
#endif
}

void DoShutdown()
{
    UpdateMessage msg = {};
    if (!WriteUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "DoShutdown: WriteUpdaterMessage empty error";
        return;
    }
    sync();
    DoReboot("shutdown");
}

std::string GetCertName()
{
#ifndef UPDATER_UT
    static std::string signingCertName = "/etc/certificate/signing_cert.crt";
#else
    static std::string signingCertName = "/data/updater/src/signing_cert.crt";
#endif
    return signingCertName;
}

bool WriteFully(int fd, const uint8_t *data, size_t size)
{
    ssize_t written = 0;
    size_t rest = size;

    while (rest > 0) {
        do {
            written = write(fd, data, rest);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return false;
        }
        data += written;
        rest -= static_cast<size_t>(written);
    }
    return true;
}

bool ReadFully(int fd, void *data, size_t size)
{
    auto p = reinterpret_cast<uint8_t *>(data);
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t sread = read(fd, p, remaining);
        if (sread <= 0) {
            LOG(ERROR) << "Utils::ReadFully run error";
            return false;
        }
        p += sread;
        remaining -= static_cast<size_t>(sread);
    }
    return true;
}

bool ReadFileToString(int fd, std::string &content)
{
    struct stat sb {};
    if (fstat(fd, &sb) != -1 && sb.st_size > 0) {
        content.resize(static_cast<size_t>(sb.st_size));
    }
    ssize_t n;
    auto remaining = static_cast<size_t>(sb.st_size);
    auto p = reinterpret_cast<char *>(content.data());
    while (remaining > 0) {
        n = read(fd, p, remaining);
        if (n <= 0) {
            return false;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

bool WriteStringToFile(int fd, const std::string& content)
{
    const char *p = content.data();
    size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n == -1) {
            return false;
        }
        p += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

void SyncFile(const std::string &dst)
{
    int fd = open(dst.c_str(), O_RDWR);
    if (fd < 0) {
        LOG(ERROR) << "open " << dst << " failed! err " << strerror(errno);
        return;
    }
    fsync(fd);
    close(fd);
}

bool CopyFile(const std::string &src, const std::string &dest, bool isAppend)
{
    char realPath[PATH_MAX + 1] = {0};
    if (realpath(src.c_str(), realPath) == nullptr) {
        LOG(ERROR) << src << " get realpath fail";
        return false;
    }

    std::ios_base::openmode mode = isAppend ? std::ios::app | std::ios::out : std::ios_base::out;
    std::ifstream fin(realPath);
    std::ofstream fout(dest, mode);
    if (!fin.is_open() || !fout.is_open()) {
        return false;
    }

    fout << fin.rdbuf();
    if (fout.fail()) {
        fout.clear();
        return false;
    }
    fout.flush();
    fout.close();
    SyncFile(dest); // no way to get fd from ofstream, so reopen to sync this file
    return true;
}

std::string GetLocalBoardId()
{
    return "HI3516";
}

void CompressLogs(const std::string &logName)
{
    PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return;
    }
    std::vector<std::pair<std::string, ZipFileInfo>> files;
    // Build the zip file to be packaged
    std::vector<std::string> testFileNames;
    std::string realName = logName.substr(logName.find_last_of("/") + 1);
    std::string logPath = logName.substr(0, logName.find_last_of("/"));
    testFileNames.push_back(realName);
    for (auto name : testFileNames) {
        ZipFileInfo file;
        file.fileInfo.identity = name;
        file.fileInfo.packMethod = PKG_COMPRESS_METHOD_ZIP;
        file.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
        std::string fileName = logName;
        files.push_back(std::pair<std::string, ZipFileInfo>(fileName, file));
    }

    PkgInfo pkgInfo;
    pkgInfo.signMethod = PKG_SIGN_METHOD_NONE;
    pkgInfo.digestMethod = PKG_SIGN_METHOD_NONE;
    pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;

    char realTime[MAX_TIME_SIZE] = {0};
    auto sysTime = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(sysTime);
    struct tm *localTime = std::localtime(&currentTime);
    if (localTime != nullptr) {
        std::strftime(realTime, sizeof(realTime), "%Y%m%d%H%M%S", localTime);
    }
    char pkgName[MAX_LOG_NAME_SIZE];
    if (snprintf_s(pkgName, MAX_LOG_NAME_SIZE, MAX_LOG_NAME_SIZE - 1,
        "%s/%s_%s.zip", logPath.c_str(), realName.c_str(), realTime) == -1) {
        PkgManager::ReleasePackageInstance(pkgManager);
        return;
    }
    int32_t ret = pkgManager->CreatePackage(pkgName, GetCertName(), &pkgInfo, files);
    if (ret != 0) {
        LOG(WARNING) << "CompressLogs failed";
        PkgManager::ReleasePackageInstance(pkgManager);
        return;
    }
    mode_t mode = 0640;
#ifndef __WIN32
    SetFileAttributes(pkgName, USER_UPDATE_AUTHORITY, GROUP_UPDATE_AUTHORITY, mode);
#endif
    (void)DeleteFile(logName);
    PkgManager::ReleasePackageInstance(pkgManager);
}

int GetFileSize(const std::string &dLog)
{
    int ret = 0;
    std::ifstream ifs(dLog, std::ios::binary | std::ios::in);
    if (ifs.is_open()) {
        ifs.seekg(0, std::ios::end);
        ret = ifs.tellg();
    }
    return ret;
}

bool RestoreconPath(const std::string &path)
{
    if (MountForPath(path) != 0) {
        LOG(ERROR) << "MountForPath " << path << " failed!";
        return false;
    }
#ifdef WITH_SELINUX
    if (RestoreconRecurse(path.c_str()) == -1) {
        LOG(WARNING) << "restore " << path << " failed";
    }
#endif // WITH_SELINUX
    if (UmountForPath(path) != 0) {
        LOG(WARNING) << "UmountForPath " << path << " failed!";
    }
    return true;
}

bool CopyUpdaterLogs(const std::string &sLog, const std::string &dLog)
{
    std::size_t found = dLog.find_last_of("/");
    if (found == std::string::npos) {
        LOG(ERROR) << "Dest filePath error";
        return false;
    }
    std::string destPath = dLog.substr(0, found);
    if (MountForPath(destPath) != 0) {
        LOG(WARNING) << "MountForPath /data/log failed!";
        return false;
    }

    if (access(destPath.c_str(), 0) != 0) {
        if (MkdirRecursive(destPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            LOG(ERROR) << "MkdirRecursive error!";
            return false;
        }
        #ifdef WITH_SELINUX
            RestoreconRecurse(UPDATER_PATH);
        #endif // WITH_SELINUX
    }

    if (Utils::GetFileSize(sLog) > MAX_LOG_SIZE) {
        LOG(ERROR) << "Size bigger for" << sLog;
        STAGE(UPDATE_STAGE_FAIL) << "Log file error, unable to copy";
        return false;
    }

    while (Utils::GetFileSize(sLog) + GetDirSizeForFile(dLog) > MAX_LOG_DIR_SIZE) {
        if (DeleteOldFile(destPath) != true) {
            break;
        }
    }

    if (!CopyFile(sLog, dLog, true)) {
        LOG(ERROR) << "copy log file failed.";
        return false;
    }
    if (GetFileSize(dLog) >= MAX_LOG_SIZE) {
        LOG(INFO) << "log size greater than 5M!";
        CompressLogs(dLog);
    }
    sync();
    return true;
}

bool CheckResultFail()
{
    std::ifstream ifs;
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    ifs.open(resultPath, std::ios::in);
    std::string buff;
    while (ifs.is_open() && getline(ifs, buff)) {
        if (buff.find("fail|") != std::string::npos) {
            ifs.close();
            return true;
        }
    }
    LOG(ERROR) << "open result file failed";
    return false;
}

void WriteDumpResult(const std::string &result)
{
    if (access(UPDATER_PATH, 0) != 0) {
        if (MkdirRecursive(UPDATER_PATH, 0755) != 0) { // 0755: -rwxr-xr-x
            LOG(ERROR) << "MkdirRecursive error!";
            return;
        }
    }
    LOG(INFO) << "WriteDumpResult: " << result;
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    FILE *fp = fopen(resultPath.c_str(), "w+");
    if (fp == nullptr) {
        LOG(ERROR) << "open result file failed";
        return;
    }
    char buf[MAX_RESULT_BUFF_SIZE] = "Pass\n";
    if (sprintf_s(buf, MAX_RESULT_BUFF_SIZE - 1, "%s\n", result.c_str()) < 0) {
        LOG(WARNING) << "sprintf status fialed";
    }
    if (fwrite(buf, 1, strlen(buf) + 1, fp) <= 0) {
        LOG(WARNING) << "write result file failed, err:" << errno;
    }
    if (fclose(fp) != 0) {
        LOG(WARNING) << "close result file failed";
    }

    (void)chown(resultPath.c_str(), USER_ROOT_AUTHORITY, GROUP_UPDATE_AUTHORITY);
    (void)chmod(resultPath.c_str(), 0660); // 0660: -rw-rw----
}

void UsSleep(int usec)
{
    auto seconds = usec / USECONDS_PER_SECONDS;
    long nanoSeconds = static_cast<long>(usec) % USECONDS_PER_SECONDS * NANOSECS_PER_USECONDS;
    struct timespec ts = { static_cast<time_t>(seconds), nanoSeconds };
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
    }
}

bool PathToRealPath(const std::string &path, std::string &realPath)
{
    if (path.empty()) {
        LOG(ERROR) << "path is empty!";
        return false;
    }

    if ((path.length() >= PATH_MAX)) {
        LOG(ERROR) << "path len is error, the len is: " << path.length();
        return false;
    }

    char tmpPath[PATH_MAX] = {0};
    if (realpath(path.c_str(), tmpPath) == nullptr) {
        LOG(ERROR) << "path to realpath error " << path;
        return false;
    }

    realPath = tmpPath;
    return true;
}

bool IsUpdaterMode()
{
    struct stat st {};
    if (stat("/bin/updater", &st) == 0 && S_ISREG(st.st_mode)) {
        LOG(INFO) << "updater mode";
        return true;
    }
    LOG(INFO) << "normal mode";
    return false;
}

bool RemoveDir(const std::string &path)
{
    if (path.empty()) {
        LOG(ERROR) << "input path is empty.";
        return false;
    }
    std::string strPath = path;
    if (strPath.at(strPath.length() - 1) != '/') {
        strPath.append("/");
    }
    DIR *d = opendir(strPath.c_str());
    if (d != nullptr) {
        struct dirent *dt = nullptr;
        dt = readdir(d);
        while (dt != nullptr) {
            if (strcmp(dt->d_name, "..") == 0 || strcmp(dt->d_name, ".") == 0) {
                dt = readdir(d);
                continue;
            }
            struct stat st {};
            auto file_name = strPath + std::string(dt->d_name);
            stat(file_name.c_str(), &st);
            if (S_ISDIR(st.st_mode)) {
                RemoveDir(file_name);
            } else {
                remove(file_name.c_str());
            }
            dt = readdir(d);
        }
        closedir(d);
    }
    return rmdir(strPath.c_str()) == 0 ? true : false;
}

bool IsFileExist(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return true;
    }
    return false;
}

bool IsDirExist(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

long long int GetDirSize(const std::string &folderPath)
{
    DIR* dir = opendir(folderPath.c_str());
    if (dir == nullptr) {
        LOG(ERROR) << "Failed to open folder: " << folderPath << std::endl;
        return 0;
    }

    struct dirent* entry;
    long long int totalSize = 0;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fileName = entry->d_name;
        std::string filePath = folderPath + "/" + fileName;
        struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) != 0) {
            LOG(ERROR) << "Failed to get file status: " << filePath << std::endl;
            continue;
        }
        if (S_ISDIR(fileStat.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            std::string subFolderPath = filePath;
            totalSize += GetDirSize(subFolderPath);
        } else {
            totalSize += fileStat.st_size;
        }
    }
    closedir(dir);
    return totalSize;
}

long long int GetDirSizeForFile(const std::string &filePath)
{
    std::size_t found = filePath.find_last_of("/");
    if (found == std::string::npos) {
        LOG(ERROR) << "filePath error";
        return -1;
    }
    return GetDirSize(filePath.substr(0, found));
}

bool DeleteOldFile(const std::string folderPath)
{
    DIR* dir = opendir(folderPath.c_str());
    if (dir == nullptr) {
        LOG(ERROR) << "Failed to open folder: " << folderPath << std::endl;
        return false;
    }

    struct dirent* entry;
    std::string oldestFilePath = "";
    time_t oldestFileTime = std::numeric_limits<time_t>::max();
    while ((entry = readdir(dir)) != nullptr) {
        std::string fileName = entry->d_name;
        std::string filePath = folderPath + "/" + fileName;
        struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) != 0) {
            LOG(ERROR) << "Failed to get file status: " << filePath;
            continue;
        }
        if (fileName == "." || fileName == "..") {
            continue;
        }
        if (fileStat.st_mtime < oldestFileTime) {
            oldestFileTime = fileStat.st_mtime;
            oldestFilePath = filePath;
        }
    }
    closedir(dir);
    if (oldestFilePath.empty()) {
        LOG(ERROR) << "Unable to delete file";
        return false;
    }
    if (remove(oldestFilePath.c_str()) != 0) {
        LOG(ERROR) << "Failed to delete file: " << oldestFilePath;
        return false;
    }
    return true;
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

bool CheckUpdateMode(const std::string &mode)
{
    std::vector<std::string> args = ParseParams(0, nullptr);
    for(const auto &arg : args) {
        if (arg.find(mode) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string DurationToString(std::vector<std::chrono::duration<double>> &durations, std::size_t pkgPosition,
    int precision)
{
    if (pkgPosition >= durations.size()) {
        LOG(ERROR) << "pkg position is " << pkgPosition << ", duration's size is " << durations.size();
        return "0";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << durations[pkgPosition].count();
    return oss.str();
}

std::string GetRealPath(const std::string &path)
{
    char realPath[PATH_MAX + 1] = {0};
    auto ret = realpath(path.c_str(), realPath);
    return (ret == nullptr) ? "" : ret;
}
 
std::string GetPartitionRealPath(const std::string &name)
{
    return GetRealPath(PREFIX_PARTITION_NODE + name);
}

void SetMessageToMisc(const std::string &miscCmd, const int message, const std::string headInfo)
{
    if (headInfo.empty()) {
        return;
    }
    std::vector<std::string> args = ParseParams(0, nullptr);
    struct UpdateMessage msg {};
    if (!ReadUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "SetMessageToMisc read misc failed";
        return;
    }
    (void)memset_s(msg.command, sizeof(msg.command), 0, sizeof(msg.command));
    if (strncpy_s(msg.command, sizeof(msg.command), miscCmd.c_str(), miscCmd.size() + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncpy_s failed";
        return;
    }
    (void)memset_s(msg.update, sizeof(msg.update), 0, sizeof(msg.update));
    for (const auto& arg : args) {
        if (arg.find(headInfo) == std::string::npos) {
            if (strncat_s(msg.update, sizeof(msg.update), arg.c_str(), strlen(arg.c_str()) + 1) != EOK) {
                LOG(ERROR) << "SetMessageToMisc strncat_s failed";
                return;
            }
            if (strncat_s(msg.update, sizeof(msg.update), "\n", strlen("\n") + 1) != EOK) {
                LOG(ERROR) << "SetMessageToMisc strncat_s failed";
                return;
            }
        }
    }
    char buffer[128] {}; // 128 : set headInfo size
    if (headInfo == "sdcard_update") {
        if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s", headInfo.c_str()) == -1) {
            LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
            return;
        }
    } else {
        if (snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "--%s=%d", headInfo.c_str(), message) == -1) {
            LOG(ERROR) << "SetMessageToMisc snprintf_s failed";
            return;
        }
    }
    if (strncat_s(msg.update, sizeof(msg.update), buffer, strlen(buffer) + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncat_s failed";
        return;
    }
    if (WriteUpdaterMiscMsg(msg) != true) {
        LOG(ERROR) << "Write command to misc failed.";
    }
}

void SetCmdToMisc(const std::string &miscCmd)
{
    struct UpdateMessage msg {};
    if (!ReadUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "SetMessageToMisc read misc failed";
        return;
    }

    (void)memset_s(msg.command, sizeof(msg.command), 0, sizeof(msg.command));
    if (strncpy_s(msg.command, sizeof(msg.command), miscCmd.c_str(), miscCmd.size() + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncpy_s failed";
        return;
    }

    if (WriteUpdaterMiscMsg(msg) != true) {
        LOG(ERROR) << "Write command to misc failed.";
    }
}

void AddUpdateInfoToMisc(const std::string headInfo, const std::optional<int> message)
{
    UpdateInfoInMisc(headInfo, message, false);
}

void RemoveUpdateInfoFromMisc(const std::string &headInfo)
{
    UpdateInfoInMisc(headInfo, std::nullopt, true);
}

void SetFaultInfoToMisc(const std::string &faultInfo)
{
    struct UpdateMessage msg {};
    if (!ReadUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "SetMessageToMisc read misc failed";
        return;
    }

    (void)memset_s(msg.faultinfo, sizeof(msg.faultinfo), 0, sizeof(msg.faultinfo));
    if (strncpy_s(msg.faultinfo, sizeof(msg.faultinfo), faultInfo.c_str(), faultInfo.size() + 1) != EOK) {
        LOG(ERROR) << "SetMessageToMisc strncpy_s failed";
        return;
    }

    if (WriteUpdaterMiscMsg(msg) != true) {
        LOG(ERROR) << "Write fault info to misc failed.";
    }
}

bool CheckFaultInfo(const std::string &faultInfo)
{
    struct UpdateMessage msg = {};
    if (!ReadUpdaterMiscMsg(msg)) {
        LOG(ERROR) << "read misc data failed";
        return false;
    }

    if (strcmp(msg.faultinfo, faultInfo.c_str()) == 0) {
        return true;
    }
    return false;
}

void GetTagValInStr(const std::string &str, const std::string &tag, std::string &val)
{
    if (str.find(tag + "=") != std::string::npos) {
        val = str.substr(str.find("=") + 1, str.size() - str.find("="));
    }
}

bool IsValidHexStr(const std::string &str)
{
    for (const auto &ch : str) {
        if (isxdigit(ch) == 0) {
            return false;
        }
    }
    return true;
}

void TrimString(std::string &str)
{
    auto pos = str.find_last_not_of("\r\n");
    if (pos != std::string::npos) {
        str.erase(pos + 1, str.size() - pos);
    }
}

#ifndef __WIN32
void SetFileAttributes(const std::string& file, uid_t owner, gid_t group, mode_t mode)
{
#ifdef WITH_SELINUX
    RestoreconRecurse(file.c_str());
#endif // WITH_SELINUX
    if (chown(file.c_str(), owner, group) != 0 && chmod(file.c_str(), mode) != EOK) {
        LOG(ERROR) << "Chmod failed!";
    }
}
#endif
} // Utils
} // namespace Updater
