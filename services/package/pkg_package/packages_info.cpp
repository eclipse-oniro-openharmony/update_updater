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
#include "packages_info.h"
#include <cstring>
#include <iostream>
#include "log.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_pkgfile.h"
#include "pkg_stream.h"
#include "pkg_upgradefile.h"
#include "pkg_utils.h"

using namespace std;
using namespace Hpackage;

namespace Updater {
vector<string> SplitString(const string &str, const string &pattern)
{
    vector<string> ret;
    if (pattern.empty()) {
        return ret;
    }
    size_t start = 0;
    size_t index = str.find_first_of(pattern, 0);
    while (index != str.npos) {
        if (start != index) {
            string tmpStr = str.substr(start, index - start);
            tmpStr.erase(0, tmpStr.find_first_not_of(" "));
            tmpStr.erase(tmpStr.find_last_not_of(" ") + 1);
            tmpStr.erase(tmpStr.find_last_not_of("\r") + 1);
            ret.push_back(tmpStr);
        }
        start = index + 1;
        index = str.find_first_of(pattern, start);
    }

    if (!str.substr(start).empty()) {
        string tmpStr = str.substr(start);
        tmpStr.erase(0, tmpStr.find_first_not_of(" "));
        tmpStr.erase(tmpStr.find_last_not_of(" ") + 1);
        tmpStr.erase(tmpStr.find_last_not_of("\r") + 1);
        ret.push_back(tmpStr);
    }
    return ret;
}

static PackagesInfoPtr g_packagesInfoInstance = nullptr;
PackagesInfoPtr PackagesInfo::GetPackagesInfoInstance()
{
    if (g_packagesInfoInstance == nullptr) {
        g_packagesInfoInstance = new PackagesInfo();
    }
    return g_packagesInfoInstance;
}

void PackagesInfo::ReleasePackagesInfoInstance(PackagesInfoPtr pkginfomanager)
{
    delete pkginfomanager;
    g_packagesInfoInstance = nullptr;
}

std::vector<std::string> PackagesInfo::GetOTAVersion(Hpackage::PkgManager::PkgManagerPtr manager,
    const std::string &versionList, const std::string &versionPath)
{
    vector<string> tmpVersionList;
    if (manager == nullptr) {
        PKG_LOGE("manager is null");
        return tmpVersionList;
    }

    PkgManager::StreamPtr outStream = nullptr;
    const string pattern = "\n";
    const FileInfo *info = manager->GetFileInfo(versionList);
    if (info == nullptr) {
        PKG_LOGE("Can not get file info %s", versionList.c_str());
        return tmpVersionList;
    }
    int32_t ret = manager->CreatePkgStream(outStream, versionList, info->unpackedSize,
        PkgStream::PkgStreamType_MemoryMap);
    if (outStream == nullptr || ret == -1) {
        PKG_LOGE("Create stream fail %s", versionList.c_str());
        return tmpVersionList;
    }
    ret = manager->ExtractFile(versionList, outStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ExtractFile versionList fail ");
        manager->ClosePkgStream(outStream);
        return tmpVersionList;
    }
    PkgBuffer data = {};
    ret = outStream->GetBuffer(data);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ExtractFile fail %s", versionList.c_str());
        manager->ClosePkgStream(outStream);
        return tmpVersionList;
    }
    std::string str(reinterpret_cast<char*>(data.buffer), data.length);
    tmpVersionList = SplitString(str, pattern);
    manager->ClosePkgStream(outStream);
    return tmpVersionList;
}

std::vector<std::string> PackagesInfo::GetBoardID(Hpackage::PkgManager::PkgManagerPtr manager,
    const std::string &boardIdName, const std::string &boardListPath)
{
    vector<string> boardlist;
    if (manager == nullptr) {
        PKG_LOGE("manager is null");
        return boardlist;
    }
    (void)boardListPath;
    PkgManager::StreamPtr outStream = nullptr;
    const FileInfo *info = manager->GetFileInfo(boardIdName);
    if (info == nullptr) {
        PKG_LOGE("Can not get file info %s", boardIdName.c_str());
        return boardlist;
    }
    int32_t ret = manager->CreatePkgStream(outStream, boardIdName, info->unpackedSize,
        PkgStream::PkgStreamType_MemoryMap);
    if (outStream == nullptr || ret != PKG_SUCCESS) {
        PKG_LOGE("Create stream fail %s", boardIdName.c_str());
        return boardlist;
    }
    ret = manager->ExtractFile(boardIdName, outStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ExtractFile error, ret =  %d", ret);
        return boardlist;
    }
    PkgBuffer data = {};
    ret = outStream->GetBuffer(data);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ExtractFile fail %s", boardIdName.c_str());
        manager->ClosePkgStream(outStream);
        return boardlist;
    }
    std::string str(reinterpret_cast<char*>(data.buffer), data.length);
    boardlist = SplitString(str, "\n");
    manager->ClosePkgStream(outStream);
    return boardlist;
}

bool PackagesInfo::IsAllowRollback()
{
    return false;
}
} // namespace Updater
