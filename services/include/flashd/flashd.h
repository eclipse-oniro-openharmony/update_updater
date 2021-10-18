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
#include <functional>
#include <iostream>

namespace flashd {
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
};

enum UpdateModType {
    UPDATEMOD_UPDATE,
    UPDATEMOD_FLASH,
    UPDATEMOD_ERASE,
    UPDATEMOD_FORMAT,
    UPDATEMOD_MAX
};

static constexpr uint16_t MAX_SIZE_BUF = 1024;
static constexpr uint32_t PERCENT_FINISH = 100;
static constexpr uint32_t PERCENT_CLEAR = (uint32_t)-1;

#ifndef LOCAL_SUPPORT
const std::string FORMAT_TOOL_FOR_EXT4 = "/bin/mke2fs";
const std::string FORMAT_TOOL_FOR_F2FS = "/bin/make_f2fs";
const std::string RESIZE_TOOL = "/bin/resize2fs";
const std::string FLASHD_FILE_PATH = "/data/updater/";
const std::string FLASHD_HDC_LOG_PATH = "/tmp/flashd_hdc.log";
#else
const std::string FORMAT_TOOL_FOR_EXT4 = "/usr/sbin/mke2fs";
const std::string FORMAT_TOOL_FOR_F2FS = "/system/bin/make_f2fs";
const std::string RESIZE_TOOL = "/bin/resize2fs";
const std::string FLASHD_FILE_PATH = "/home/axw/develop/build/";
const std::string FLASHD_HDC_LOG_PATH = "/home/axw/develop/build/hdc.log";
#endif
static constexpr uint32_t MIN_BLOCKS_FOR_UPDATE = 1024 * 1024;
static constexpr uint32_t DEFAULT_BLOCK_SIZE = 4096;
static constexpr uint32_t DEFAULT_SIZE_UNIT = 1024 * 1024;

using FlashHandle = void *;
int flashd_main(int argc, char **argv);
using ProgressFunction = std::function<void(uint32_t type, size_t dataLen, const void *context)>;

int CreateFlashInstance(FlashHandle *handle, std::string &errorMsg, ProgressFunction progressor);
int DoUpdaterPrepare(FlashHandle handle, uint8_t type, const std::string &cmdParam, std::string &filePath);
int DoUpdaterFlash(FlashHandle handle, uint8_t type, const std::string &cmdParam, const std::string &filePath);
int DoUpdaterFinish(FlashHandle handle, uint8_t type, const std::string &partition);
} // namespace flashd
#endif /* UPDATER_FLASHING_H */
