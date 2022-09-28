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
#include "update_processor.h"
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include "applypatch/data_writer.h"
#include "applypatch/partition_record.h"
#include "dump.h"
#include "log.h"
#include "pkg_manager.h"
#ifdef UPDATER_USE_PTABLE
#include "ptable_manager.h"
#endif
#include "script_instruction.h"
#include "script_manager.h"
#include "slot_info/slot_info.h"
#include "update_image_block.h"
#include "update_image_patch.h"
#include "update_partitions.h"
#include "updater_main.h"
#include "updater/updater_const.h"

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
size_t UScriptInstructionRawImageWrite::totalSize_ = 0;
size_t UScriptInstructionRawImageWrite::readSize_ = 0;

UpdaterEnv::~UpdaterEnv()
{
    if (factory_ != nullptr) {
        delete factory_;
        factory_ = nullptr;
    }
}

void UpdaterEnv::PostMessage(const std::string &cmd, std::string content)
{
    if (postMessage_ != nullptr) {
        postMessage_(cmd.c_str(), content.c_str());
    }
}

UScriptInstructionFactoryPtr UpdaterEnv::GetInstructionFactory()
{
    if (factory_ == nullptr) {
        factory_ = new UpdaterInstructionFactory();
    }
    return factory_;
}

const std::vector<std::string> UpdaterEnv::GetInstructionNames() const
{
    static std::vector<std::string> updaterCmds = {
        "sha_check", "first_block_check", "block_update",
        "raw_image_write", "update_partitions", "image_patch",
        "image_sha_check"
    };
    return updaterCmds;
}

int32_t UpdaterInstructionFactory::CreateInstructionInstance(UScriptInstructionPtr& instr,
    const std::string& name)
{
    if (name == "sha_check") {
        instr = new UScriptInstructionShaCheck();
    } else if (name == "first_block_check") {
        instr = new UScriptInstructionBlockCheck();
    } else if (name == "block_update") {
        instr = new UScriptInstructionBlockUpdate();
    } else if (name == "raw_image_write") {
        instr = new UScriptInstructionRawImageWrite();
    } else if (name == "update_partitions") {
        instr = new UpdatePartitions();
    } else if (name == "image_patch") {
        instr = new USInstrImagePatch();
    }  else if (name == "image_sha_check") {
        instr = new USInstrImageShaCheck();
    }
    return USCRIPT_SUCCESS;
}

int UScriptInstructionRawImageWrite::RawImageWriteProcessor(const PkgBuffer &buffer, size_t size, size_t start,
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

    if (totalSize_ != 0) {
        readSize_ += size;
        writer->GetUpdaterEnv()->PostMessage("set_progress", std::to_string((float)readSize_ / totalSize_));
    }

    return PKG_SUCCESS;
}

int32_t UScriptInstructionRawImageWrite::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    std::string partitionName;
    int32_t ret = context.GetParam(0, partitionName);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Error to get partitionName", return ret);

    if (env.IsRetry()) {
        LOG(DEBUG) << "Retry updater, check if current partition updated already during last time";
        bool isUpdated = PartitionRecord::GetInstance().IsPartitionUpdated(partitionName);
        if (isUpdated) {
            LOG(INFO) << partitionName << " already updated, skip";
            return USCRIPT_SUCCESS;
        }
    }
    LOG(INFO) << "UScriptInstructionRawImageWrite::Execute " << partitionName;
    UPDATER_ERROR_CHECK(env.GetPkgManager() != nullptr, "Error to get pkg manager", return USCRIPT_ERROR_EXECUTE);

    std::string writePath;
    uint64_t offset = 0;
    uint64_t partitionSize = 0;
    if (GetWritePathAndOffset(partitionName, writePath, offset, partitionSize) != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Get partition:%s WritePathAndOffset fail \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        return USCRIPT_ERROR_EXECUTE;
    }

    std::unique_ptr<DataWriter> writer = DataWriter::CreateDataWriter(WRITE_RAW, writePath,
        static_cast<UpdaterEnv *>(&env), offset);
    UPDATER_ERROR_CHECK(writer != nullptr, "Error to create writer", return USCRIPT_ERROR_EXECUTE);

    // Extract partition information
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(partitionName);
    UPDATER_ERROR_CHECK(info != nullptr, "Error to get file info",
        DataWriter::ReleaseDataWriter(writer); return USCRIPT_ERROR_EXECUTE);
    totalSize_ = info->unpackedSize;
#ifdef UPDATER_USE_PTABLE
    UPDATER_ERROR_CHECK(partitionSize >= totalSize_,
        "partition size: " << partitionSize << " is short than image size: " << totalSize_,
        DataWriter::ReleaseDataWriter(writer); return USCRIPT_ERROR_EXECUTE);
#endif

    ret = env.GetPkgManager()->CreatePkgStream(outStream,
        partitionName, RawImageWriteProcessor, writer.get());
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS && outStream != nullptr, "Error to create output stream",
        DataWriter::ReleaseDataWriter(writer); return USCRIPT_ERROR_EXECUTE);

    ret = env.GetPkgManager()->ExtractFile(partitionName, outStream);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Error to extract file",
        env.GetPkgManager()->ClosePkgStream(outStream);
        DataWriter::ReleaseDataWriter(writer); return USCRIPT_ERROR_EXECUTE);

    PartitionRecord::GetInstance().RecordPartitionUpdateStatus(partitionName, true);
    ret = USCRIPT_SUCCESS;
    env.GetPkgManager()->ClosePkgStream(outStream);
    DataWriter::ReleaseDataWriter(writer);
    totalSize_ = 0;
    readSize_ = 0;
    LOG(INFO)<<"UScriptInstructionRawImageWrite  finish";
    return ret;
}

int ExecUpdate(PkgManager::PkgManagerPtr pkgManager, int retry, PostMessageFunction postMessage)
{
    UpdaterEnv* env = new UpdaterEnv(pkgManager, postMessage, retry);
    if (env == nullptr) {
        LOG(ERROR) << "Fail to creat env";
        UPDATER_LAST_WORD(EXIT_PARSE_SCRIPT_ERROR);
        return EXIT_PARSE_SCRIPT_ERROR;
    }
    int ret = 0;
    ScriptManager* scriptManager = ScriptManager::GetScriptManager(env);
    if (scriptManager == nullptr) {
        LOG(ERROR) << "Fail to creat scriptManager";
        ScriptManager::ReleaseScriptManager();
        delete env;
        UPDATER_LAST_WORD(EXIT_PARSE_SCRIPT_ERROR);
        return EXIT_PARSE_SCRIPT_ERROR;
    }

    UpdaterInit::GetInstance().InvokeEvent(UPDATER_BINARY_INIT_DONE_EVENT);

    for (int32_t i = 0; i < ScriptManager::MAX_PRIORITY; i++) {
        ret = scriptManager->ExecuteScript(i);
        if (ret != USCRIPT_SUCCESS) {
            LOG(ERROR) << "Fail to execute script";
            UPDATER_LAST_WORD(ret);
            break;
        }
    }
    ScriptManager::ReleaseScriptManager();
    delete env;
    return ret;
}

int UScriptInstructionRawImageWrite::GetWritePathAndOffset(const std::string &partitionName, std::string &writePath,
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
#endif
#endif
    return USCRIPT_SUCCESS;
}

int ProcessUpdater(bool retry, int pipeFd, const std::string &packagePath, const std::string &keyPath)
{
    UPDATER_INIT_RECORD;
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_BINARY_INIT_EVENT);
    Dump::GetInstance().RegisterDump("DumpHelperLog", std::make_unique<DumpHelperLog>());
    FILE *pipeWrite = fdopen(pipeFd, "w");
    if (pipeWrite == nullptr) {
        LOG(ERROR) << "Fail to fdopen";
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS);
        return EXIT_INVALID_ARGS;
    }
    // line buffered, make sure parent read per line.
    setlinebuf(pipeWrite);
    PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "Fail to GetPackageInstance";
        fclose(pipeWrite);
        pipeWrite = nullptr;
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS);
        return EXIT_INVALID_ARGS;
    }

    std::vector<std::string> components;
    int32_t ret = pkgManager->LoadPackage(packagePath, keyPath, components);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "Fail to load package";
        fclose(pipeWrite);
        pipeWrite = nullptr;
        PkgManager::ReleasePackageInstance(pkgManager);
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS);
        return EXIT_INVALID_ARGS;
    }
#ifdef UPDATER_USE_PTABLE
    PackagePtable& packagePtb = PackagePtable::GetInstance();
    packagePtb.LoadPartitionInfo(pkgManager);
#endif

    ret = Updater::ExecUpdate(pkgManager, retry,
        [&pipeWrite](const char *cmd, const char *content) {
            if (pipeWrite != nullptr) {
                fprintf(pipeWrite, "%s:%s\n", cmd, content);
            }
        });
    PkgManager::ReleasePackageInstance(pkgManager);
#ifndef UPDATER_UT
    fclose(pipeWrite);
    pipeWrite = nullptr;
    if (ret == 0) {
        SetActiveSlot();
    }
#endif
    return ret;
}
} // Updater