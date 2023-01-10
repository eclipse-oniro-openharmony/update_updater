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
#include <limits>
#include <linux/reboot.h>
#include <string>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "fs_manager/mount.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
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
        std::cout << "subDir : " << subDir << std::endl;
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

void DoReboot(const std::string& rebootTarget, const std::string &extData)
{
    LOG(INFO) << ", rebootTarget: " << rebootTarget;
    static const int32_t maxCommandSize = 16;
    LoadFstab();
    struct UpdateMessage msg;
    if (rebootTarget.empty()) {
        UPDATER_ERROR_CHECK(!memset_s(msg.command, MAX_COMMAND_SIZE, 0, MAX_COMMAND_SIZE), "Memset_s failed", return);
        if (WriteUpdaterMiscMsg(msg) != true) {
            LOG(INFO) << "DoReboot: WriteUpdaterMessage empty error";
            return;
        }
        sync();
    } else {
        int result = 0;
        bool ret = ReadUpdaterMiscMsg(msg);
        UPDATER_ERROR_CHECK(ret == true, "DoReboot read misc failed", return);
        if (rebootTarget == "updater" && strcmp(msg.command, "boot_updater") != 0) {
            result = strcpy_s(msg.command, maxCommandSize, "boot_updater");
            msg.command[maxCommandSize] = 0;
        } else if (rebootTarget == "flashd" && strcmp(msg.command, "flashd") != 0) {
            result = strcpy_s(msg.command, maxCommandSize, "boot_flash");
            msg.command[maxCommandSize] = 0;
        } else if (rebootTarget == "bootloader" && strcmp(msg.command, "boot_loader") != 0) {
            result = strcpy_s(msg.command, maxCommandSize, "boot_loader");
            msg.command[maxCommandSize] = 0;
        }
        UPDATER_ERROR_CHECK(result == 0, "strcpy failed", return);
        if (!extData.empty()) {
            result = strcpy_s(msg.update, MAX_UPDATE_SIZE - 1, extData.c_str());
            UPDATER_ERROR_CHECK(result == 0, "Failed to copy update", return);
            msg.update[MAX_UPDATE_SIZE - 1] = 0;
        } else {
            UPDATER_ERROR_CHECK(!memset_s(msg.update, MAX_UPDATE_SIZE, 0, MAX_UPDATE_SIZE), "Memset_s failed", return);
        }
        if (!WriteUpdaterMiscMsg(msg)) {
            LOG(INFO) << "DoReboot: WriteUpdaterMiscMsg empty error";
            return;
        }
        sync();
    }
#ifndef UPDATER_UT
    syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, rebootTarget.c_str());
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
    reboot(RB_POWER_OFF);
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

bool CopyFile(const std::string &src, const std::string &dest)
{
    char realPath[PATH_MAX + 1] = {0};
    if (realpath(src.c_str(), realPath) == nullptr) {
        LOG(ERROR) << src << " get realpath fail";
        return false;
    }

    std::ifstream fin(realPath);
    std::ofstream fout(dest);
    if (!fin.is_open() || !fout.is_open()) {
        return false;
    }

    fout << fin.rdbuf();
    if (fout.fail()) {
        fout.clear();
        return false;
    }
    fout.flush();
    return true;
}

std::string GetLocalBoardId()
{
    return "HI3516";
}

void CompressLogs(const std::string &name)
{
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return;
    }
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
    pkgInfo.signMethod = PKG_SIGN_METHOD_NONE;
    pkgInfo.digestMethod = PKG_SIGN_METHOD_NONE;
    pkgInfo.pkgType = PKG_PACK_TYPE_ZIP;

    char realTime[MAX_TIME_SIZE] = {0};
    auto sysTime = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(sysTime);
    struct tm *localTime = std::localtime(&currentTime);
    if (localTime != nullptr) {
        std::strftime(realTime, sizeof(realTime), "%H_%M_%S", localTime);
    }
    char pkgName[MAX_LOG_NAME_SIZE];
    if (snprintf_s(pkgName, MAX_LOG_NAME_SIZE, MAX_LOG_NAME_SIZE - 1,
        "/data/updater/log/%s_%s.zip", realName.c_str(), realTime) == -1) {
        return;
    }
    int32_t ret = pkgManager->CreatePackage(pkgName, GetCertName(), &pkgInfo, files);
    if (ret != 0) {
        LOG(WARNING) << "CompressLogs failed";
        return;
    }
    (void)DeleteFile(name);
}

bool CopyUpdaterLogs(const std::string &sLog, const std::string &dLog)
{
    UPDATER_WARING_CHECK(MountForPath(UPDATER_LOG_DIR) == 0, "MountForPath /data/log failed!", return false);
    if (access(UPDATER_LOG_DIR, 0) != 0) {
        if (MkdirRecursive(UPDATER_LOG_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            LOG(ERROR) << "MkdirRecursive error!";
            return false;
        }
        if (chown(UPDATER_PATH, USER_ROOT_AUTHORITY, GROUP_SYS_AUTHORITY) != EOK &&
            chmod(UPDATER_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != EOK) {
                LOG(ERROR) << "Chmod failed!";
                return false;
        }
    }

    FILE* dFp = fopen(dLog.c_str(), "ab+");
    if (dFp == nullptr) {
        LOG(ERROR) << "open log failed";
        return false;
    }
    FILE* sFp = fopen(sLog.c_str(), "r");
    if (sFp == nullptr) {
        LOG(ERROR) << "open log failed";
        fclose(dFp);
        return false;
    }

    char buf[MAX_LOG_BUF_SIZE];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), sFp)) != 0) {
        if (fwrite(buf, 1, bytes, dFp) <= 0) {
            LOG(ERROR) << "fwrite failed";
            fclose(sFp);
            fclose(dFp);
            return false;
        }
    }
    if (fseek(dFp, 0, SEEK_END) != 0) {
        LOG(ERROR) << "fseek failed";
        fclose(sFp);
        fclose(dFp);
        return false;
    }
    UPDATER_INFO_CHECK(ftell(dFp) < MAX_LOG_SIZE, "log size greater than 5M!", CompressLogs(dLog));
    sync();
    fclose(sFp);
    fclose(dFp);
    return true;
}

bool CheckDumpResult()
{
    std::ifstream ifs;
    const std::string resultPath = std::string(UPDATER_PATH) + "/" + std::string(UPDATER_RESULT_FILE);
    ifs.open(resultPath, std::ios::in);
    std::string buff;
    if (ifs.is_open() && getline(ifs, buff) && buff.find("fail:") != std::string::npos) {
        return true;
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
} // Utils
} // namespace Updater
