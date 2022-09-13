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

#include "fs_manager/mount.h"
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <linux/fs.h>
#include "log/log.h"
#include "utils.h"

namespace Updater {
using Updater::Utils::SplitString;
static std::string g_defaultUpdaterFstab = "";
static Fstab *g_fstab = nullptr;
static const std::string PARTITION_PATH = "/dev/block/by-name";

static std::string GetFstabFile()
{
    /* check vendor fstab files from specific directory */
    std::vector<const std::string> specificFstabFiles = {"/vendor/etc/fstab.updater"};
    for (auto& fstabFile : specificFstabFiles) {
        if (access(fstabFile.c_str(), F_OK) == 0) {
            return fstabFile;
        }
    }
    return "";
}

MountStatus GetMountStatusForPath(const std::string &path)
{
    FstabItem *item = FindFstabItemForPath(*g_fstab, path.c_str());
    if (item == nullptr) {
        return MountStatus::MOUNT_ERROR;
    }
    return GetMountStatusForMountPoint(item->mountPoint);
}

void LoadFstab()
{
    std::string fstabFile = g_defaultUpdaterFstab;
    if (fstabFile.empty()) {
        fstabFile = GetFstabFile();
        if (fstabFile.empty()) {
            fstabFile = "/etc/fstab.updater";
        }
    }
    // Clear fstab before read fstab file.
    if ((g_fstab = ReadFstabFromFile(fstabFile.c_str(), false)) == nullptr) {
        LOG(WARNING) << "Read " << fstabFile << " failed";
        return;
    }

    LOG(DEBUG) << "Updater filesystem config info:";
    for (FstabItem *item = g_fstab->head; item != nullptr; item = item->next) {
        LOG(DEBUG) << "\tDevice: " << item->deviceName;
        LOG(DEBUG) << "\tMount point : " << item->mountPoint;
        LOG(DEBUG) << "\tFs type : " << item->fsType;
        LOG(DEBUG) << "\tMount options: " << item->mountOptions;
    }
}

void LoadSpecificFstab(const std::string &fstabName)
{
    g_defaultUpdaterFstab = fstabName;
    LoadFstab();
    g_defaultUpdaterFstab = "";
}

int UmountForPath(const std::string& path)
{
    FstabItem *item = FindFstabItemForPath(*g_fstab, path.c_str());
    if (item == nullptr) {
        LOG(ERROR) << "Cannot find fstab item for " << path << " to umount.";
        return -1;
    }

    LOG(DEBUG) << "Umount for path " << path;
    MountStatus rc = GetMountStatusForMountPoint(item->mountPoint);
    if (rc == MOUNT_ERROR) {
        return -1;
    } else if (rc == MOUNT_UMOUNTED) {
        return 0;
    } else {
        int ret = umount(item->mountPoint);
        if (ret == -1) {
            LOG(ERROR) << "Umount " << item->mountPoint << "failed: " << errno;
            return -1;
        }
    }
    return 0;
}

int MountForPath(const std::string &path)
{
    FstabItem *item = FindFstabItemForPath(*g_fstab, path.c_str());
    int ret = -1;
    if (item == nullptr) {
        LOG(ERROR) << "Cannot find fstab item for " << path << " to mount.";
        return -1;
    }

    LOG(DEBUG) << "Mount for path " << path;
    MountStatus rc = GetMountStatusForMountPoint(item->mountPoint);
    if (rc == MountStatus::MOUNT_ERROR) {
        ret = -1;
    } else if (rc == MountStatus::MOUNT_MOUNTED) {
        LOG(INFO) << path << " already mounted";
        ret = 0;
    } else {
        ret = MountOneItem(item);
    }
    return ret;
}

void ErasePartition(const std::string &devPath)
{
    std::string realPath {};
    if (!Utils::PathToRealPath(devPath, realPath)) {
        LOG(ERROR) << "realpath failed:" << devPath;
        return;
    }
    int fd = open(realPath.c_str(), O_RDWR | O_LARGEFILE);
    if (fd == -1) {
        LOG(ERROR) << "open failed:" << realPath;
        return;
    }

    uint64_t size = 0;
    int ret = ioctl(fd, BLKGETSIZE64, &size);
    if (ret < 0) {
        LOG(ERROR) << "get partition size failed:" << size;
        close(fd);
        return;
    }

    LOG(INFO) << "erase partition size:" << size;

    uint64_t range[] { 0, size };
    ret = ioctl(fd, BLKDISCARD, &range);
    if (ret < 0) {
        LOG(ERROR) << "erase partition failed";
    }
    close(fd);

    return;
}

int FormatPartition(const std::string &path, bool isZeroErase)
{
    FstabItem *item = FindFstabItemForPath(*g_fstab, path.c_str());
    if (item == nullptr) {
        LOG(ERROR) << "Cannot find fstab item for " << path << " to format.";
        return -1;
    }

    if (strcmp(item->mountPoint, "/") == 0) {
        /* Can not format root */
        return 0;
    }

    if (!IsSupportedFilesystem(item->fsType)) {
        LOG(ERROR) << "Try to format " << item->mountPoint << " with unsupported file system type: " << item->fsType;
        return -1;
    }

    // Umount first
    if (UmountForPath(path) != 0) {
        return -1;
    }

    if (isZeroErase) {
        ErasePartition(item->deviceName);
    }

    int ret = DoFormat(item->deviceName, item->fsType);
    if (ret != 0) {
        LOG(ERROR) << "Format " << path << " failed";
    }
    return ((ret != 0) ? -1 : 0);
}

int SetupPartitions(PackageUpdateMode mode)
{
    if (!Utils::IsUpdaterMode()) {
        LOG(ERROR) << "live update mode";
        return 0;
    }

    if (g_fstab == NULL || g_fstab->head == NULL) {
        LOG(ERROR) << "Fstab is invalid";
        return -1;
    }
    for (const FstabItem *item = g_fstab->head; item != nullptr; item = item->next) {
        std::string mountPoint(item->mountPoint);
        std::string fsType(item->fsType);
        if (mountPoint == "/" || mountPoint == "/tmp" || fsType == "none" ||
            mountPoint == "/sdcard") {
            continue;
        }

        if (mountPoint == "/data" && mode != SDCARD_UPDATE) {
            if (MountForPath(mountPoint) != 0) {
                LOG(ERROR) << "Expected partition " << mountPoint << " is not mounted.";
                return -1;
            }
            continue;
        }
        if (UmountForPath(mountPoint) != 0) {
            LOG(ERROR) << "Umount " << mountPoint << " failed";
            return -1;
        }
    }
    return 0;
}

const std::string GetBlockDeviceByMountPoint(const std::string &mountPoint)
{
    if (mountPoint.empty()) {
        LOG(ERROR) << "mountPoint empty error.";
        return "";
    }
    std::string blockDevice = PARTITION_PATH + mountPoint;
    if (mountPoint[0] != '/') {
        blockDevice = PARTITION_PATH + "/" + mountPoint;
    }
    if (g_fstab != nullptr) {
        FstabItem *item = FindFstabItemForMountPoint(*g_fstab, mountPoint.c_str());
        if (item != NULL) {
            blockDevice = item->deviceName;
        }
    }
    return blockDevice;
}
} // updater
