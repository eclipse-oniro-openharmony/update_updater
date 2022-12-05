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

#include <unordered_map>

#include "datetime_ex.h"
#include "flashd_define.h"
#include "flashd_utils.h"
#include "fs_manager/mount.h"
#include "package/pkg_manager.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "updaterkits/updaterkits.h"
#include "utils.h"

namespace Flashd {
namespace {
constexpr size_t CMD_PARAM_COUNT_MIN = 1;
}

UpdateCommander::~UpdateCommander()
{
    SafeCloseFile(fd_);
}

void UpdateCommander::DoCommand(const std::string &cmmParam, size_t fileSize)
{
    FLASHD_LOGI("start to update");
    startTime_ = OHOS::GetMicroTickCount();
    auto params = Split(cmmParam, { "-f" });
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
    filePath_ = FLASHD_FILE_PATH + GetFileName(cmmParam);
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
        SafeCloseFile(fd_);
        auto useSec = static_cast<double>(OHOS::GetMicroTickCount() - startTime_) / OHOS::SEC_TO_MICROSEC;
        FLASHD_LOGI("update write file success, size = %u bytes, %.3lf s", fileSize_, useSec);
        NotifySuccess(CmdType::UPDATE);
        return true;
    }
    UpdateProgress(CmdType::UPDATE);
    return true;
}

bool UpdateCommander::ExecUpdate() const
{
    const std::string miscFile = "/dev/block/by-name/misc";
    std::vector<std::string> filePath;
    filePath.push_back(filePath_);
    return RebootAndInstallUpgradePackage(miscFile, filePath);
}

void UpdateCommander::PostCommand()
{
    SaveLog();
    if (!ExecUpdate()) {
        FLASHD_LOGE("ExecUpdate failed");
    }
}

void UpdateCommander::SaveLog() const
{
    const std::unordered_map<std::string, std::string> logMap = {
        { Updater::TMP_LOG, Updater::UPDATER_LOG },
        { Updater::TMP_ERROR_CODE_PATH, Updater::ERROR_CODE_PATH },
        { FLASHD_HDC_LOG_PATH, Updater::UPDATER_HDC_LOG },
        { Updater::TMP_STAGE_LOG, Updater::UPDATER_STAGE_LOG },
    };
    constexpr mode_t mode = 0640;

    for (const auto &iter : logMap) {
        if (!Updater::Utils::CopyUpdaterLogs(iter.first, iter.second)) {
            FLASHD_LOGW("Copy %s failed!", GetFileName(iter.second).c_str());
        }
        chmod((iter.second).c_str(), mode);
    }
}
} // namespace Flashd