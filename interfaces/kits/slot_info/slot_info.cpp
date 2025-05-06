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

#include "slot_info/slot_info.h"

#include "log/log.h"
#ifdef UPDATER_AB_SUPPORT
#include "v1_0/ipartition_slot.h"
using namespace OHOS::HDI::Partitionslot::V1_0;
#endif

namespace Updater {
#ifndef UPDATER_AB_SUPPORT
void GetPartitionSuffix(std::string &suffix)
{
    suffix = "";
}
void GetActivePartitionSuffix(std::string &suffix)
{
    suffix = "";
}
void SetActiveSlot()
{
}
#else
void GetPartitionSuffix(std::string &suffix)
{
    sptr<OHOS::HDI::Partitionslot::V1_0::IPartitionSlot> partitionslot =
        OHOS::HDI::Partitionslot::V1_0::IPartitionSlot::Get(true);
    if (partitionslot == nullptr) {
        LOG(ERROR) << "partitionslot ptr is nullptr";
        return;
    }
    int32_t curSlot = -1;
    std::string numOfSlots = "0";
    int32_t ret = partitionslot->GetSlotSuffix(curSlot, numOfSlots);
    LOG(INFO) << "Get slot info, curSlot: " << curSlot << "numOfSlots :" << numOfSlots;
    if (ret != 0 || curSlot <= 0 || curSlot > 2 || std::stoi(numOfSlots) != 2) { // 2: max slot num
        suffix = "";
        return;
    }

    int32_t updateSlot = curSlot == 1 ? 2 : 1;
    ret = partitionslot->GetSlotSuffix(updateSlot, suffix);
    if (ret != 0) {
        LOG(ERROR) << "Get slot suffix error, partitionPath: " << suffix;
        suffix = "";
    }
}

void GetActivePartitionSuffix(std::string &suffix)
{
    sptr<OHOS::HDI::Partitionslot::V1_0::IPartitionSlot> partitionslot =
        OHOS::HDI::Partitionslot::V1_0::IPartitionSlot::Get(true);
    if (partitionslot == nullptr) {
        LOG(ERROR) << "partitionslot ptr is nullptr";
        return;
    }
    int32_t curSlot = -1;
    std::string numOfSlots = "0";
    int32_t ret = partitionslot->GetCurrentSlot(curSlot, numOfSlots);
    LOG(INFO) << "Get slot info, curSlot: " << curSlot << "numOfSlots :" << numOfSlots;
    if (ret != 0 || curSlot <= 0 || curSlot > 2 || std::stoi(numOfSlots) != 2) { // 2: max slot num
        suffix = "";
        return;
    }

    ret = partitionslot->GetSlotSuffix(curSlot, suffix);
    if (ret != 0) {
        LOG(ERROR) << "Get slot suffix error, partitionPath: " << suffix;
        suffix = "";
    }
}

void SetActiveSlot()
{
    sptr<OHOS::HDI::Partitionslot::V1_0::IPartitionSlot> partitionslot =
        OHOS::HDI::Partitionslot::V1_0::IPartitionSlot::Get(true);
    if (partitionslot == nullptr) {
        LOG(ERROR) << "partitionslot ptr is nullptr";
        return;
    }
    int32_t curSlot = -1;
    std::string numOfSlots = "0";
    int32_t ret = partitionslot->GetCurrentSlot(curSlot, numOfSlots);
    LOG(INFO) << "Get slot info, curSlot: " << curSlot << "numOfSlots :" << numOfSlots;
    if (ret != 0 || curSlot <= 0 || curSlot > 2 || std::stoi(numOfSlots) != 2) { // 2: max slot num
        return;
    }

    int32_t activeSlot = curSlot == 1 ? 2 : 1;
    ret = partitionslot->SetActiveSlot(activeSlot);
    if (ret != 0) {
        LOG(ERROR) << "Set active slot error, slot: " << activeSlot;
    }
}
#endif
} // Updater
