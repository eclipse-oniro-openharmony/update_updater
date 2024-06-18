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

#include "update_bin/bin_flow_update.h"
#include "update_bin/bin_process.h"
#include "log.h"

using namespace Hpackage;
using namespace Updater;

namespace OHOS {
    int TestBinFlowUpdater(const uint8_t* data, size_t size)
    {
        InitUpdaterLogger("UPDATER", "updater_log.log", "updater_status.log", "error_code.log");
        LOG(INFO) << "TestBinFlowUpdater start";
        std::string packagePath = std::string(reinterpret_cast<const char*>(data), size);
        char *realPath = realpath(packagePath.c_str(), nullptr);
        if (realPath == nullptr) {
            LOG(ERROR) << "realPath is NULL" << " : " << strerror(errno);
            packagePath = "/data/fuzz/test/updater_flow.zip";
        }
        PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
        if (pkgManager == nullptr) {
            LOG(ERROR) << "pkgManager is nullptr";
            return -1;
        }

        std::vector<std::string> components;
        int32_t ret = pkgManager->LoadPackage(packagePath, Utils::GetCertName(), components);
        if (ret != PKG_SUCCESS) {
            LOG(ERROR) << "Fail to load package";
            PkgManager::ReleasePackageInstance(pkgManager);
            return -1;
        }

        ret = Updater::ExecUpdate(pkgManager, false, packagePath,
            [](const char *cmd, const char *content) {
                LOG(INFO) << "pip msg, " << cmd << ":" << content;
            });
        PkgManager::ReleasePackageInstance(pkgManager);
        return ret;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::TestBinFlowUpdater(data, size);
    return 0;
}

