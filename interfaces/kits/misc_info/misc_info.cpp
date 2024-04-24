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
#include "fs_manager/mount.h"
#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"

namespace Updater {
bool WriteUpdaterMessage(const std::string &path, const UpdateMessage &boot)
{
    char *realPath = realpath(path.c_str(), NULL);
    if (realPath == nullptr) {
        LOG(ERROR) << "realPath is NULL" << " : " << strerror(errno);
        return false;
    }
    FILE* fp = fopen(realPath, "rb+");
    free(realPath);
    if (fp == nullptr) {
        LOG(ERROR) << "WriteUpdaterMessage fopen failed" << " : " << strerror(errno);
        return false;
    }

    size_t ret = fwrite(&boot, sizeof(UpdateMessage), 1, fp);
    if (ret != 1) {
        LOG(ERROR) << "WriteUpdaterMessage fwrite failed" << " : " << strerror(errno);
        fclose(fp);
        return false;
    }
    if (fsync(fileno(fp)) == -1) {
        LOG(ERROR) << "WriteUpdaterMessage fsync failed" << " : " << strerror(errno);
    }

    int res = fclose(fp);
    if (res != 0) {
        LOG(ERROR) << "WriteUpdaterMessage fclose failed" << " : " << strerror(errno);
        return false;
    }
    return true;
}

bool ReadUpdaterMessage(const std::string &path, UpdateMessage &boot)
{
    char *realPath = realpath(path.c_str(), NULL);
    if (realPath == nullptr) {
        LOG(ERROR) << "realPath is NULL" << " : " << strerror(errno);
        return false;
    }
    FILE* fp = fopen(realPath, "rb");
    free(realPath);
    if (fp == nullptr) {
        LOG(ERROR) << "ReadUpdaterMessage fopen failed" << " : " << strerror(errno);
        return false;
    }

    struct UpdateMessage tempBoot {};
    size_t ret = fread(&tempBoot, sizeof(UpdateMessage), 1, fp);
    if (ret != 1) {
        LOG(ERROR) << "ReadUpdaterMessage fwrite failed" << " : " << strerror(errno);
        fclose(fp);
        return false;
    }

    int res = fclose(fp);
    if (res != 0) {
        LOG(ERROR) << "ReadUpdaterMessage fclose failed" << " : " << strerror(errno);
        return false;
    }
    if (memcpy_s(&boot, sizeof(UpdateMessage), &tempBoot, sizeof(UpdateMessage)) != EOK) {
        LOG(ERROR) << "ReadUpdaterMessage memcpy failed" << " : " << strerror(errno);
        return false;
    }
    return true;
}

bool WriteUpdaterMiscMsg(const UpdateMessage &boot)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    if (path.empty()) {
        LOG(INFO) << "cannot get block device of partition";
        path = MISC_FILE;
    }
    LOG(INFO) << "WriteUpdaterMiscMsg::misc path : " << path;
    return WriteUpdaterMessage(path, boot);
}

bool ReadUpdaterMiscMsg(UpdateMessage &boot)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    if (path.empty()) {
        LOG(INFO) << "cannot get block device of partition";
        path = MISC_FILE;
    }
    return ReadUpdaterMessage(path, boot);
}

bool WriteUpdaterParaMisc(const UpdaterPara &para)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    if (path.empty()) {
        LOG(INFO) << "WriteUpdaterParaMisc cannot get block device of partition";
        path = MISC_FILE;
    }

    char *realPath = realpath(path.c_str(), NULL);
    if (realPath == nullptr) {
        LOG(ERROR) << "WriteUpdaterParaMisc realPath is NULL" << ":" << strerror(errno);
        return false;
    }
    FILE *fp = fopen(realPath, "rb+");
    free(realPath);
    if (fp == nullptr) {
        LOG(ERROR) << "WriteUpdaterParaMisc fopen failed" << ":" << strerror(errno);
        return false;
    }

    if (lseek(fileno(fp), MISC_UPDATER_PARA_OFFSET, SEEK_SET) == -1) {
        LOG(ERROR) << "lseek fp failed";
        fclose(fp);
        return false;
    }
    size_t ret = fwrite(&para, sizeof(UpdaterPara), 1, fp);
    if (ret != 1) {
        LOG(ERROR) << "WriteUpdaterParaMisc fwrite failed" << " : " << strerror(errno);
        fclose(fp);
        return false;
    }

    if (fsync(fileno(fp)) == -1) {
        LOG(ERROR) << "WriteUpdaterParaMisc fsync failed" << " : " << strerror(errno);
    }

    if (fclose(fp) != 0) {
        LOG(ERROR) << "WriteUpdaterParaMisc fclose failed" << " : " << strerror(errno);
    }
    return true;
}

bool ReadUpdaterParaMisc(UpdaterPara &para)
{
    auto path = GetBlockDeviceByMountPoint(MISC_PATH);
    if (path.empty()) {
        LOG(INFO) << "ReadUpdaterParaMisc cannot get block device of partition";
        path = MISC_FILE;
    }

    char *realPath = realpath(path.c_str(), NULL);
    if (realPath == nullptr) {
        LOG(ERROR) << "ReadUpdaterParaMisc realPath is NULL" << ":" << strerror(errno);
        return false;
    }
    FILE *fp = fopen(realPath, "rb");
    free(realPath);
    if (fp == nullptr) {
        LOG(ERROR) << "ReadUpdaterParaMisc fopen failed" << ":" << strerror(errno);
        return false;
    }

    if (lseek(fileno(fp), MISC_UPDATER_PARA_OFFSET, SEEK_SET) == -1) {
        LOG(ERROR) << "lseek fp failed";
        fclose(fp);
        return false;
    }
    size_t ret = fread(&para, sizeof(UpdaterPara), 1, fp);
    if (ret != 1) {
        LOG(ERROR) << "ReadUpdaterParaMisc fwrite failed" << " : " << strerror(errno);
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

void ClearUpdaterParaMisc(void)
{
    struct UpdaterPara cleanPara {};
    if (!WriteUpdaterParaMisc(cleanPara)) {
        LOG(ERROR) << "Clear para including language of misc failed";
    }
}
} // Updater
