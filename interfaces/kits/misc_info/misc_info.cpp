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

#include "misc_info/misc_info.h"
#include <cstring>
#include <unistd.h>
#include "fs_manager/mount.h"
#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"

namespace updater {
bool WriteUpdaterMessage(const std::string &path, const UpdateMessage &boot)
{
    char *realPath = realpath(path.c_str(), NULL);
    UPDATER_FILE_CHECK(realPath != nullptr, "realPath is NULL", return false);
    FILE* fp = fopen(realPath, "rb+");
    free(realPath);
    UPDATER_FILE_CHECK(fp != nullptr, "WriteUpdaterMessage fopen failed", return false);

    size_t ret = fwrite(&boot, sizeof(UpdateMessage), 1, fp);
    UPDATER_FILE_CHECK(ret == 1, "WriteUpdaterMessage fwrite failed", fclose(fp); return false);

    int res = fclose(fp);
    UPDATER_FILE_CHECK(res == 0, "WriteUpdaterMessage fclose failed", return false);
    return true;
}

bool ReadUpdaterMessage(const std::string &path, UpdateMessage &boot)
{
    char *realPath = realpath(path.c_str(), NULL);
    UPDATER_FILE_CHECK(realPath != nullptr, "realPath is NULL", return false);
    FILE* fp = fopen(realPath, "rb");
    free(realPath);
    UPDATER_FILE_CHECK(fp != nullptr, "ReadUpdaterMessage fopen failed", return false);

    struct UpdateMessage tempBoot {};
    size_t ret = fread(&tempBoot, sizeof(UpdateMessage), 1, fp);
    UPDATER_FILE_CHECK(ret == 1, "ReadUpdaterMessage fwrite failed", fclose(fp); return false);

    int res = fclose(fp);
    UPDATER_FILE_CHECK(res == 0, "ReadUpdaterMessage fclose failed", return false);
    UPDATER_FILE_CHECK(!memcpy_s(&boot, sizeof(UpdateMessage), &tempBoot, sizeof(UpdateMessage)),
        "ReadUpdaterMessage memcpy failed", return false);
    return true;
}

bool WriteUpdaterMiscMsg(const UpdateMessage &boot)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    UPDATER_INFO_CHECK(!path.empty(), "cannot get block device of partition", path = MISC_FILE);
    LOG(INFO) << "WriteUpdaterMiscMsg::misc path : " << path;
    return WriteUpdaterMessage(path, boot);
}

bool ReadUpdaterMiscMsg(UpdateMessage &boot)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    UPDATER_INFO_CHECK(!path.empty(), "cannot get block device of partition", path = MISC_FILE);
    LOG(INFO) << "ReadUpdaterMiscMsg::misc path : " << path;
    return ReadUpdaterMessage(path, boot);
}
} // updater
