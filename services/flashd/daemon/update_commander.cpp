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

#include "update_commander.h"

#include "datetime_ex.h"
#include "flashd_define.h"
#include "flashd_utils.h"
#include "fs_manager/mount.h"
#include "package/pkg_manager.h"
#include "updater/updater.h"
#include "utils.h"

namespace Flashd {
namespace {
constexpr size_t CMD_PARAM_COUNT_MIN = 1;
}

UpdateCommander::~UpdateCommander()
{
    SafeCloseFile(fd_);
}

void UpdateCommander::DoCommand(const std::string &cmdParam, size_t fileSize)
{
    FLASHD_LOGI("start to update");
    startTime_ = OHOS::GetMicroTickCount();
    auto params = Split(cmdParam, { "-f" });
    if (params.size() < CMD_PARAM_COUNT_MIN) {
        FLASHD_LOGE("update param count is %u, not invaild", params.size());
        NotifyFail(CmdType::UPDATE);
        return;
    }

    if (auto ret = Updater::MountForPath(GetPathRoot(FLASHD_FILE_PATH)); ret != 0) {
        FLASHD_LOGE("MountForPath fail, ret = %d", ret);
        NotifyFail(CmdType::UPDATE);
        return;
    }

    if (access(FLASHD_FILE_PATH, F_OK) == -1) {
        mkdir(FLASHD_FILE_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

    fileSize_ = fileSize;
    filePath_ = FLASHD_FILE_PATH + GetFileName(cmdParam);
    fd_ = open(filePath_.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd_ < 0) {
        NotifyFail(CmdType::UPDATE);
        FLASHD_LOGE("open file fail, errno = %d ", errno);
        return;
    }
}

void UpdateCommander::DoCommand(const uint8_t *payload, int payloadSize)
{
    if (payload == nullptr || payloadSize <= 0) {
        NotifyFail(CmdType::UPDATE);
        FLASHD_LOGE("payload is null or payloadSize is invaild");
        return;
    }

    if (!DoUpdate(payload, payloadSize)) {
        NotifyFail(CmdType::UPDATE);
        return;
    }
}

bool UpdateCommander::DoUpdate(const uint8_t *payload, int payloadSize)
{
    if (fd_ < 0) {
        FLASHD_LOGE("file fd is invaild");
        return false;
    }

    auto writeSize = std::min(static_cast<size_t>(payloadSize), fileSize_ - currentSize_);
    if (writeSize <= 0) {
        FLASHD_LOGW("all the data has been written");
        return true;
    }

    if (!Updater::Utils::WriteFully(fd_, payload, writeSize)) {
        FLASHD_LOGE("WriteFully fail, errno = %d", errno);
        return false;
    }

    currentSize_ += writeSize;
    if (currentSize_ >= fileSize_) {
        auto useSec = static_cast<double>(OHOS::GetMicroTickCount() - startTime_) / OHOS::SEC_TO_MICROSEC;
        FLASHD_LOGI("update write success, size = %u bytes, %.3lf s", fileSize_, useSec);

        if (ExecUpdate(filePath_)) {
            FLASHD_LOGI("update success");
            NotifySuccess(CmdType::UPDATE);
            return true;
        }
        FLASHD_LOGE("update fail");
        NotifyFail(CmdType::UPDATE);
        return false;
    }
    UpdateProgress(CmdType::UPDATE);

    return true;
}

bool UpdateCommander::ExecUpdate(const std::string &packageName) const
{
    FLASHD_LOGI("packageName is %s", packageName.c_str());
    if (access(packageName.c_str(), 0) != 0) {
        FLASHD_LOGE("package is not exist");
        return false;
    }

    auto pkgManager = Hpackage::PkgManager::GetPackageInstance();
    if (pkgManager == nullptr) {
        FLASHD_LOGE("pkgManager is null");
        return false;
    }

    uint64_t pkgLen = 0;
    pkgManager->SetPkgDecodeProgress([&](int type, size_t writeLen, const void *context) { pkgLen += writeLen; });

    std::vector<std::string> components;
    if (auto ret = pkgManager->LoadPackage(packageName, Updater::Utils::GetCertName(), components); ret != 0) {
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        FLASHD_LOGE("LoadPackage fail, ret = %d", ret);
        return false;
    }

    if (auto ret = Updater::UpdatePreProcess(pkgManager, packageName); ret != 0) {
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        FLASHD_LOGE("UpdatePreProcess fail, ret = %d", ret);
        return false;
    }

#ifdef UPDATER_USE_PTABLE
    if (!Updater::PtableProcess(pkgManager, Updater::HOTA_UPDATE)) {
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        FLASHD_LOGE("PtableProcess fail");
        return false;
    }
#endif

    if (auto ret = Updater::ExecUpdate(pkgManager, 0, nullptr); ret != 0) {
        FLASHD_LOGE("ExecUpdate fail, ret = %d", ret);
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        return false;
    }

    Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
    FLASHD_LOGI("Update success and pkgLen is = %llu", pkgLen);
    return true;
}

void UpdateCommander::PostCommand()
{
    Updater::PostUpdater(true);
    Updater::Utils::DoReboot("");
}
} // namespace Flashd