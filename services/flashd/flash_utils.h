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

#ifndef FLASHING_UTILS_H
#define FLASHING_UTILS_H
#include <cstdlib>

#include "flashd/flashd.h"
#include "log/log.h"
#include "securec.h"

namespace Flashd {
#define FLASHING_LOGE(format, ...) Logger(Updater::ERROR, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define FLASHING_DEBUG(format, ...) Logger(Updater::DEBUG, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define FLASHING_LOGI(format, ...) Logger(Updater::INFO, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define FLASHING_LOGW(format, ...) Logger(Updater::WARNING, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)

#define FLASHING_CHECK(retCode, exper, ...) \
    if (!(retCode)) {                       \
        FLASHING_LOGE(__VA_ARGS__);         \
        exper;                              \
    }

static constexpr size_t BUFFER_SIZE = 64 * 1024;
static constexpr uint32_t LOOP_MAJOR = 7;
static constexpr uint32_t SCSI_CDROM_MAJOR = 11;
static constexpr uint32_t SCSI_DISK0_MAJOR = 8;
static constexpr uint32_t SDMMC_MAJOR = 179;
static constexpr uint32_t DEVICE_PATH_SIZE = 256;
static constexpr uint32_t LINE_BUFFER_SIZE = 256;
static constexpr size_t SECTOR_SIZE_DEFAULT = 512;
#define SCSI_BLK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR)

class FlashService;
using FlashingPtr = FlashService *;
} // namespace flashd
#endif // FLASHING_UTILS_H
