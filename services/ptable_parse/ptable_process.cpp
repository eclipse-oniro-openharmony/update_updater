/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "log/log.h"
#include "ptable_manager.h"
#include "ptable_process.h"
#include "utils.h"

using namespace Hpackage;
namespace Updater {
extern "C" __attribute__((constructor)) void RegisterPtableHelper(void)
{
    PtablePreProcess::GetInstance().RegisterPtableHelper(PtableProcess);
}

void PtablePreProcess::RegisterPtableHelper(PtablePreProcessFunc ptr)
{
    helper_ = std::move(ptr);
}

bool PtablePreProcess::DoPtableProcess(UpdaterParams &upParams)
{
    if (helper_ == nullptr) {
        LOG(INFO) << "PtablePreProcess helper_ is null";
        return false;
    }
    return helper_(upParams);
}

bool PtableProcess(UpdaterParams &upParams)
{
    for (auto &path : upParams.updatePackage) {
        Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
        std::vector<std::string> components;
        if (pkgManager == nullptr) {
            LOG(ERROR) << "Fail to CreatePackageInstance";
            return false;
        }
        int32_t ret = pkgManager->LoadPackage(path, Utils::GetCertName(), components);
        if (ret != Hpackage::PKG_SUCCESS) {
            LOG(ERROR) << "LoadPackage fail ret:" << ret;
            Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
            return false;
        }
        DevicePtable& devicePtb = DevicePtable::GetInstance();
        devicePtb.LoadPartitionInfo();
        PackagePtable& packagePtb = PackagePtable::GetInstance();
        packagePtb.LoadPartitionInfo(pkgManager);
        if (!devicePtb.ComparePtable(packagePtb)) {
            LOG(INFO) << "Ptable NOT changed, no need to process!";
            Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
            continue;
        }
        if (upParams.updateMode != SDCARD_UPDATE) {
            if (devicePtb.ComparePartition(packagePtb, "USERDATA")) {
                LOG(ERROR) << "Hota update not allow userdata partition change!";
                Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
                return false;
            }
        }
        if (!packagePtb.WritePtableToDevice()) {
            LOG(ERROR) << "Ptable changed, write new ptable failed!";
            Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
            return false;
        }
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
    }
    return true;
}
}