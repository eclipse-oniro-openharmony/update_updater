/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "utils_fs.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits>
#include <linux/reboot.h>
#include <string>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "log/log.h"

namespace Updater {
namespace Utils {

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

void* LoadLibrary(const std::string &libName, const std::string &libPath)
{
    if (libName.empty()) {
        LOG(ERROR) << "lib name is empty";
        return nullptr;
    }
    if (libPath != "/system/lib64/" && libPath != "/vendor/lib64/") {
        LOG(ERROR) << "lib path invalid";
        return nullptr;
    }
    std::string libAbsPath = libPath + libName;
    char realPath[PATH_MAX + 1] = {0};
    if (realpath(libAbsPath.c_str(), realPath) == nullptr) {
        LOG(ERROR) << "realpath error";
        UPDATER_LAST_WORD(PKG_INVALID_FILE, "realpath error");
        return PKG_INVALID_FILE;
    }
    void* handle = dlopen(libAbsPath.c_str(), RTLD_LAZY);
    if (handle == nullptr) {
        LOG(ERROR) << "dlopen fail, lib name = " << libName << "; dlerror = " << dlerror();
        return nullptr;
    }
    return handle;
}

void CloseLibrary(void* handle)
{
    if (handle == nullptr) {
        LOG(ERROR) << "handle is nulptr";
        return;
    }
    dlclose(handle);
    handle = nullptr;
}

void* GetFunction(void* handle, const std::string &funcName)
{
    if (handle == nullptr) {
        LOG(ERROR) << "handle is nullptr";
        return nullptr;
    }
    if (funcName.empty()) {
        LOG(ERROR) << "func name is empty";
        return nullptr;
    }
    return dlsym(handle, funcName.c_str());
}
} // Utils
} // updater
