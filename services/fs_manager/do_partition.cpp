/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cerrno>
#include <cstring>
#include <linux/blkpg.h>
#include <linux/fs.h>
#include <libgen.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include "fs_manager/cmp_partition.h"
#include "fs_manager/mount.h"
#include "fs_manager/partitions.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "partition_const.h"
#include "securec.h"

namespace Updater {
namespace {
constexpr const char *USERDATA_PARTNAME = "userdata";
constexpr const char *UPDATER_PARTNAME = "updater";
}

static int BlkpgPartCommand(const Partition &part, struct blkpg_partition &pg, int op)
{
    struct blkpg_ioctl_arg args {};
    args.op = op;
    args.flags = 0;
    args.datalen = static_cast<int>(sizeof(struct blkpg_partition));
    args.data = static_cast<void *>(&pg);

    int ret = 0;
#ifndef UPDATER_UT
    ret = ioctl(part.partfd, BLKPG, &args);
#endif
    if (ret < 0) {
        LOG(ERROR) << "ioctl of partition " << part.partName << " with operation " << op << " failed";
    }
    return ret;
}

static int DoUmountDiskPartition(const Partition &part)
{
    std::string partName = std::string("/") + part.partName;
    int ret = UmountForPath(partName);
    if (ret == -1) {
        LOG(ERROR) << "Umount " << partName << " failed: " << errno;
        return 0;
    }
    return 1;
}

static void DoFsync(const BlockDevice &dev)
{
    BlockSpecific* bs = BLOCK_SPECIFIC(&dev);
    int status;

    while (true) {
        status = fsync (bs->fd);
        if (status >= 0) {
            break;
        }
    }
}

static int BlockSync(const Disk &disk)
{
    if (disk.dev->readOnly) {
        return 0;
    }
    DoFsync(*(disk.dev));
    return 1;
}

static int BlkpgRemovePartition(const Partition &part)
{
    struct blkpg_partition blkPart {};
    if (memset_s(&blkPart, sizeof(blkPart), 0, sizeof(blkPart)) != EOK) {
        return -1;
    }
    blkPart.pno = part.partNum;
    return BlkpgPartCommand(part, blkPart, BLKPG_DEL_PARTITION);
}

static int BlockDiskOpen(Disk &disk)
{
    disk.dev->fd = open(disk.dev->devPath.c_str(), RW_MODE);
    if (disk.dev->fd < 0) {
        LOG(WARNING) << "open fail: " << disk.dev->devPath << errno;
    }
    return disk.dev->fd;
}

static void BlockDiskClose(Disk &disk)
{
    if (disk.dev != nullptr) {
        if (disk.dev->fd > 0) {
            close(disk.dev->fd);
            disk.dev->fd = -1;
        }
    }
}

static bool DoRmPartition(const Disk &disk, int partn)
{
    Partition *part = nullptr;
    part = GetPartition(disk, partn);
    if (part == nullptr) {
        LOG(ERROR) << "Cannot get partition info for partition number: " << partn;
        return false;
    }

    if (disk.dev->fd < 0) {
        return false;
    }
    part->partfd = disk.dev->fd;
    int ret = BlkpgRemovePartition(*part);
    part->partfd = -1;
    if (ret < 0) {
        LOG(ERROR) << "Delete part failed";
        return false;
    }
    return true;
}

static int BlkpgAddPartition(Partition &part)
{
    struct blkpg_partition blkPart {};
    if (memset_s(&blkPart, sizeof(blkPart), 0, sizeof(blkPart)) != EOK) {
        return 0;
    }
    blkPart.start = static_cast<long long>(part.start * SECTOR_SIZE_DEFAULT);
    LOG(INFO) << "blkPart.start " << blkPart.start;
    blkPart.length = static_cast<long long>(part.length * SECTOR_SIZE_DEFAULT);
    LOG(INFO) << "blkPart.length " << blkPart.length;
    blkPart.pno = part.partNum;
    LOG(INFO) << "blkPart.pno " << blkPart.pno;
    if (strncpy_s(blkPart.devname, BLKPG_DEVNAMELTH, part.devName.c_str(), part.devName.size()) != EOK) {
        return 0;
    }
    LOG(INFO) << "blkPart.devname " << blkPart.devname;
    if (strncpy_s(blkPart.volname, BLKPG_VOLNAMELTH, part.partName.c_str(), part.partName.size()) != EOK) {
        return 0;
    }
    LOG(INFO) << "blkPart.volname " << blkPart.volname;
    if (BlkpgPartCommand(part, blkPart, BLKPG_ADD_PARTITION) < 0) {
        return 0;
    }
    return 1;
}

static bool DoAddPartition(const Disk &disk, Partition &part)
{
    if (disk.dev->fd < 0) {
        return false;
    }

    part.partfd = disk.dev->fd;
    int ret = BlkpgAddPartition(part);
    part.partfd = -1;
    if (ret == 0) {
        LOG(ERROR) << "Add partition failed";
        return false;
    }
    return true;
}

static void DestroyDiskPartitions(Disk &disk)
{
    if (!disk.partList.empty()) {
        for (auto& p : disk.partList) {
            if (p != nullptr) {
                free(p);
            }
        }
    }
    disk.partList.clear();
}

static void DestroyDiskDevices(const Disk &disk)
{
    if (disk.dev != nullptr) {
        if (disk.dev->specific != nullptr) {
            free(disk.dev->specific);
        }
        free(disk.dev);
    }
}

static bool WriteDiskPartitionToMisc(PartitonList &nlist)
{
    UPDATER_CHECK_ONLY_RETURN(nlist.empty() == 0, return false);
    char blkdevparts[MISC_RECORD_UPDATE_PARTITIONS_SIZE] = "mmcblk0:";
    std::sort(nlist.begin(), nlist.end(), [](const struct Partition *a, const struct Partition *b) {
            return (a->start < b->start);
    }); // Sort in ascending order
    char tmp[SMALL_BUFFER_SIZE] = {0};
    size_t size = 0;
    for (auto& p : nlist) {
        UPDATER_CHECK_ONLY_RETURN(memset_s(tmp, sizeof(tmp), 0, sizeof(tmp)) == 0, return false);
        if (p->partName == "userdata") {
            UPDATER_CHECK_ONLY_RETURN(snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "-(%s),",
                p->partName.c_str()) != -1, return false);
        } else {
            size = static_cast<size_t>(p->length * SECTOR_SIZE_DEFAULT / DEFAULT_SIZE_1MB);
            UPDATER_CHECK_ONLY_RETURN(snprintf_s(tmp, sizeof(tmp), sizeof(tmp) - 1, "%luM(%s),",
                size, p->partName.c_str()) != -1, return false);
        }
        int ncatRet = strncat_s(blkdevparts, MISC_RECORD_UPDATE_PARTITIONS_SIZE - 1, tmp, strlen(tmp));
        UPDATER_ERROR_CHECK(ncatRet == EOK, "Block device name overflow", return false);
    }

    blkdevparts[strlen(blkdevparts) - 1] = '\0';
    LOG(INFO) << "blkdevparts is " << blkdevparts;

    const std::string miscDevPath = GetBlockDeviceByMountPoint("/misc");
    char *realPath = realpath(miscDevPath.c_str(), NULL);
    UPDATER_ERROR_CHECK(realPath != nullptr, "realPath is NULL", return false);
    FILE *fp = fopen(realPath, "rb+");
    free(realPath);
    UPDATER_ERROR_CHECK(fp, "fopen error " << errno, return false);

    if (fseek(fp, MISC_RECORD_UPDATE_PARTITIONS_OFFSET, SEEK_SET) != 0) {
        LOG(ERROR) << "fseek error";
        fclose(fp);
        return false;
    }
    size_t ret = fwrite(blkdevparts, sizeof(blkdevparts), 1, fp);
    UPDATER_ERROR_CHECK(ret >= 0, "fwrite error " << errno, fclose(fp); return false);

    int fd = fileno(fp);
    fsync(fd);
    fclose(fp);
    return true;
}

static bool AddPartitions(const Disk &disk, const PartitonList &ulist, int &partitionAddedCounter)
{
    if (!ulist.empty()) {
        int userNum = GetPartitionNumByPartName(USERDATA_PARTNAME, disk.partList);
        int step = 1;
        char pdevname[DEVPATH_SIZE] = {0};
        for (auto& p2 : ulist) {
            if (p2->partName == USERDATA_PARTNAME) {
                LOG(INFO) << "Change userdata image is not support.";
                continue;
            }
            if (p2->partName == UPDATER_PARTNAME) {
                LOG(ERROR) << "Change updater image is not supported.";
                continue;
            }
            p2->partNum = userNum + step;
            if (snprintf_s(pdevname, sizeof(pdevname), sizeof(pdevname) - 1, "%sp%d", MMC_DEV, p2->partNum) == -1) {
                return false;
            }
            p2->devName.clear();
            p2->devName = pdevname;
            LOG(INFO) << "Adding partition " << p2->partName;
            if (!DoAddPartition (disk, *p2)) {
                LOG(ERROR) << "Add partition fail for " << p2->partName;
                return false;
            }
            step++;
            partitionAddedCounter++;
        }
    }
    return true;
}

static bool RemovePartitions(const Disk &disk, int &partitionRemovedCounter)
{
    PartitonList pList = disk.partList;
    for (const auto &it : pList) {
        if (it->changeType == NOT_CHANGE) {
            continue;
        }
        if (it->partName == UPDATER_PARTNAME) {
            LOG(ERROR) << "Cannot delete updater partition.";
            continue;
        }

        if (it->partName == USERDATA_PARTNAME) {
            LOG(INFO) << "Cannot delete userdata partition.";
            continue;
        }
        if (DoUmountDiskPartition(*it) == 0) {
            continue;
        }
        LOG(INFO) << "Removing partition " << it->partName;
        if (!DoRmPartition (disk, it->partNum)) {
            LOG(ERROR) << "Remove partition failed.";
            return false;
        }
        partitionRemovedCounter++;
    }
    return true;
}

int DoPartitions(PartitonList &nlist)
{
    LOG(INFO) << "do_partitions start ";
    UPDATER_ERROR_CHECK(!nlist.empty(), "newpartitionlist is empty ", return 0);
    const std::string path = MMC_PATH;
    int get = 0;
    int fd = -1;
    int partitionChangedCounter = 1;
    PartitonList ulist;
    ulist.clear();
    int ret = DiskAlloc(path);
    UPDATER_ERROR_CHECK(ret, "path not exist " << path, return 0);
    int sum = ProbeAllPartitions();
    UPDATER_ERROR_CHECK(sum, "partition sum  is zero! ", return 0);
    Disk *disk = GetRegisterBlockDisk(MMC_PATH);
    if (disk == nullptr) {
        LOG(ERROR) << "getRegisterdisk fail! ";
        return 0;
    }
    int reg = RegisterUpdaterPartitionList(nlist, disk->partList);
    UPDATER_ERROR_CHECK(reg, "register updater list fail! ", free(disk); return 0);
    get = GetRegisterUpdaterPartitionList(ulist);
    UPDATER_ERROR_CHECK(get, "get updater list fail! ", goto error);

    fd = BlockDiskOpen(*disk);
    if (fd < 0) {
        goto error;
    }
    if (!RemovePartitions(*disk, partitionChangedCounter)) {
        goto error;
    }
    BlockSync (*disk);
    if (!AddPartitions(*disk, ulist, partitionChangedCounter)) {
        goto error;
    }
    BlockSync (*disk);
    WriteDiskPartitionToMisc(nlist);
    BlockDiskClose(*disk);
    DestroyDiskPartitions(*disk);
    DestroyDiskDevices(*disk);
    free(disk);
    return partitionChangedCounter;
error:
    BlockDiskClose(*disk);
    DestroyDiskPartitions(*disk);
    DestroyDiskDevices(*disk);
    free(disk);
    return 0;
}
} // Updater

