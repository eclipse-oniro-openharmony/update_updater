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
#include "partition.h"

#include <cstdio>
#include <fcntl.h>
#include <map>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <linux/fs.h>
#include "flash_service.h"
#include "utils.h"

namespace Flashd {
Partition::~Partition()
{
    if (fd_ != -1) {
        close(fd_);
    }
}

int Partition::Load()
{
    std::vector<std::string> table;
    std::string info = ReadPartitionSysInfo(devName_, "partition", table);
    FLASHING_CHECK(!info.empty(), return -1, "Can not get partition for %s", devName_.c_str());
    partNumber_ = atoi(info.c_str());

    ReadPartitionSysInfo(devName_, "uevent", table);
    info = FlashService::GetParamFromTable(table, "DEVNAME=");
    FLASHING_CHECK(info == devName_, return -1, "Failed to check dev name %s %s", info.c_str(), devName_.c_str());

    partName_ = FlashService::GetParamFromTable(table, "PARTNAME=");
    if (partName_.empty()) {
        partName_ = devName_;
    }
    int ret = Partition::GetMountInfo();
    FLASHING_LOGI("Partition info devName \"%s\" partPath_ \"%s\" partName_ \"%s\"",
        devName_.c_str(), partPath_.c_str(), partName_.c_str());
    if (!mountPoint_.empty()) {
        FLASHING_LOGI("Partition mount info mount \"%s\" fs %s flags: %s",
            mountPoint_.c_str(), fsType_.c_str(), mountFlags_.c_str());
    }
    return ret;
}

int Partition::DoFlash(const std::string &fileName)
{
    int ret = Open();
    FLASHING_CHECK(ret == 0, return ret, "Can not open partiton %s for flash", partName_.c_str());

    auto inputFd = open(fileName.c_str(), O_RDONLY);
    FLASHING_CHECK(inputFd > 0,
        flash_->RecordMsg(Updater::ERROR, "Can not open image \"%s\" error: %s", fileName.c_str(), strerror(errno));
        return FLASHING_NOPERMISSION, "Can not open image \"%s\" error: %s", fileName.c_str(), strerror(errno));

    struct stat st {};
    size_t fileSize = 1;
    if (fstat(inputFd, &st) >= 0) {
        fileSize = st.st_size;
    }

    std::vector<uint8_t> content(BUFFER_SIZE);
    ssize_t readLen = read(inputFd, content.data(), content.size());
    FLASHING_CHECK(readLen >= 0, close(inputFd);
        return FLASHING_PART_WRITE_ERROR, "Failed to flash data of len %d", readLen);
    ret = WriteRowData(inputFd, fileSize, content, readLen);
    close(inputFd);
    FLASHING_CHECK(ret == 0, return ret, "Failed to write %s", fileName.c_str());
    FLASHING_LOGI("DoFlash partition %s image:%s success", partPath_.c_str(), fileName.c_str());
    return ret;
}

int Partition::DoErase()
{
    int ret = Open();
    FLASHING_CHECK(ret == 0, return ret, "Can not open partiton %s for erase", partName_.c_str());

    if (!IsBlockDevice(fd_)) {
        return 0;
    }

    uint64_t size = GetBlockDeviceSize(fd_);
    FLASHING_LOGI("DoErase partition %s size %luM", partPath_.c_str(), size);
    uint64_t range[2] = {0};
    range[1] = size;
    ret = ioctl(fd_, BLKSECDISCARD, &range);
    FLASHING_LOGI("DoErase partition %s ret %d", partPath_.c_str(), ret);
    if (ret < 0) {
        range[0] = 0;
        range[1] = size;
#ifndef UPDATER_UT
        ret = ioctl(fd_, BLKDISCARD, &range);
#endif
        FLASHING_CHECK(ret >= 0,
            flash_->RecordMsg(Updater::ERROR, "Failed to erase \"%s\" error: %s", partName_.c_str(), strerror(errno));
            return ret, "Failed to erase %s error: %s", partName_.c_str(), strerror(errno));
        std::vector<uint8_t> buffer(BLOCK_SIZE, 0);
#ifndef UPDATER_UT
        ret = Updater::Utils::WriteFully(fd_, buffer.data(), buffer.size());
#endif
        FLASHING_CHECK(ret == 0, return FLASHING_PART_WRITE_ERROR, "Failed to flash data errno %d", errno);
        fsync(fd_);
    }
    FLASHING_LOGI("DoErase partition %s erase success", partPath_.c_str());
    return 0;
}

int Partition::DoFormat(const std::string &fsType)
{
    int ret = DoUmount();
    FLASHING_CHECK(ret == 0, return FLASHING_PART_WRITE_ERROR, "Failed to umount partition");
    FLASHING_LOGI("DoFormat partition %s format %s", partName_.c_str(), fsType_.c_str());

    std::vector<std::string> formatCmds {};
    ret = BuildCommandParam(fsType, formatCmds);
    FLASHING_CHECK(ret == 0, return FLASHING_PART_WRITE_ERROR, "Failed to get param");
    ret = FlashService::ExecCommand(formatCmds);
    FLASHING_CHECK(ret == 0,
        flash_->RecordMsg(Updater::ERROR, "Failed to format \"%s\" error: %s", partName_.c_str(), strerror(ret));
        return FLASHING_PART_WRITE_ERROR, "Failed to format \"%s\" error: %s", partPath_.c_str(), strerror(ret));

    fsType_ = fsType;
    ret = DoMount();
    FLASHING_CHECK(ret == 0,
        flash_->RecordMsg(Updater::ERROR, "Failed to mount \"%s\" error: %s", partName_.c_str(), strerror(ret));
        return FLASHING_PART_WRITE_ERROR, "Failed to mount \"%s\"", partPath_.c_str());
    FLASHING_LOGI("DoFormat partition %s format %s success", partName_.c_str(), fsType_.c_str());
    return ret;
}

int Partition::DoResize(uint32_t blocks)
{
    int ret = 0;
    bool needResize = false;
    if (!mountPoint_.empty()) {
        needResize = FlashService::CheckFreeSpace(mountPoint_, blocks);
    }
    if (!needResize) {
        FLASHING_LOGI("No need to resize partition %s", partName_.c_str());
        return 0;
    }
    ret = DoUmount();
    FLASHING_CHECK(ret == 0, return FLASHING_PART_WRITE_ERROR, "Failed to umount partition");

    std::vector<std::string> formatCmds;
    formatCmds.push_back(RESIZE_TOOL);
    formatCmds.push_back(partPath_);
    ret = FlashService::ExecCommand(formatCmds);
    FLASHING_CHECK(ret == 0,
        flash_->RecordMsg(Updater::ERROR, "Failed to resize \"%s\" error: %s", partName_.c_str(), strerror(ret));
        return FLASHING_PART_WRITE_ERROR, "Failed to resize \"%s\" error: %s", partName_.c_str(), strerror(ret));
    ret = DoMount();
    FLASHING_CHECK(ret == 0,
        flash_->RecordMsg(Updater::ERROR, "Failed to mount \"%s\" error: %s", partName_.c_str(), strerror(ret));
        return FLASHING_PART_WRITE_ERROR, "Failed to mount \"%s\"", partPath_.c_str());
    FLASHING_LOGI("Resize partition %s success", partName_.c_str());
    return 0;
}

int Partition::Open()
{
    if (fd_ <= 0) {
        fd_ = open(partPath_.c_str(), O_RDWR);
    }
    FLASHING_CHECK(fd_ > 0,
        flash_->RecordMsg(Updater::ERROR,
        "Can not open partiton \"%s\" error: %s", partName_.c_str(), strerror(errno));
        return FLASHING_NOPERMISSION,
        "Can open partition %s error %s", partPath_.c_str(), strerror(errno));
    return 0;
}

int Partition::WriteRowData(int inputFd, size_t fileSize, std::vector<uint8_t> &buffer, size_t dataSize)
{
    size_t dataLen = dataSize;
    size_t totalWrite = 0;
    do {
#ifndef UPDATER_UT
        ssize_t writeLen = write(fd_, buffer.data(), dataLen);
#else
        ssize_t writeLen = dataLen;
#endif
        FLASHING_CHECK(writeLen >= 0,
            return FLASHING_PART_WRITE_ERROR, "Failed to write data of len %d", dataLen);
        totalWrite += writeLen;

        // continue read and write
        ssize_t ret = read(inputFd, buffer.data(), dataSize);
        FLASHING_CHECK(ret > 0, return -1, "Failed to read data %d %d", errno, buffer.size());
        flash_->PostProgress(UPDATEMOD_FLASH, writeLen, nullptr);
        dataLen = ret;
    } while (1);
    fsync(fd_);
    return 0;
}

int Partition::IsBlockDevice(int fd) const
{
    struct stat st {};
    int ret = fstat(fd, &st);
    FLASHING_CHECK(ret >= 0, return 0, "Invalid get fstate %d", errno);
    return S_ISBLK(st.st_mode);
}

uint64_t Partition::GetBlockDeviceSize(int fd) const
{
    uint64_t size = 0;
    int ret = ioctl(fd, BLKGETSIZE64, &size);
    return (ret == 0) ? size : 0;
}

std::string Partition::ReadPartitionSysInfo(const std::string &partition,
    const std::string &type, std::vector<std::string> &table)
{
    std::vector<char> buffer(DEVICE_PATH_SIZE, 0);
    int ret = snprintf_s(buffer.data(), DEVICE_PATH_SIZE, DEVICE_PATH_SIZE - 1,
        "/sys/block/%s/%s/%s", device_->GetDeviceName().c_str(), partition.c_str(), type.c_str());
    FLASHING_CHECK(ret != -1, return "", "Failed to snprintf_s %s", device_->GetDeviceName().c_str());
    return FlashService::ReadSysInfo(buffer.data(), type, table);
}

const std::string Partition::GetPartitionName() const
{
    return partName_;
}

int Partition::GetMountInfo()
{
    auto file = std::unique_ptr<FILE, decltype(&fclose)>(fopen("/proc/mounts", "r"), fclose);
    FLASHING_CHECK(file != nullptr, return -1, "Failed to open mounts ");
    std::vector<char> buffer(LINE_BUFFER_SIZE, 0);
    std::vector<char> mount(DEVICE_PATH_SIZE, 0);
    std::vector<char> fsType(DEVICE_PATH_SIZE, 0);
    std::vector<char> dev(DEVICE_PATH_SIZE, 0);
    std::vector<char> flags(DEVICE_PATH_SIZE, 0);
    while (fgets(buffer.data(), LINE_BUFFER_SIZE, file.get()) != nullptr) {
        // clang-format off
        int ret = sscanf_s(buffer.data(), "%255s %255s %255s %255s %*d %*d\n",
            dev.data(), DEVICE_PATH_SIZE - 1,
            mount.data(), DEVICE_PATH_SIZE - 1,
            fsType.data(), DEVICE_PATH_SIZE - 1,
            flags.data(), DEVICE_PATH_SIZE - 1);
        // clang-format on
        if (ret <= 0) {
            break;
        }
        struct stat st {};
        if (lstat(dev.data(), &st) < 0) {
            continue;
        }
        if (S_ISLNK(st.st_mode)) {
            readlink(dev.data(), dev.data(), DEVICE_PATH_SIZE);
        }
        if (strncmp(dev.data(), partPath_.c_str(), partPath_.size()) == 0 ||
            (partName_ == "system" && strncmp(mount.data(), "/", strlen("/")) == 0)) {
            mountPoint_.assign(mount.data());
            fsType_.assign(fsType.data());
            mountFlags_.assign(flags.data());
            break;
        }
        memset_s(buffer.data(), LINE_BUFFER_SIZE, 0, LINE_BUFFER_SIZE);
    }
    return 0;
}

int Partition::DoUmount()
{
    if (mountPoint_.empty()) {
        return 0;
    }
#ifndef UPDATER_UT
    int ret = umount2(mountPoint_.c_str(), MNT_FORCE);
    FLASHING_CHECK(ret == 0,
        flash_->RecordMsg(Updater::ERROR, "Failed to umount \"%s\" error: %s", partName_.c_str(), strerror(errno));
        return FLASHING_PART_WRITE_ERROR, "Failed to umount \"%s\" error: %s", partName_.c_str(), strerror(errno));
#endif
    return 0;
}

int Partition::DoMount()
{
    if (mountPoint_.empty()) {
        return 0;
    }
    struct stat st {};
    int ret = lstat(mountPoint_.c_str(), &st);
    FLASHING_CHECK(ret >= 0, return ret, "Failed to fstat %s error %s", mountPoint_.c_str(), strerror(errno));

    if (S_ISLNK(st.st_mode)) {
        unlink(mountPoint_.c_str());
    }
    mkdir(mountPoint_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // get mount flags
    std::string data;
    uint32_t flags = GetMountFlags(mountFlags_, data);
    errno = 0;

    while ((ret = mount(partPath_.c_str(), mountPoint_.c_str(), fsType_.c_str(), flags, data.c_str()) != 0)) {
#ifdef UPDATER_UT
        ret = 0;
#endif
        if (errno == EAGAIN) {
            continue;
        } else {
            break;
        }
    }
    return ret;
}

int Partition::BuildCommandParam(const std::string &fsType, std::vector<std::string> &formatCmds) const
{
    std::map<std::string, std::string> fsToolsMap = {
        { "ext4", FORMAT_TOOL_FOR_EXT4 },
        { "f2fs", FORMAT_TOOL_FOR_F2FS },
    };
    auto it = fsToolsMap.find(fsType);
    FLASHING_CHECK(it != fsToolsMap.end(),
        flash_->RecordMsg(Updater::ERROR,  "Not support fs type %s", fsType.c_str());
        return FLASHING_FSTYPE_NOT_SUPPORT, "Not support fs type %s", fsType.c_str());

    if (fsType == "ext4") {
        formatCmds.push_back(it->second);
        formatCmds.push_back("-F");
        formatCmds.push_back("-t");
        formatCmds.push_back(fsType);
        formatCmds.push_back("-b");
        formatCmds.push_back(std::to_string(DEFAULT_BLOCK_SIZE));
        formatCmds.push_back(partPath_);
    } else if (fsType == "f2fs") {
        formatCmds.push_back(it->second);
        formatCmds.push_back(partPath_);
    }
    return 0;
}

bool Partition::IsOnlyErase() const
{
    std::vector<std::string> rawPartName = {
        "boot", "fastboot", "kernel", "misc", "system"
    };
    auto it = std::find (rawPartName.begin(), rawPartName.end(), partName_);
    return it != rawPartName.end();
}

uint32_t Partition::GetMountFlags(const std::string &mountFlagsStr, std::string &data) const
{
    static std::map<std::string, uint32_t> mntInfo = {
        { "ro", MS_RDONLY },
        { "rw", 0 },
        { ",sync", MS_SYNCHRONOUS },
        { ",dirsync", MS_DIRSYNC },
        { ",mand", MS_MANDLOCK },
        { ",lazytime", MS_LAZYTIME },
        { ",nosuid", MS_NOSUID },
        { ",nodev", MS_NODEV },
        { ",noexec", MS_NOEXEC },
        { ",noatime", MS_NOATIME },
        { ",nodiratime", MS_NODIRATIME },
        { ",relatime", MS_RELATIME },
        { ",remount", MS_REMOUNT },
        { ",bind", MS_BIND },
        { ",rec", MS_REC },
        { ",unbindable", MS_UNBINDABLE },
        { ",private", MS_PRIVATE },
        { ",slave", MS_SLAVE },
        { ",shared", MS_SHARED },
        { ",defaults", 0 },
    };

    // get rw flags
    uint32_t flags = 0;
    std::string::size_type found = std::string::npos;
    std::string::size_type start = 0;
    while (true) {
        found = mountFlagsStr.find_first_of(",", start + 1);
        std::string option = mountFlagsStr.substr(start, found - start);
        auto iter = mntInfo.find(option);
        if (iter != mntInfo.end()) {
            flags |= iter->second;
        } else {
            data.append(option);
            data.append(",");
        }
        if (found == std::string::npos) {
            break;
        }
        start = found;
    }
    data.pop_back(); // Remove last ','
    return flags;
}
}
