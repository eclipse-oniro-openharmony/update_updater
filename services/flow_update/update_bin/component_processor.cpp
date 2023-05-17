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

#include "component_processor.h"
#include <fcntl.h>
#include "applypatch/data_writer.h"
#include "applypatch/partition_record.h"
#include "log.h"
#include "parameter.h"
#include "slot_info/slot_info.h"

using namespace std;
using namespace std::placeholders;
using namespace Hpackage;
using namespace Uscript;

namespace Updater {
REGISTER_PROCESSOR(VersionCheckProcessor, "/version_list")
REGISTER_PROCESSOR(BoardIdCheckProcessor, "/board_list")
REGISTER_PROCESSOR(RawImgProcessor, "/uboot", "/boot_linux",
                   "/system", "/vendor", "/resource", "/updater", "/userdata")

ComponentProcessorFactory &ComponentProcessorFactory::GetInstance()
{
    static ComponentProcessorFactory instance;
    return instance;
}

void ComponentProcessorFactory::RegisterProcessor(Constructor constructor, std::vector<std::string> &nameList)
{
    for (auto &iter : nameList) {
        if (!m_constructorMap.emplace(iter, constructor).second) {
            LOG(ERROR) << "emplace: " << iter.c_str() << " fail";
        }
    }
}

std::unique_ptr<ComponentProcessor> ComponentProcessorFactory::GetProcessor(const std::string &name,
    const uint8_t len) const
{
    auto it = m_constructorMap.find(name);
    if (it == m_constructorMap.end() || it->second == nullptr) {
        LOG(ERROR) << "GetProcessor for: " << name.c_str() << " fail";
        return nullptr;
    }
    return (*(it->second))(name, len);
}

int32_t VersionCheckProcessor::DoProcess(Uscript::UScriptEnv &env)
{
    PackagesInfoPtr pkginfomanager = PackagesInfo::GetPackagesInfoInstance();
    if (pkginfomanager == nullptr) {
        LOG(ERROR) << "Fail to pkginfomanager";
        return PKG_INVALID_VERSION;
    }

    if (env.GetPkgManager() == nullptr || pkginfomanager == nullptr) {
        return PKG_INVALID_VERSION;
    }
    const char *verPtr = GetDisplayVersion();
    if (verPtr == nullptr) {
        LOG(ERROR) << "Fail to GetDisplayVersion";
        return PKG_INVALID_VERSION;
    }
    std::string verStr(verPtr);
    LOG(INFO) << "current version:" << verStr;
    int ret = -1;
    std::vector<std::string> targetVersions = pkginfomanager->GetOTAVersion(env.GetPkgManager(), "/version_list", "");
    for (size_t i = 0; i < targetVersions.size(); i++) {
        LOG(INFO) << "Check version " << targetVersions[i];
        ret = verStr.compare(targetVersions[i]);
        if (ret == 0) {
            LOG(INFO) << "Check version success";
            break;
        }
    }
#ifndef UPDATER_UT
    return ret;
#else
    return USCRIPT_SUCCESS;
#endif
}

int32_t BoardIdCheckProcessor::DoProcess(Uscript::UScriptEnv &env)
{
    PackagesInfoPtr pkginfomanager = PackagesInfo::GetPackagesInfoInstance();
    if (pkginfomanager == nullptr) {
        LOG(ERROR) << "Fail to get pkginfomanager";
        return PKG_INVALID_VERSION;
    }

    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Fail to GetPkgManager";
        return PKG_INVALID_VERSION;
    }

    std::string localBoardId = Utils::GetLocalBoardId();
    if (localBoardId.empty()) {
        return 0;
    }

    int ret = -1;
    std::vector<std::string> boardIdList = pkginfomanager->GetBoardID(env.GetPkgManager(), "/board_list", "");
    for (size_t i = 0; i < boardIdList.size(); i++) {
        LOG(INFO) << "Check BoardId " << boardIdList[i];
        ret = localBoardId.compare(boardIdList[i]);
        if (ret == 0) {
            LOG(INFO) << "Check board list success ";
            break;
        }
    }
#ifndef UPDATER_UT
    return ret;
#else
    return USCRIPT_SUCCESS;
#endif
}

int32_t RawImgProcessor::PreProcess(Uscript::UScriptEnv &env)
{
    std::string partitionName = name_;
    LOG(INFO) << "RawImgProcessor::PreProcess " << partitionName;
    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_ERROR_EXECUTE;
    }

    std::string writePath;
    uint64_t offset = 0;
    uint64_t partitionSize = 0;
    if (GetWritePathAndOffset(partitionName, writePath, offset, partitionSize) != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Get partition:%s WritePathAndOffset fail \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        return USCRIPT_ERROR_EXECUTE;
    }

    writer_ = DataWriter::CreateDataWriter(WRITE_RAW, writePath,
        static_cast<UpdaterEnv *>(&env), offset);
    if (writer_ == nullptr) {
        LOG(ERROR) << "Error to create writer";
        return USCRIPT_ERROR_EXECUTE;
    }
#ifdef UPDATER_UT
    int fd = open(writePath.c_str(), O_RDWR | O_CREAT);
    if (fd >= 0) {
        close(fd);
    }
#endif
    return USCRIPT_SUCCESS;
}

int32_t RawImgProcessor::DoProcess(Uscript::UScriptEnv &env)
{
    std::string partitionName = name_;
    // Extract partition information
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(partitionName);
    if (info == nullptr) {
        LOG(ERROR) << "Error to get file info";
        return USCRIPT_ERROR_EXECUTE;
    }

#ifdef UPDATER_USE_PTABLE
    if (partitionSize < totalSize_) {
        LOG(ERROR) << "partition size: " << partitionSize << " is short than image size: " << totalSize_;
        return USCRIPT_ERROR_EXECUTE;
    }
#endif

    PkgStream::ExtractFileProcessor processor =
        [this](const PkgBuffer &buffer, size_t size, size_t start, bool isFinish, const void *context) {
            return this->RawImageWriteProcessor(buffer, size, start, isFinish, context);
        };

    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    int ret = env.GetPkgManager()->CreatePkgStream(outStream, partitionName, processor, writer_.get());
    if (ret != USCRIPT_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_ERROR_EXECUTE;
    }

    ret = env.GetPkgManager()->ExtractFile(partitionName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    env.GetPkgManager()->ClosePkgStream(outStream);
    return USCRIPT_SUCCESS;
}

int32_t RawImgProcessor::PostProcess(Uscript::UScriptEnv &env)
{
    PartitionRecord::GetInstance().RecordPartitionUpdateStatus(name_, true);
    DataWriter::ReleaseDataWriter(writer_);
    totalSize_ = 0;
    LOG(INFO) << "UScriptInstructionRawImageWrite finish";
    return USCRIPT_SUCCESS;
}

int RawImgProcessor::GetWritePathAndOffset(const std::string &partitionName, std::string &writePath,
                                           uint64_t &offset, uint64_t &partitionSize)
{
#ifdef UPDATER_USE_PTABLE
    PackagePtable& packagePtb = PackagePtable::GetInstance();
    Ptable::PtnInfo ptnInfo;
    if (!packagePtb.GetPartionInfoByName(partitionName, ptnInfo)) {
        LOG(ERROR) << "Datawriter: cannot find device path for partition \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        return USCRIPT_ERROR_EXECUTE;
    }
    char lunIndexName = 'a' + ptnInfo.lun;
    writePath = std::string(PREFIX_UFS_NODE) + lunIndexName;
    offset = ptnInfo.startAddr;
    partitionSize = ptnInfo.partitionSize;
#else
    writePath = GetBlockDeviceByMountPoint(partitionName);
    if (writePath.empty()) {
        LOG(ERROR) << "Datawriter: cannot find device path for partition \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        return USCRIPT_ERROR_EXECUTE;
    }

#ifndef UPDATER_UT
    if (partitionName != "/userdata") {
        std::string suffix = "";
        GetPartitionSuffix(suffix);
        writePath += suffix;
    }
    LOG(INFO) << "write partition path: " << writePath;
#else
    writePath = "/data/updater" + partitionName;
#endif
#endif
    return USCRIPT_SUCCESS;
}

int RawImgProcessor::RawImageWriteProcessor(const PkgBuffer &buffer, size_t size, size_t start,
                                            bool isFinish, const void* context)
{
    void *p = const_cast<void *>(context);
    DataWriter *writer = static_cast<DataWriter *>(p);
    if (writer == nullptr) {
        LOG(ERROR) << "Data writer is null";
        return PKG_INVALID_STREAM;
    }

    // maybe extract from package is finished. just return.
    if (buffer.buffer == nullptr || size == 0) {
        return PKG_SUCCESS;
    }

    bool ret = writer->Write(const_cast<uint8_t*>(buffer.buffer), size, nullptr);
    if (!ret) {
        LOG(ERROR) << "Write " << size << " byte(s) failed";
        return PKG_INVALID_STREAM;
    }

    if (pkgFileSize_ != 0) {
        readOffset_ += size;
        writer->GetUpdaterEnv()->PostMessage("set_progress", std::to_string((float)readOffset_ / pkgFileSize_));
        LOG(INFO) << "set_progress readsize: " << readOffset_ << " totalsize: " << pkgFileSize_ <<" byte(s)";
    }

    return PKG_SUCCESS;
}
}