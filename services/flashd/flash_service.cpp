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
#include "flash_service.h"
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "blockdevice.h"
#include "fs_manager/mount.h"
#include "package/pkg_manager.h"
#include "securec.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
#include "utils.h"

using namespace hpackage;
using namespace updater;

namespace flashd {
static std::atomic<bool> g_flashdRunning { false };
FlashService::~FlashService()
{
    for (auto part : partitions_) {
        printf("LoadBlockDevice %s \n", part->GetPartitionName().c_str());
        delete part;
    }
    partitions_.clear();
    for (auto dev : blockDevices_) {
        delete dev;
    }
    blockDevices_.clear();
}

int FlashService::LoadSysDevice()
{
    if (loadSysDevice_) {
        return 0;
    }
    return LoadBlockDevice("/dev/block");
}

int FlashService::DoUpdate(const std::string &packageName)
{
    int ret = 0;
    FLASHING_LOGI("DoUpdate packageName %s", packageName.c_str());
    FLASHING_CHECK(access(packageName.c_str(), 0) == 0,
        return FLASHING_IMAGE_INVALID, "Invalid package %s for update", packageName.c_str());
    ret = updater::IsSpaceCapacitySufficient(packageName);
    if (ret == UPDATE_SPACE_NOTENOUGH) {
        RecordMsg(updater::ERROR, "Free space is not enough");
        return FLASHING_SPACE_NOTENOUGH;
    }
    FLASHING_LOGI("Check space for packageName %s success", packageName.c_str());

    uint64_t pkgLen = 0;
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    FLASHING_CHECK(pkgManager != nullptr, return FLASHING_PACKAGE_INVALID, "Failed to GetPackageInstance");
    pkgManager->SetPkgDecodeProgress([&](int type, size_t writeDataLen, const void *context) {
        pkgLen += writeDataLen;
        PostProgress(UPDATEMOD_UPDATE, writeDataLen, context);
    });
    std::vector<std::string> components;
    ret = pkgManager->LoadPackage(packageName, utils::GetCertName(), components);
    FLASHING_CHECK(ret == PKG_SUCCESS, PkgManager::ReleasePackageInstance(pkgManager);
        RecordMsg(updater::ERROR, "Can not load package %s", packageName.c_str());
        return FLASHING_PACKAGE_INVALID, "Failed to load package %s", packageName.c_str());

    ret = UpdatePreProcess(pkgManager, packageName);
    FLASHING_CHECK(ret == PKG_SUCCESS, PkgManager::ReleasePackageInstance(pkgManager);
        RecordMsg(updater::ERROR, "Invalid package %s", packageName.c_str());
        return FLASHING_PACKAGE_INVALID, "Invalid package %s", packageName.c_str());
#ifndef UPDATER_UT
    ret = updater::ExecUpdate(pkgManager, 0,
        [&](const char *cmd, const char *content) {
            if (strncmp(cmd, "data", strlen(cmd)) == 0) {
                size_t dataLen = std::stoll(content);
                PostProgress(UPDATEMOD_UPDATE, dataLen, nullptr);
            }
        });
#endif
    FLASHING_CHECK(ret == PKG_SUCCESS, PkgManager::ReleasePackageInstance(pkgManager);
        RecordMsg(updater::ERROR, "Failed to update package %s", packageName.c_str());
        return FLASHING_PACKAGE_INVALID, "Failed to update package %s", packageName.c_str());
    FLASHING_LOGI("Load packageName %s success %llu", packageName.c_str(), pkgLen);
    PkgManager::ReleasePackageInstance(pkgManager);
    return ret;
}

int FlashService::DoFlashPartition(const std::string &fileName, const std::string &partition)
{
    int ret = CheckOperationPermission(flashd::UPDATEMOD_FLASH, partition);
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Forbit to flash partition %s", partition.c_str());
        return FLASHING_NOPERMISSION, "Forbit to flash partition %s", partition.c_str());

    ret = LoadSysDevice();
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Can not read device information");
        return FLASHING_PART_NOEXIST, "Failed to load partition");

    FLASHING_LOGI("DoFlashPartition partition %s image:%s", partition.c_str(), fileName.c_str());
    PartitionPtr part = GetPartition(partition);
    FLASHING_CHECK(part != nullptr,
        RecordMsg(updater::ERROR, "Can not find partition %s", partition.c_str());
        return FLASHING_PART_NOEXIST, "Failed to get partition %s", partition.c_str());
    return part->DoFlash(fileName);
}

int FlashService::GetPartitionPath(const std::string &partition, std::string &paratitionPath)
{
    int ret = CheckOperationPermission(flashd::UPDATEMOD_FLASH, partition);
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Forbit to flash partition %s", partition.c_str());
        return FLASHING_NOPERMISSION, "Forbit to flash partition %s", partition.c_str());

    ret = LoadSysDevice();
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Can not read device information");
        return FLASHING_PART_NOEXIST, "Failed to load partition");

    FLASHING_LOGI("DoFlashPartition partition %s", partition.c_str());
    PartitionPtr part = GetPartition(partition);
    FLASHING_CHECK(part != nullptr,
        RecordMsg(updater::ERROR, "Can not find partition %s", partition.c_str());
        return FLASHING_PART_NOEXIST, "Failed to get partition %s", partition.c_str());
    paratitionPath = part->GetPartitionPath();
    return 0;
}

int FlashService::DoErasePartition(const std::string &partition)
{
    int ret = CheckOperationPermission(flashd::UPDATEMOD_ERASE, partition);
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Forbit to erase partition %s", partition.c_str());
        return FLASHING_NOPERMISSION, "Forbit to erase partition %s", partition.c_str());

    ret = LoadSysDevice();
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Can not read device information");
        return FLASHING_PART_NOEXIST, "Failed to load partition");

    FLASHING_LOGI("DoErasePartition partition %s ", partition.c_str());
    PartitionPtr part = GetPartition(partition);
    FLASHING_CHECK(part != nullptr,
        RecordMsg(updater::ERROR, "Can not find partition %s", partition.c_str());
        return FLASHING_PART_NOEXIST, "Failed to get partition %s", partition.c_str());
    return part->DoErase();
}

int FlashService::DoFormatPartition(const std::string &partition, const std::string &fsType)
{
    int ret = CheckOperationPermission(flashd::UPDATEMOD_FORMAT, partition);
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Forbit to format partition %s", partition.c_str());
        return FLASHING_NOPERMISSION, "Forbit to format partition %s", partition.c_str());

    ret = LoadSysDevice();
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Can not read device information");
        return FLASHING_PART_NOEXIST, "Failed to load partition");
    PartitionPtr part = GetPartition(partition);
    FLASHING_CHECK(part != nullptr,
        RecordMsg(updater::ERROR, "Can not find partition %s", partition.c_str());
        return FLASHING_PART_NOEXIST, "Failed to get partition %s", partition.c_str());
    if (part->IsOnlyErase()) {
        FLASHING_LOGI("DoFormatPartition format partition %s", partition.c_str());
        return part->DoErase();
    }
    FLASHING_LOGI("DoFormatPartition partition %s fsType:%s", partition.c_str(), fsType.c_str());
    return part->DoFormat(fsType);
}

int FlashService::DoResizeParatiton(const std::string &partition, uint32_t blocks)
{
    int ret = CheckOperationPermission(flashd::UPDATEMOD_UPDATE, partition);
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Forbit to resize partition %s", partition.c_str());
        return FLASHING_NOPERMISSION, "Forbit to resize partition %s", partition.c_str());

    ret = LoadSysDevice();
    FLASHING_CHECK(ret == 0,
        RecordMsg(updater::ERROR, "Can not read device information");
        return FLASHING_PART_NOEXIST, "Failed to load partition");

    PartitionPtr part = GetPartition(partition);
    FLASHING_CHECK(part != nullptr,
        RecordMsg(updater::ERROR, "Can not find partition %s", partition.c_str());
        return FLASHING_PART_NOEXIST, "Failed to get partition %s", partition.c_str());
    return part->DoResize(blocks);
}

PartitionPtr FlashService::GetPartition(const std::string &partition) const
{
    const std::string partName = GetPartNameByAlias(partition);
    for (auto part : partitions_) {
        if (strcmp(partName.c_str(), part->GetPartitionName().c_str()) == 0) {
            return part;
        }
    }
    return nullptr;
}

int FlashService::LoadBlockDevice(const std::string &fileDir)
{
    std::vector<std::string> partitionsName {};
    struct stat dirStat = {};
    int ret = stat(fileDir.c_str(), &dirStat);
    FLASHING_CHECK(ret != -1 && S_ISDIR(dirStat.st_mode), return -1, "Invlid dir %s", fileDir.c_str());

    std::vector<char> buffer(DEVICE_PATH_SIZE, 0);
    struct dirent *entry = nullptr;
    DIR *dir = opendir(fileDir.c_str());
    while ((entry = readdir(dir)) != nullptr) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        std::string devPath = fileDir + "/" + entry->d_name;
        if (entry->d_type == 10) { // 10 link file
            readlink(devPath.c_str(), buffer.data(), DEVICE_PATH_SIZE);
            devPath = fileDir + "/" + buffer.data();
            memset_s(buffer.data(), DEVICE_PATH_SIZE, 0, DEVICE_PATH_SIZE);
        }
        ret = stat(devPath.c_str(), &dirStat);
        FLASHING_CHECK(ret != -1, break, "Invlid dir %s %s", devPath.c_str(), strerror(errno));
        uint32_t devMajor = major(dirStat.st_rdev);
        uint32_t devMinor = minor(dirStat.st_rdev);
        if (devMajor == LOOP_MAJOR) {
            continue;
        }
        ret = 0;
        if (SCSI_BLK_MAJOR(devMajor)) {
            if ((devMinor % 0x10) == 0) { // 0x10 scsi device
                ret = AddNewBlockDevice(DeviceType::DEVICE_SCSI, devPath);
            } else {
                partitionsName.push_back(devPath);
            }
        } else if (devMajor == SDMMC_MAJOR) {
            if (devMinor % 0x08 == 0) { // 0x08 emmc
                ret = AddNewBlockDevice(DeviceType::DEVICE_EMMC, devPath);
            } else {
                partitionsName.push_back(devPath);
            }
        }
        FLASHING_CHECK(ret == 0, break, "Failed to add device %s", devPath.c_str());
    }
    closedir(dir);
    FLASHING_CHECK(ret == 0, return -1, "Failed to add device %s", fileDir.c_str());
    ret = LoadPartition(partitionsName);
    loadSysDevice_ = true;
    return ret;
}

int FlashService::LoadPartition(std::vector<std::string> &partitionsNames)
{
    std::sort(std::begin(partitionsNames), std::end(partitionsNames), std::less<std::string>());
    for (auto device : blockDevices_) {
        for (std::string path : partitionsNames) {
            std::string name = GetBaseName(path);
            if (strncmp(name.c_str(), device->GetDeviceName().c_str(), device->GetDeviceName().size()) != 0) {
                continue;
            }
            AddNewPartition(path, device);
        }
    }
    return 0;
}

int FlashService::AddNewPartition(const std::string &path, BlockDevicePtr device)
{
    std::string name = GetBaseName(path);
    PartitionPtr part = new Partition(name, GetRealPath(path), device, this);
    FLASHING_CHECK(part != nullptr, return -1, "Failed to create partition %s", path.c_str());
    int ret = part->Load();
    FLASHING_CHECK(ret == 0, delete part;
        return -1, "Failed to create partition %s", path.c_str());
    partitions_.push_back(part);
    return 0;
}

int FlashService::AddNewBlockDevice(DeviceType type, const std::string &devPath)
{
    BlockDevicePtr device = new BlockDevice(type, GetRealPath(devPath));
    FLASHING_CHECK(device != nullptr, return -1, "Failed to create device %s", devPath.c_str());
    int ret = device->Load();
    FLASHING_CHECK(ret == 0, delete device;
        return 0, "Failed to create device %s", devPath.c_str());
    blockDevices_.push_back(device);
    return 0;
}

std::string FlashService::ReadSysInfo(const std::string &path, const std::string &type, std::vector<std::string> &table)
{
    std::string returnStr;
    auto file = std::unique_ptr<FILE, decltype(&fclose)>(fopen(path.c_str(), "r"), fclose);
    FLASHING_CHECK(file != nullptr, return "", "Failed to open %s", path.c_str());
    std::vector<char> buffer(LINE_BUFFER_SIZE, 0);
    while (fgets(buffer.data(), LINE_BUFFER_SIZE, file.get()) != nullptr) {
        if (type == "uevent") {
            table.push_back(buffer.data());
        } else if (type == "start") {
            returnStr = std::string(buffer.data());
            break;
        } else if (type == "size") {
            returnStr = std::string(buffer.data());
            break;
        } else if (type == "partition") {
            returnStr = std::string(buffer.data());
            break;
        }
        memset_s(buffer.data(), LINE_BUFFER_SIZE, 0, LINE_BUFFER_SIZE);
    }
    return returnStr;
}

std::string FlashService::GetParamFromTable(const std::vector<std::string> &table, const std::string &param)
{
    for (std::string line : table) {
        std::string::size_type pos = line.find(param);
        if (pos != std::string::npos) {
            return line.substr(param.size(), line.size() - 1 - param.size());
        }
    }
    return "";
}

int FlashService::ExecCommand(const std::vector<std::string> &cmds)
{
    std::vector<char *> extractedCmds;
    for (const auto &cmd : cmds) {
        extractedCmds.push_back(const_cast<char *>(cmd.c_str()));
    }
    extractedCmds.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
#ifndef UPDATER_UT
        execv(extractedCmds[0], extractedCmds.data());
#endif
        exit(0x7f); // 0x7f exit code
    }
    FLASHING_CHECK(pid > 0, return -1, "Failed to fork %d error:%d", pid, errno);
#ifndef UPDATER_UT
    int status;
    waitpid(pid, &status, 0);
    if (WEXITSTATUS(status) != 0 || !WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
#endif
    return 0;
}

void FlashService::RecordMsg(uint8_t level, const char *msg, ...)
{
    errorLevel_ = level;
    va_list vaArgs;
    va_start(vaArgs, msg);
    std::vector<char> zc(MAX_SIZE_BUF);
    const int retSize = vsnprintf_s(zc.data(), MAX_SIZE_BUF, zc.size() - 1, msg, vaArgs);
    if (retSize >= 0) {
        errorMsg_ = std::string(zc.data(), retSize);
    }
    va_end(vaArgs);
}

void FlashService::PostProgress(uint32_t type, size_t dataLen, const void *context) const
{
    if (progressor_ != nullptr) {
        progressor_(type, dataLen, context);
    }
}

int FlashService::CheckOperationPermission(int type, const std::string &partition) const
{
    FLASHING_CHECK(type < UPDATEMOD_MAX, return 1, "Invalid type %d", type);
    std::vector<std::string> forbitPartName[] = {
        {}, // updater
        {"updater"}, // flash
        {"updater", "boot", "kernel"}, // erase
        {"updater", "boot", "kernel"} // format
    };
    if (forbitPartName[type].size() == 0) {
        return 0;
    }
    const std::string partName = GetPartNameByAlias(partition);
    auto it = std::find (forbitPartName[type].begin(), forbitPartName[type].end(), partName);
    return it != forbitPartName[type].end();
}

const std::string FlashService::GetBaseName(const std::string &path)
{
    std::string::size_type pos = path.find_last_of('/') + 1;
    return path.substr(pos, path.size() - pos);
}

const std::string FlashService::GetPathRoot(const std::string &path)
{
    std::string::size_type pos = path.find_first_of('/', 1);
    return path.substr(0, pos);
}

const std::string FlashService::GetPartNameByAlias(const std::string &alias)
{
    std::map<std::string, std::vector<std::string>> partNameMap = {
        { "userdata", { "data", "/data", "userdata" } },
        { "system", { "system", "/system" } },
        { "vendor", { "vendor", "/vendor" } },
        { "misc", { "misc", "/misc" } },
        { "updater", { "updater", "/updater" } },
        { "kernel", { "boot" } }, // image
        { "boot", { "fastboot" } }, // fastboot
    };
    for (auto iter = partNameMap.begin(); iter != partNameMap.end(); iter++) {
        for (auto iter2 = iter->second.begin(); iter2 != iter->second.end(); iter2++) {
            if (strcmp(alias.c_str(), (*iter2).c_str()) == 0) {
                return iter->first;
            }
        }
    }
    return alias;
}

const std::string FlashService::GetRealPath(const std::string &path)
{
    std::vector<char> realPath(DEVICE_PATH_SIZE, 0);
    return realpath(path.c_str(), realPath.data());
}

bool FlashService::CheckFreeSpace(const std::string &root, uint32_t blocks)
{
    struct statvfs64 vfs {};
    int ret = statvfs64(root.c_str(), &vfs);
    FLASHING_CHECK(ret >= 0, return false, "Failed to statvfs %s", root.c_str());
    if (static_cast<uint32_t>(vfs.f_bsize) == DEFAULT_BLOCK_SIZE) {
        return static_cast<uint32_t>(vfs.f_bfree) < static_cast<uint32_t>(blocks);
    }
    return static_cast<uint32_t>(vfs.f_bfree) < (blocks / static_cast<uint32_t>(vfs.f_bsize)) * DEFAULT_BLOCK_SIZE;
}

static std::string GetValueFromParam(const std::vector<std::string> &params,
    const std::string &paramType, const std::string &defValue)
{
    std::string ret = defValue;
    for (size_t i = 0; i < params.size(); i++) {
        if (strcmp(paramType.c_str(), params[i].c_str()) == 0) {
            if (i < (params.size() - 1)) {
                ret = params[i + 1];
            } else {
                ret = "true";
            }
        }
    }
    return ret;
}

static bool FilterParam(const std::string &param, const std::vector<std::string> &filter)
{
    auto iter = filter.begin();
    while (iter != filter.end()) {
        if (strcmp(param.c_str(), (*iter).c_str()) == 0) {
            return true;
        }
        iter++;
    }
    return false;
}

static int GetCmdParam(uint8_t type, const std::string &origString,
    const std::vector<std::string> &filter, std::vector<std::string> &resultStrings)
{
    static uint32_t paramMinNumber[flashd::UPDATEMOD_MAX + 1] = { 1, 2, 2, 2, 0 };
    std::string::size_type p1 = 0;
    std::string::size_type p2 = origString.find(" ");

    while (p2 != std::string::npos) {
        if (p2 == p1) {
            ++p1;
            p2 = origString.find(" ", p1);
            continue;
        }

        std::string param = origString.substr(p1, p2 - p1);
        if (!FilterParam(param, filter)) {
            resultStrings.push_back(param);
        }
        p1 = p2 + 1;
        p2 = origString.find(" ", p1);
    }

    if (p1 != origString.size()) {
        std::string param = origString.substr(p1);
        if (!FilterParam(param, filter)) {
            resultStrings.push_back(param);
        }
    }
    FLASHING_CHECK((resultStrings.size() >= paramMinNumber[type]) && (type <= flashd::UPDATEMOD_MAX),
        return FLASHING_ARG_INVALID, "Invalid param for %d cmd %s", type, origString.c_str());
    return 0;
}

int CreateFlashInstance(FlashHandle *handle, std::string &errorMsg, ProgressFunction progressor)
{
    int mode = BOOT_UPDATER;
    int ret = updater::GetBootMode(mode);
    FLASHING_CHECK(ret == 0 && mode == BOOT_FLASHD, errorMsg = "Boot mode is not in flashd";
        return FLASHING_SYSTEM_ERROR, "Boot mode error");

    FLASHING_CHECK(!g_flashdRunning, errorMsg = "Flashd has been running";
        return FLASHING_SYSTEM_ERROR, "Flashd has been running");
    g_flashdRunning = true;

    FLASHING_CHECK(handle != nullptr, return FLASHING_ARG_INVALID, "Invalid handle");
    flashd::FlashService *flash = new flashd::FlashService(errorMsg, progressor);
    FLASHING_CHECK(flash != nullptr, errorMsg = "Failed to create flash service";
        return FLASHING_SYSTEM_ERROR, "Failed to create flash service");
    *handle = static_cast<FlashHandle>(flash);
    return 0;
}

int DoUpdaterPrepare(FlashHandle handle, uint8_t type, const std::string &cmdParam, std::string &filePath)
{
    FLASHING_CHECK(handle != nullptr, return FLASHING_ARG_INVALID, "Invalid handle for %d", type);
    flashd::FlashService *flash = static_cast<flashd::FlashService *>(handle);

    std::vector<std::string> params {};
    int ret = GetCmdParam(type, cmdParam, { "-f" }, params);
    FLASHING_CHECK(ret == 0, flash->RecordMsg(updater::ERROR, "Invalid param for %d", type);
        return FLASHING_ARG_INVALID, "Invalid param for %d", type);
    FLASHING_DEBUG("DoUpdaterPrepare type: %d param %s filePath %s", type, cmdParam.c_str(), filePath.c_str());
    switch (type) {
        case flashd::UPDATEMOD_UPDATE: {
            filePath = FLASHD_FILE_PATH + filePath;
            // 检查剩余分区大小，扩展分区
            const std::string root = flashd::FlashService::GetPathRoot(FLASHD_FILE_PATH);
            ret = MountForPath(root);
            FLASHING_CHECK(ret == 0, g_flashdRunning = false;
                flash->RecordMsg(updater::ERROR, "Failed to mount data paratition for %s", filePath.c_str());
                return FLASHING_INVALID_SPACE, "Failed to mount data paratition for %s", filePath.c_str());

            ret = flash->DoResizeParatiton(root, MIN_BLOCKS_FOR_UPDATE);
            FLASHING_CHECK(ret == 0, g_flashdRunning = false;
                return ret, "Failed to resize partition");
            if (access(FLASHD_FILE_PATH.c_str(), F_OK) == -1) {
                mkdir(FLASHD_FILE_PATH.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            }
            break;
        }
        case flashd::UPDATEMOD_FLASH: {
            ret = flash->GetPartitionPath(params[0], filePath);
            break;
        }
        default:
            ret = flashd::FLASHING_SYSTEM_ERROR;
            break;
    }
    return ret;
}

int DoUpdaterFlash(FlashHandle handle, uint8_t type, const std::string &cmdParam, const std::string &filePath)
{
    FLASHING_CHECK(handle != nullptr, return FLASHING_ARG_INVALID, "Invalid handle for %d", type);
    flashd::FlashService *flash = static_cast<flashd::FlashService *>(handle);

    std::vector<std::string> params {};
    int ret = GetCmdParam(type, cmdParam, {"-f"}, params);
    FLASHING_CHECK(ret == 0, g_flashdRunning = false;
        flash->RecordMsg(updater::ERROR, "Invalid param for %d", type);
        return FLASHING_ARG_INVALID, "Invalid param for %d", type);
    FLASHING_DEBUG("DoUpdaterFlash type: %d param %s filePath %s", type, cmdParam.c_str(), filePath.c_str());
    switch (type) {
        case flashd::UPDATEMOD_UPDATE: {
            ret = flash->DoUpdate(filePath);
            break;
        }
        case flashd::UPDATEMOD_ERASE:
            FLASHING_CHECK(params.size() > 1, g_flashdRunning = false;
                return FLASHING_ARG_INVALID, "Invalid param size for erase");
            ret = flash->DoErasePartition(params[1]);
            break;
        case flashd::UPDATEMOD_FORMAT: {
            std::string fsType = GetValueFromParam(params, "-t", "ext4");
            FLASHING_CHECK(params.size() > 1, g_flashdRunning = false;
                return FLASHING_ARG_INVALID, "Invalid param size for format");
            ret = flash->DoFormatPartition(params[1], fsType);
            break;
        }
        default:
            ret = flashd::FLASHING_SYSTEM_ERROR;
            break;
    }
    return ret;
}

int DoUpdaterFinish(FlashHandle handle, uint8_t type, const std::string &partition)
{
    FLASHING_CHECK(handle != nullptr, return FLASHING_ARG_INVALID, "Invalid handle for %d", type);
    FLASHING_DEBUG("DoUpdaterFinish type: %d %s", type, partition.c_str());
    switch (type) {
        case flashd::UPDATEMOD_UPDATE: {
#ifndef UPDATER_UT
            unlink(partition.c_str());
#endif
            updater::PostUpdater(true);
            utils::DoReboot("");
            break;
        }
        case flashd::UPDATEMOD_FLASH: {
            updater::PostUpdater(false);
            break;
        }
        default:
            break;
    }
    g_flashdRunning = false;
    return 0;
}

int SetParameter(const char *key, const char *value)
{
    std::string sKey = key;
    std::string sValue = value;
    std::string sBuf = "param set " + sKey + " " + value;
    system(sBuf.c_str());
    return 0;
}
} // namespace flashd
