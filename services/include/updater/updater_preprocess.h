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
#ifndef UPDATER_PREPROCESS_H
#define UPDATER_PREPROCESS_H

#include "macros_updater.h"
#include "pkg_manager.h"
#include "package/packages_info.h"
#include "updater/updater.h"

namespace Updater {
typedef int32_t (*PreProcessFunc)(UpdaterParams &upParams, Hpackage::PkgManager::PkgManagerPtr pkgManager);
typedef int32_t (*AuthFunc)(std::string &path);
typedef int32_t (*ClearFunc)(void);

int CheckBoardId(Hpackage::PkgManager::PkgManagerPtr pkgManager, PackagesInfoPtr pkginfomanager);
int CheckVersion(Hpackage::PkgManager::PkgManagerPtr pkgManager, PackagesInfoPtr pkginfomanager);
int32_t UpdatePreProcess(UpdaterParams &upParams, Hpackage::PkgManager::PkgManagerPtr pkgManager);
int32_t UpdateAuth(std::string &path);
int32_t UpdateClear(void);

class PreProcess {
    DISALLOW_COPY_MOVE(PreProcess);
public:
    void RegisterHelper(PreProcessFunc ptr);
    void AuthHelper(AuthFunc ptr);
    void ClearHelper(ClearFunc ptr);
    static PreProcess &GetInstance();
    int32_t DoUpdatePreProcess(UpdaterParams &upParams, Hpackage::PkgManager::PkgManagerPtr pkgManager);
    int32_t DoUpdateAuth(std::string path);
    int32_t DoUpdateClear(void);
private:
    PreProcess() = default;
    ~PreProcess() = default;
    PreProcessFunc helper_ {};
    AuthFunc authHelper_ {};
    ClearFunc clearHelper_ {};
};
} // namespace Updater

#endif
