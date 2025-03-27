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

#ifndef UTILS_FS_H
#define UTILS_FS_H

#include <cerrno>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace Updater {
namespace Utils {
int MkdirRecursive(const std::string &pathName, mode_t mode);
int64_t GetFilesFromDirectory(const std::string &path, std::vector<std::string> &files, bool isRecursive = false);
bool RemoveDir(const std::string &path);
bool IsFileExist(const std::string &path);
bool IsDirExist(const std::string &path);
void* LoadLibrary(const std::string &libName, const std::string *libPath);
void CloseLibrary(void* handle);
void* GetFunction(void* handle, const std::string &funcName);
} // Utils
} // Updater
#endif // UTILS_FS_H