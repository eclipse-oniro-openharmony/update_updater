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
#include "partitionslot_manager.h"

namespace Updater {
#ifndef UPDATER_AB_SUPPORT
void GetPartitionSuffix(std::string &suffix)
{
    suffix = "";
}
void SetActiveSlot()
{
}
#else
void GetPartitionSuffix(std::string &suffix)
{
    OHOS::HDI::Partitionslot::V1_0::PartitionSlotManager psMgr;
    int32_t curSlot = -1;
    int32_t numOfSlots = 0;
    int32_t ret = psMgr.GetCurrentSlot(curSlot, numOfSlots);
    LOG(INFO) << "Get slot info, curSlot: " << curSlot << "numOfSlots :" << numOfSlots;
    if (ret != 0 || curSlot <= 0 || curSlot > 2 || numOfSlots != 2) { // 2: max slot num
        suffix = "";
        return;
    }

    int32_t updateSlot = curSlot == 1 ? 2 : 1;
    ret = psMgr.GetSlotSuffix(updateSlot, suffix);
    if (ret != 0) {
        LOG(ERROR) << "Get slot suffix error, partitionPath: " << suffix;
        suffix = "";
    }
}

void SetActiveSlot()
{
    OHOS::HDI::Partitionslot::V1_0::PartitionSlotManager psMgr;
    int32_t curSlot = -1;
    int32_t numOfSlots = 0;
    int32_t ret = psMgr.GetCurrentSlot(curSlot, numOfSlots);
    LOG(INFO) << "Get slot info, curSlot: " << curSlot << "numOfSlots :" << numOfSlots;
    if (ret != 0 || curSlot <= 0 || curSlot > 2 || numOfSlots != 2) { // 2: max slot num
        return;
    }

    int32_t activeSlot = curSlot == 1 ? 2 : 1;
    ret = psMgr.SetActiveSlot(activeSlot);
    if (ret != 0) {
        LOG(ERROR) << "Set active slot error, slot: " << activeSlot;
    }
}
#endif
} // Updater
