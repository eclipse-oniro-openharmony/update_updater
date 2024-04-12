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
#include "fs_manager/partitions.h"
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include "log/log.h"
#include "partition_const.h"
#include "scope_guard.h"
#include "securec.h"

namespace Updater {
static struct Disk *g_disks;
static int DeviceStat(const BlockDevice &dev, struct stat &devStat)
{
    int ret = 0;
    if (!stat (dev.devPath.c_str(), &devStat)) {
        ret = 1;
    }
    if (stat (dev.devPath.c_str(), &devStat) != EOK) {
        LOG(ERROR) << "stat error: " << errno << std::endl;
        ret = 0;
    }
    return ret;
}

static int DeviceProbeType(BlockDevice &dev)
{
    struct stat devStat {};
    int devMajor;
    int devMinor;
    BlockSpecific *specific = BLOCK_SPECIFIC(&dev);
    if (DeviceStat(dev, devStat) == 0) {
        return 0;
    }

    devMajor = static_cast<int>(major (devStat.st_rdev));
    specific->major = devMajor;
    devMinor = static_cast<int>(minor (devStat.st_rdev));
    specific->minor = devMinor;
    bool a1 = SCSI_BLK_MAJOR(devMajor) && (devMinor % 0x10 == 0);
    bool a2 = devMajor == SDMMC_MAJOR && (devMinor % 0x08 == 0);
    if (a1) {
        dev.type = DEVICE_SCSI;
    }
    if (a2) {
        dev.type = DEVICE_EMMC;
    }
    if (!a1 && !a2) {
        dev.type = DEVICE_UNKNOWN;
    }
    return 1;
}

static std::string LastComponent(const std::string &path)
{
    std::string tmp = "";
    if (path == MMC_PATH) {
        tmp = MMC_DEV;
    }
    if (path == SDA_PATH) {
        tmp = SDA_DEV;
    }
    if (path == SDB_PATH) {
        tmp = SDB_DEV;
    }
    return tmp;
}

static bool ReadDeviceSysfsFile(BlockDevice &dev, const std::string &file, std::string &strl)
{
    FILE *f = nullptr;
    char nameBuf[DEVPATH_SIZE];
    char buf[BUFFER_SIZE];

    if (snprintf_s(nameBuf, DEVPATH_SIZE, DEVPATH_SIZE - 1, "/sys/block/%s/device/%s",
        LastComponent(dev.devPath).c_str(), file.c_str()) == -1) {
        return false;
    }
    char *realPath = realpath(nameBuf, nullptr);
    if (realPath == nullptr) {
        return false;
    }
    if ((f = fopen(realPath, "r")) == nullptr) {
        free(realPath);
        return false;
    }

    if (fgets(buf, BUFFER_SIZE, f) == nullptr) {
        fclose(f);
        free(realPath);
        return false;
    }
    strl = buf;
    fclose(f);
    free(realPath);
    return true;
}

static bool SdmmcGetProductInfo(BlockDevice &dev, std::string &type, std::string &name)
{
    std::string typeStr = "type";
    std::string nameStr = "name";

    bool ret = ReadDeviceSysfsFile(dev, typeStr, type);
    bool red = ReadDeviceSysfsFile(dev, nameStr, name);
    return (ret || red);
}

bool SetBlockDeviceMode(BlockDevice &dev)
{
    BlockSpecific *specific = BLOCK_SPECIFIC(&dev);

    specific->fd = open(dev.devPath.c_str(), RW_MODE);
    if (specific->fd == -1) {
        LOG(WARNING) << "Open " << dev.devPath << " with read-write failed, try read-only mode";
        specific->fd = open(dev.devPath.c_str(), RD_MODE);
        bool a1 = dev.readOnly;
        dev.readOnly = 1;
        if (specific->fd == -1) {
            LOG(ERROR) << "Open " << dev.devPath << " with read-only mode failed: " << errno;
            dev.readOnly = a1;
            return false;
        }
    } else {
        dev.readOnly = 0;
    }
    return true;
}

static int BlockDeviceClose(const BlockDevice &dev)
{
    BlockSpecific* specific = BLOCK_SPECIFIC(&dev);
    if (fsync(specific->fd) < 0 || close(specific->fd) < 0) {
        return 0;
    }
    return 1;
}

static std::string ReadPartitionFromSys(const std::string &devname, const std::string &partn,
    const std::string &type, const std::string &table)
{
    FILE *f = nullptr;
    char buf[BUFFER_SIZE] = {0};
    std::string devPath;
    std::string partString = "";
    devPath = "/sys/block/" + devname + "/" + partn + "/" + type;
    if (partn.empty()) {
        devPath = "/sys/block/" + devname + "/" + type;
    }

    if (devPath.length() >= DEVPATH_SIZE) {
        LOG(ERROR) << "devPath is invalid";
        return partString;
    }

    if ((f = fopen(devPath.c_str(), "r")) == nullptr) {
        return partString;
    }

    ON_SCOPE_EXIT(fclosef) {
        fclose(f);
    };

    while (!feof(f)) {
        if (fgets(buf, BUFFER_SIZE, f) == nullptr) {
            return partString;
        }
        if (type == "uevent") {
            if (strstr(buf, table.c_str()) != nullptr) {
                partString = std::string(buf + table.size(), sizeof(buf) - table.size());
                if (!partString.empty()) {
                    partString.pop_back();
                }
                return partString;
            }
        } else if (type == "start" || type == "size") {
            partString = std::string(buf, sizeof(buf) - 1);
            LOG(INFO) << type << " partInf: " << std::string(buf, sizeof(buf) - 1);
            return partString;
        }
        if (memset_s(buf, sizeof(buf), 0, sizeof(buf)) != EOK) {
            return partString;
        }
    }

    return partString;
}

static int InitGeneric(BlockDevice &dev, const std::string modelName)
{
    struct stat devStat {};
    if (DeviceStat(dev, devStat) == 0) {
        LOG(ERROR) << "device stat error ";
        return 0;
    }
    if (!SetBlockDeviceMode(dev)) {
        LOG(ERROR) << "device authority error ";
        return 0;
    }

    const std::string devName = LastComponent(dev.devPath);
    std::string partSize = ReadPartitionFromSys(devName, "", "size", "");
    if (partSize.empty()) {
        return 0;
    }
    int devSize = atoi(partSize.c_str());
    dev.length = devSize;
    dev.sectorSize = SECTOR_SIZE_DEFAULT;
    dev.physSectorSize = SECTOR_SIZE_DEFAULT;
    dev.model = modelName;
    BlockDeviceClose (dev);
    dev.fd = -1;
    return 1;
}

static int InitSdmmc(BlockDevice &dev)
{
    std::string type = "";
    std::string name = "";
    std::string id = "";
    bool a1 = SdmmcGetProductInfo(dev, type, name);
    if (a1) {
        id = type + name;
    }
    if (!a1) {
        id = "Generic SD/MMC Storage Card";
        return 0;
    }
    return InitGeneric(dev, id);
}

static BlockDevice* NewBlockDevice(const std::string &path)
{
    BlockDevice *dev = nullptr;
    BlockSpecific *specific = nullptr;

    dev = static_cast<BlockDevice*>(calloc(1, sizeof (BlockDevice)));
    if (dev == nullptr) {
        LOG(ERROR) << "calloc errno " << errno;
        return nullptr;
    }

    dev->devPath = path;
    dev->specific = static_cast<BlockSpecific*>(calloc(1, sizeof (BlockSpecific)));
    if (!dev->specific) {
        LOG(ERROR) << "calloc errno " << errno;
        free(dev);
        return nullptr;
    }

    specific = BLOCK_SPECIFIC(dev);
    dev->readOnly = 0;
    dev->sectorSize = 0;
    dev->physSectorSize = 0;

    int ret = 0;
    bool a1 = DeviceProbeType(*dev);
    if (a1) {
        if (dev->type == DEVICE_EMMC)  {
            ret = InitSdmmc(*dev);
            if (ret == 0) {
                LOG(ERROR) << "Init sdmmc error";
            }
        }
        if (dev->type != DEVICE_EMMC) {
            LOG(WARNING) << "Unsupported device type";
        }
    }
    if (!a1) {
        LOG(ERROR) << "Device probe error";
    }

    if (ret == 0) {
        free(dev->specific);
        free(dev);
        dev = nullptr;
    }
    return dev;
}

static Disk* NewBlockDisk(const BlockDevice &dev, const DiskType diskType)
{
    Disk *disk = nullptr;

    disk = static_cast<Disk*>(calloc (1, sizeof (Disk)));
    if (disk == nullptr) {
        LOG(ERROR) << "Allocate memory for disk failed: " << errno;
        return nullptr;
    }

    disk->dev = (BlockDevice*)&dev;
    disk->type = diskType;
    disk->partsum = 0;
    disk->partList.clear();
    return disk;
}

int DiskAlloc(const std::string &path)
{
    struct Disk *disk = nullptr;
    struct BlockDevice *dev = nullptr;
    dev = NewBlockDevice(path);
    if (dev == nullptr) {
        LOG(ERROR) << "NewBlockDevice nullptr ";
        return 0;
    }

    disk = NewBlockDisk(*dev, GPT);
    if (disk == nullptr) {
        LOG(ERROR) << "NewBlockDevice nullptr ";
        return 0;
    }
    g_disks = disk;
    return 1;
}

static struct Partition* NewPartition(const BlockDevice &dev, int partn)
{
    Partition* part = (Partition*) calloc (1, sizeof (Partition));
    if (part == nullptr) {
        LOG(ERROR) << "Allocate memory for partition failed.";
        return nullptr;
    }
    const std::string devName = LastComponent(dev.devPath);
    char partName[64] = {0};
    if (devName == MMC_DEV) {
        if (snprintf_s(partName, sizeof(partName), sizeof(partName) - 1, "%sp%d", devName.c_str(), partn) == -1) {
            free(part);
            return nullptr;
        }
    }
    if (devName != MMC_DEV && ((devName == SDA_DEV) || (devName == SDB_DEV)) &&
        snprintf_s(partName, sizeof(partName), sizeof(partName) - 1, "%s%d", devName.c_str(), partn) == -1) {
        free(part);
        return nullptr;
    }

    std::string strstart = ReadPartitionFromSys(devName, partName, "start", "");
    if (strstart.empty()) {
        free(part);
        return nullptr;
    }
    part->start = static_cast<size_t>(atoi(strstart.c_str()));

    std::string strsize = ReadPartitionFromSys(devName, partName, "size", "");
    if (strsize.empty()) {
        free(part);
        return nullptr;
    }
    part->length = static_cast<size_t>(atoi(strsize.c_str()));

    std::string strdevname = ReadPartitionFromSys(devName, partName, "uevent", "DEVNAME=");
    part->devName = partName;
    if (!strdevname.empty()) {
        part->devName = strdevname;
    }
    std::string strpartname = ReadPartitionFromSys(devName, partName, "uevent", "PARTNAME=");
    part->partName = partName;
    if (!strpartname.empty()) {
        part->partName = strpartname;
    }

    part->partNum = partn;
    part->type = NORMAL;
    part->fsType = "";
    part->changeType = NORMAL_CHANGE;
    return part;
}

struct Partition* GetPartition(const Disk &disk, int partn)
{
    struct Partition *part = nullptr;
    if (partn == 0) {
        return nullptr;
    }
    if (disk.partList.empty()) {
        return nullptr;
    }
    for (auto& p : disk.partList) {
        if (p->partNum == partn) {
            part = p;
            break;
        }
    }
    return part;
}

int ProbeAllPartitions()
{
    int i = 0;
    struct Disk* disk = nullptr;
    disk = g_disks;
    if (disk == nullptr) {
        return 0;
    }
    int partSum = DEFAULT_PARTSUM;
    struct Partition* part = nullptr;
    for (i = 1; i < partSum; i++) {
        part = NewPartition(*(disk->dev), i);
        if (!part) {
            LOG(ERROR) << "Create new partition failed.";
            break;
        }
        disk->partList.push_back(part);
        disk->partsum++;
    }
    return disk->partsum;
}

Disk* GetRegisterBlockDisk(const std::string &path)
{
    if (g_disks == nullptr) {
        return nullptr;
    }
    Disk *p = nullptr;
    if (g_disks->dev->devPath == path) {
        p = g_disks;
    }
    return p;
}

int GetPartitionNumByPartName(const std::string &partname, const PartitonList &plist)
{
    int ret = 0;
    for (const auto &p : plist) {
        if (p->partName == partname) {
            ret = p->partNum;
            break;
        }
    }
    return ret;
}
} // namespace Updater
