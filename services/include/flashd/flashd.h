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

#ifndef UPDATER_FLASHING_H
#define UPDATER_FLASHING_H

#include <string>

namespace Flashd {
enum {
    FLASHING_SYSTEM_ERROR = 100,
    FLASHING_ARG_INVALID,
    FLASHING_IMAGE_INVALID,
    FLASHING_PART_NOEXIST,
    FLASHING_NOPERMISSION,
    FLASHING_PART_WRITE_ERROR,
    FLASHING_PACKAGE_INVALID,
    FLASHING_SPACE_NOTENOUGH,
    FLASHING_INVALID_SPACE,
    FLASHING_FSTYPE_NOT_SUPPORT,
    FLASHING_OPEN_PART_ERROR,
};

enum class CmdType : uint8_t {
    UPDATE = 0,
    FLASH,
    ERASE,
    FORMAT,
    UNKNOWN
};

static constexpr uint8_t PERCENT_FINISH = 100;
static constexpr uint8_t PERCENT_CLEAR = UINT8_MAX;

#ifndef UPDATER_UT
constexpr const char *FLASHD_FILE_PATH = "/data/updater/";
constexpr const char *FLASHD_HDC_LOG_PATH = "/tmp/flashd_hdc.log";
#else
constexpr const char *FLASHD_FILE_PATH = "/data/updater/updater/";
constexpr const char *FLASHD_HDC_LOG_PATH = "/data/updater/flashd_hdc.log";
#endif

int flashd_main(int argc, char **argv);

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
int SetParameter(const char *key, const char *value);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
} // namespace Flashd
#endif /* UPDATER_FLASHING_H */
