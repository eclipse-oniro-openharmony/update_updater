/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include "flash_commander.h"

#include "datetime_ex.h"
#include "flashd_define.h"
#include "flashd_utils.h"
#include "fs_manager/mount.h"
#include "ptable_parse/ptable_manager.h"
#include "updater/updater.h"

namespace Flashd {
namespace {
constexpr size_t CMD_PARAM_COUNT_MIN = 2;
}

void FlashCommander::DoCommand(const std::string &cmmParam, size_t fileSize)
{
    FLASHD_LOGI("start to flash");
    auto params = Split(cmmParam, { "-f" });
    if (params.size() < CMD_PARAM_COUNT_MIN) {
        FLASHD_LOGE("flash param count is %u, not invaild", params.size());
        NotifyFail(CmdType::FLASH);
        return;
    }

    fileSize_ = fileSize;
    partName_ = params[0];
    startTime_ = OHOS::GetMicroTickCount();

    if (auto ret = Updater::MountForPath(GetPathRoot(FLASHD_FILE_PATH)); ret != 0) {
        FLASHD_LOGE("MountForPath fail, ret = %d", ret);
        NotifyFail(CmdType::FLASH);
        return;
    }

    if (access(FLASHD_FILE_PATH, F_OK) == -1) {
        mkdir(FLASHD_FILE_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    return;
}

void FlashCommander::DoCommand(const uint8_t *payload, int payloadSize)
{
    if (payload == nullptr || payloadSize <= 0) {
        FLASHD_LOGE("payload is null or payloadSize is invaild");
        NotifyFail(CmdType::FLASH);
        return;
    }

    auto writeSize = DoFlash(payload, payloadSize);
    if (writeSize < 0) {
        NotifyFail(CmdType::FLASH);
        return;
    }

    currentSize_ += static_cast<size_t>(writeSize);
    if (currentSize_ >= fileSize_) {
        auto useSec = static_cast<double>(OHOS::GetMicroTickCount() - startTime_) / OHOS::SEC_TO_MICROSEC;
        FLASHD_LOGI("flash success, size = %u bytes, %.3lf s", fileSize_, useSec);
        NotifySuccess(CmdType::FLASH);
        return;
    }
    UpdateProgress(CmdType::FLASH);
}

bool FlashCommander::InitPartition(const std::string &partName, const uint8_t *buffer, int bufferSize)
{
#ifdef UPDATER_USE_PTABLE
    DevicePtable::GetInstance().LoadPartitionInfo();
#endif
    std::unique_ptr<FlashdWriter> writer = FlashdImageWriter::GetInstance().GetWriter(
        partName, buffer, bufferSize);
    if (writer == nullptr) {
        FLASHD_LOGE("FlashdImageWriter GetWriter error");
        return false;
    }
    partition_ = std::make_unique<Partition>(partName, std::move(writer));
    return (partition_ == nullptr) ? false : true;
}

int FlashCommander::DoFlash(const uint8_t *payload, int payloadSize)
{
    if (partition_ == nullptr) {
        if (!InitPartition(partName_, payload, payloadSize)) {
            FLASHD_LOGE("DoFlash InitPartition fail");
            return -1;
        }
    }

    auto writeSize = std::min(static_cast<size_t>(payloadSize), fileSize_ - currentSize_);
    if (writeSize <= 0) {
        FLASHD_LOGW("all the data is written");
        return 0;
    }

    if (partition_->DoFlash(payload, writeSize) != 0) {
        FLASHD_LOGE("DoFlash fail");
        return -1;
    }
    return writeSize;
}

void FlashCommander::PostCommand()
{
    Updater::PostUpdater(false);
}
} // namespace Flashd