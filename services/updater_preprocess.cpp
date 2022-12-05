/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "updater/updater_preprocess.h"
#include "log.h"
#include "parameter.h"
#include "utils.h"

using namespace std;
using namespace Hpackage;
namespace Updater {
extern "C" __attribute__((constructor)) void RegisterHelper(void)
{
    PreProcess::GetInstance().RegisterHelper(UpdatePreProcess);
}

void PreProcess::RegisterHelper(PreProcessFunc ptr)
{
    helper_ = std::move(ptr);
}

PreProcess &PreProcess::GetInstance()
{
    static PreProcess checkPackagesInfo;
    return checkPackagesInfo;
}

int32_t PreProcess::DoUpdatePreProcess(PkgManager::PkgManagerPtr pkgManager)
{
    if (helper_ == nullptr) {
        LOG(INFO) << "PreProcess helper_ is null";
        return -1;
    }
    return helper_(pkgManager);
}

int CheckVersion(PkgManager::PkgManagerPtr pkgManager, PackagesInfoPtr pkginfomanager)
{
    int ret = -1;
    if (pkgManager == nullptr || pkginfomanager == nullptr) {
        return PKG_INVALID_VERSION;
    }
    const char *verPtr = GetDisplayVersion();
    if (verPtr == nullptr) {
        LOG(ERROR) << "Fail to GetDisplayVersion";
        return PKG_INVALID_VERSION;
    }
    std::string verStr(verPtr);
    LOG(INFO) << "current version:" << verStr;
    std::vector<std::string> targetVersions = pkginfomanager->GetOTAVersion(pkgManager, "/version_list", "");
    for (size_t i = 0; i < targetVersions.size(); i++) {
        ret = verStr.compare(targetVersions[i]);
        if (ret == 0) {
            LOG(INFO) << "Check version success ";
            break;
        }
    }
    return ret;
}

int CheckBoardId(PkgManager::PkgManagerPtr pkgManager, PackagesInfoPtr pkginfomanager)
{
    int ret = -1;
    if (pkgManager == nullptr || pkginfomanager == nullptr) {
        return PKG_INVALID_VERSION;
    }
    std::string localBoardId = Utils::GetLocalBoardId();
    if (localBoardId.empty()) {
        return 0;
    }
    std::vector<std::string> boardIdList = pkginfomanager->GetBoardID(pkgManager, "/board_list", "");
    for (size_t i = 0; i < boardIdList.size(); i++) {
        ret = localBoardId.compare(boardIdList[i]);
        if (ret == 0) {
            LOG(INFO) << "Check board list success ";
            break;
        }
    }
    return ret;
}

int32_t UpdatePreProcess(PkgManager::PkgManagerPtr pkgManager)
{
    int ret = -1;
    if (pkgManager == nullptr) {
        return PKG_INVALID_VERSION;
    }
    PackagesInfoPtr pkginfomanager = PackagesInfo::GetPackagesInfoInstance();
    if (pkginfomanager == nullptr) {
        return PKG_INVALID_VERSION;
    }
    ret = CheckBoardId(pkgManager, pkginfomanager);
    if (ret != 0) {
        PackagesInfo::ReleasePackagesInfoInstance(pkginfomanager);
        return ret;
    }
    ret = CheckVersion(pkgManager, pkginfomanager);
    PackagesInfo::ReleasePackagesInfoInstance(pkginfomanager);
    return ret;
}
} // namespace Updater
