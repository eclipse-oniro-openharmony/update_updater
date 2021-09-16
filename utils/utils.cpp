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

namespace updater {
using namespace hpackage;

namespace utils {
constexpr uint32_t MAX_PATH_LEN = 256;
constexpr uint8_t SHIFT_RIGHT_FOUR_BITS = 4;
constexpr int USER_ROOT_AUTHORITY = 0;
constexpr int GROUP_SYS_AUTHORITY = 1000;
int32_t DeleteFile(const std::string& filename)
{
    UPDATER_ERROR_CHECK (!filename.empty(), "Invalid filename", return -1);
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
        UPDATER_CHECK_ONLY_RETURN(slashPos != std::string::npos, break);
        if (slashPos == 0) {
            slashPos++;
            continue;
        }
        UPDATER_ERROR_CHECK(slashPos <= MAX_PATH_LEN, "path too long for mkdir", return -1);
        auto subDir = pathName.substr(0, slashPos);
        std::cout << "subDir : " << subDir << std::endl;
        if (stat(subDir.c_str(), &info) != 0) {
            int ret = mkdir(subDir.c_str(), mode);
            UPDATER_CHECK_ONLY_RETURN(!(ret && errno != EEXIST), return ret);
        }
        slashPos++;
    }
    int ret = mkdir(pathName.c_str(), mode);
    UPDATER_CHECK_ONLY_RETURN(!(ret && errno != EEXIST), return ret);
    return 0;
}

int64_t GetFilesFromDirectory(const std::string &path, std::vector<std::string> &files,
    bool isRecursive)
{
    struct stat sb {};
    UPDATER_ERROR_CHECK (stat(path.c_str(), &sb) != -1, "Failed to stat", return -1);
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

void DoReboot(const std::string& rebootTarget)
{
    LOG(INFO) << ", rebootTarget: " << rebootTarget;
    LoadFstab();
    auto miscBlockDevice = GetBlockDeviceByMountPoint("/misc");
    struct UpdateMessage msg;
    if (rebootTarget == "updater") {
	std::string command = "boot_updater";
        bool ret = ReadUpdaterMessage(miscBlockDevice, msg);
        UPDATER_ERROR_CHECK(ret == true, "DoReboot read misc failed", return);
        if (strcmp(msg.command, command.c_str()) != 0) {
            UPDATE_ERROR_CHECK(memset_s(msg.command, MAX_COMMAND_SIZE, 0, MAX_COMMAND_SIZE) == 0,
                "Failed to clear update message", return);
            UPDATER_ERROR_CHECK(!memcpy_s(msg.command, MAX_COMMAND_SIZE - 1, command.c_str(), command.size()),
                "Memcpy failed", return);
        }
        ret = WriteUpdaterMessage(miscBlockDevice, msg);
        if (ret != true) {
            LOG(INFO) << "DoReboot: WriteUpdaterMessage boot_updater error";
            return;
        }
        sync();
    } else {
        UPDATER_ERROR_CHECK(!memset_s(msg.command, MAX_COMMAND_SIZE, 0, MAX_COMMAND_SIZE), "Memset_s failed", return);
        bool ret = WriteUpdaterMessage(miscBlockDevice, msg);
        if (ret != true) {
            LOG(INFO) << "DoReboot: WriteUpdaterMessage empty error";
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

std::string GetCertName()
{
#ifndef UPDATER_UT
    static std::string signingCertName = "/certificate/signing_cert.crt";
#else
    static std::string signingCertName = "/data/updater/src/signing_cert.crt";
#endif
    return signingCertName;
}

bool WriteFully(int fd, const void *data, size_t size)
{
    ssize_t written = 0;
    size_t rest = size;

    auto p = reinterpret_cast<const uint8_t*>(data);
    while (rest > 0) {
        do {
            written = write(fd, p, rest);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return false;
        }
        p += written;
        rest -= written;
    }
    return true;
}

bool ReadFully(int fd, void *data, size_t size)
{
    auto p = reinterpret_cast<uint8_t *>(data);
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t sread = read(fd, p, remaining);
        UPDATER_ERROR_CHECK (sread > 0, "Utils::ReadFully run error", return false);
        p += sread;
        remaining -= sread;
    }
    return true;
}

bool ReadFileToString(int fd, std::string &content)
{
    struct stat sb {};
    if (fstat(fd, &sb) != -1 && sb.st_size > 0) {
        content.resize(sb.st_size);
    }
    ssize_t n;
    size_t remaining = sb.st_size;
    auto p = reinterpret_cast<char *>(content.data());
    while (remaining > 0) {
        n = read(fd, p, remaining);
        UPDATER_CHECK_ONLY_RETURN (n > 0, return false);
        p += n;
        remaining -= n;
    }
    return true;
}

bool WriteStringToFile(int fd, const std::string& content)
{
    const char *p = content.data();
    size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        UPDATER_CHECK_ONLY_RETURN (n != -1, return false);
        p += n;
        remaining -= n;
    }
    return true;
}

std::string GetLocalBoardId()
{
    return "HI3516";
}

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

    char realTime[MAX_TIME_SIZE] = {0};
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

    FILE* dFp = fopen(dLog.c_str(), "ab+");
    UPDATER_ERROR_CHECK(dFp != nullptr, "open log failed", return false);

    FILE* sFp = fopen(sLog.c_str(), "r");
    UPDATER_ERROR_CHECK(sFp != nullptr, "open log failed", fclose(dFp); return false);

    char buf[MAX_LOG_BUF_SIZE];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), sFp)) != 0) {
        fwrite(buf, 1, bytes, dFp);
    }
    fseek(dFp, 0, SEEK_END);
    UPDATER_INFO_CHECK(ftell(dFp) < MAX_LOG_SIZE, "log size greater than 5M!", CompressLogs(dLog));
    sync();
    fclose(sFp);
    fclose(dFp);
    return true;
}
} // utils
} // namespace updater
