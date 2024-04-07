/*
 * Copyright (c) 2024. Huawei Device Co., Ltd.
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

#include "patch_shared.h"

#include <string>
#include <unistd.h>
#include <cstdio>
// #include <stdio.h>
#include <fcntl.h>
#include "log/log.h"
#include "applypatch/store.h"
#include "applypatch/transfer_manager.h"
#include "script_manager.h"
#include "applypatch/partition_record.h"
#include "utils.h"
#include "pkg_manager.h"
#include "updater_env.h"

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
struct UpdateBlockInfo {
    std::string partitionName;
    std::string transferName;
    std::string newDataName;
    std::string patchDataName;
    std::string devPath;
};

static bool UpdatePathCheck(const std::string &updatePath)
{
    if (updatePath.c_str() == nullptr) {
        LOG(ERROR) << "updatePath is nullptr.";
        return false;
    }

    if (access(updatePath.c_str(), F_OK) != 0) {
        LOG(ERROR) << "package does not exist!";
        return false;
    }

    return true;
}

static int GetUpdateBlockInfo(UpdateBlockInfo &infos, const std::string &path, const std::string &srcImage)
{
    if (UpdatePathCheck(path)) {
        LOG(ERROR) << path << " is empty.";
        return -1;
    }
    if (UpdatePathCheck(srcImage)) {
        LOG(ERROR) << srcImage << " is empty.";
        return -1;
    }

    infos.newDataName = "anco_hmos.new.dat";

    infos.patchDataName = "anco_hmos.patch.dat";

    infos.transferName = "anco_hmos.transfer.list";

    infos.devPath = srcImage;

    infos.partitionName = "/anco_hmos";

    return 0;
}

static int32_t ExtractFileByNameFunc(Uscript::UScriptEnv &env, const std::string &fileName,
                                     Hpackage::PkgManager::StreamPtr &outStream, uint8_t *&outBuf, size_t &buffSize)
{
    if (env.GetPkgManager() == nullptr) {
        LOG(ERROR) << "Error to get pkg manager";
        return USCRIPT_ERROR_EXECUTE;
    }

    const FileInfo *info = env.GetPkgManager()->GetFileInfo(fileName);
    if (info == nullptr) {
        LOG(ERROR) << "GetFileInfo fail";
        return USCRIPT_ERROR_EXECUTE;
    }
    auto ret = env.GetPkgManager()->CreatePkgStream(outStream,
        fileName, info->unpackedSize, PkgStream::PkgStreamType_MemoryMap);
    if (ret != USCRIPT_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return USCRIPT_ERROR_EXECUTE;
    }
    ret = env.GetPkgManager()->ExtractFile(fileName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    ret = outStream->GetBuffer(outBuf, buffSize);
    LOG(DEBUG) << "outBuf data size is: " << buffSize;

    return USCRIPT_SUCCESS;
}

static int32_t ExecuteTransferCommand(int fd, const std::vector<std::string> &lines,
                                      TransferManagerPtr tm, const std::string &partitionName)
{
    auto transferParams = tm->GetTransferParams();
    auto writerThreadInfo = transferParams->writerThreadInfo.get();

    transferParams->storeBase = std::string("/data/updater") + partitionName + "_tmp";
    LOG(INFO) << "Store base path is " << transferParams->storeBase;
    int32_t ret = Store::CreateNewSpace(transferParams->storeBase, true);
    if (ret == -1) {
        LOG(ERROR) << "Error to create new store space";
        return -1;
    }
    transferParams->storeCreated = ret;

    if (!tm->CommandsParser(fd, lines)) {
        return Uscript::USCRIPT_ERROR_EXECUTE;
    }
    pthread_mutex_lock(&writerThreadInfo->mutex);
    if (writerThreadInfo->readyToWrite) {
        LOG(WARNING) << "New data writer thread is still available...";
    }

    writerThreadInfo->readyToWrite = false;
    pthread_cond_broadcast(&writerThreadInfo->cond);
    pthread_mutex_unlock(&writerThreadInfo->mutex);
    ret = pthread_join(transferParams->thread, nullptr);
    std::ostringstream logMessage;
    logMessage << "pthread join returned with " << ret;
    if (ret != 0) {
        LOG(WARNING) << logMessage.str();
    }
    if (transferParams->storeCreated != -1) {
        Store::DoFreeSpace(transferParams->storeBase);
    }
    return Uscript::USCRIPT_SUCCESS;
}

static int32_t DoExecuteUpdateBlock(const UpdateBlockInfo &infos, TransferManagerPtr tm,
    Hpackage::PkgManager::StreamPtr &outStream, const std::vector<std::string> &lines, const std::string &targetPath)
{
    int fd = open(infos.devPath.c_str(), O_RDWR | O_LARGEFILE);
    auto env = tm->GetTransferParams()->env;
    if (fd == -1) {
        LOG(ERROR) << "Failed to open block";
        env->GetPkgManager()->ClosePkgStream(outStream);
        return Uscript::USCRIPT_ERROR_EXECUTE;
    }
    int32_t ret = ExecuteTransferCommand(fd, lines, tm, infos.partitionName);

    fsync(fd);
    close(fd);
    fd = -1;
    env->GetPkgManager()->ClosePkgStream(outStream);
    if (ret == Uscript::USCRIPT_SUCCESS) {
        PartitionRecord::GetInstance().RecordPartitionUpdateStatus(infos.partitionName, true);
    }
    if (!Updater::Utils::CopyFile(infos.devPath, targetPath)) {
        LOG(ERROR) << "copy " << infos.devPath << " to " << targetPath << " failed";
        ret = Uscript::USCRIPT_ERROR_CREATE_THREAD;
    }
    (void)Utils::DeleteFile(infos.devPath);
    
    return ret;
}

static int ExtractNewDataFunc(const PkgBuffer &buffer, size_t size, size_t start, bool isFinish, const void* context)
{
    void *p = const_cast<void *>(context);
    WriterThreadInfo *info = static_cast<WriterThreadInfo *>(p);
    uint8_t *addr = buffer.buffer;
    while (size > 0) {
        pthread_mutex_lock(&info->mutex);
        while (info->writer == nullptr) {
            if (!info->readyToWrite) {
                LOG(WARNING) << "writer is not ready to write.";
                pthread_mutex_unlock(&info->mutex);
                return Hpackage::PKG_INVALID_STREAM;
            }
            pthread_cond_wait(&info->cond, &info->mutex);
        }
        pthread_mutex_unlock(&info->mutex);
        size_t toWrite = std::min(size, info->writer->GetBlocksSize() - info->writer->GetTotalWritten());
        // No more data to write.
        if (toWrite == 0) {
            break;
        }
        bool ret = info->writer->Write(addr, toWrite, nullptr);
        std::ostringstream logMessage;
        logMessage << "Write " << toWrite << " byte(s) failed";
        if (!ret) {
            LOG(ERROR) << logMessage.str();
            return Hpackage::PKG_INVALID_STREAM;
        }
        size -= toWrite;
        addr += toWrite;

        if (info->writer->IsWriteDone()) {
            pthread_mutex_lock(&info->mutex);
            info->writer.reset();
            pthread_cond_broadcast(&info->cond);
            pthread_mutex_unlock(&info->mutex);
        }
    }
    return Hpackage::PKG_SUCCESS;
}

static inline void CondBroadcast(WriterThreadInfo *info)
{
    pthread_mutex_lock(&info->mutex);
    info->readyToWrite = false;
    if (info->writer != nullptr) {
        pthread_cond_broadcast(&info->cond);
    }
    pthread_mutex_unlock(&info->mutex);
}

void* UnpackNewDataFunc(void *arg)
{
    TransferManagerPtr tm = static_cast<TransferManagerPtr>(arg);
    WriterThreadInfo *info = tm->GetTransferParams()->writerThreadInfo.get();
    Hpackage::PkgManager::StreamPtr stream = nullptr;
    if (info->newPatch.empty()) {
        LOG(ERROR) << "new patch file name is empty. thread quit.";
        CondBroadcast(info);
        return nullptr;
    }
    LOG(DEBUG) << "new patch file name: " << info->newPatch;
    auto env = tm->GetTransferParams()->env;
    const FileInfo *file = env->GetPkgManager()->GetFileInfo(info->newPatch);
    if (file == nullptr) {
        LOG(ERROR) << "Cannot get file info of :" << info->newPatch;
        CondBroadcast(info);
        return nullptr;
    }
    LOG(DEBUG) << info->newPatch << " info: size " << file->packedSize << " unpacked size " <<
        file->unpackedSize << " name " << file->identity;
    int32_t ret = env->GetPkgManager()->CreatePkgStream(stream, info->newPatch, ExtractNewDataFunc, info);
    if (ret != Hpackage::PKG_SUCCESS || stream == nullptr) {
        LOG(ERROR) << "Cannot extract " << info->newPatch << " from package.";
        CondBroadcast(info);
        return nullptr;
    }
    ret = env->GetPkgManager()->ExtractFile(info->newPatch, stream);
    env->GetPkgManager()->ClosePkgStream(stream);
    LOG(DEBUG) << "new data writer ending...";
    // extract new data done.
    // tell command.
    CondBroadcast(info);
    return nullptr;
}

static int InitThread(const struct UpdateBlockInfo &infos, TransferManagerPtr tm)
{
    auto transferParams = tm->GetTransferParams();
    auto writerThreadInfo = transferParams->writerThreadInfo.get();
    writerThreadInfo->readyToWrite = true;
    pthread_mutex_init(&writerThreadInfo->mutex, nullptr);
    pthread_cond_init(&writerThreadInfo->cond, nullptr);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    writerThreadInfo->newPatch = infos.newDataName;
    int error = pthread_create(&transferParams->thread, &attr, UnpackNewDataFunc, tm);
    return error;
}

int RestoreOriginalFile(const std::string &path, const std::string &srcImage, const std::string &targetPath)
{
    UpdateBlockInfo infos {};
    if (GetUpdateBlockInfo(infos, path, srcImage) != 0) {
        return -1;
    }

    PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return -1;
    }

    std::vector<std::string> components;
    std::string packagePath = path;
    int32_t ret = pkgManager->LoadPackage(packagePath, components, PkgFile::PKG_TYPE_ZIP);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "Fail to load package";
        return -1;
    }
    PostMessageFunction postMessage = nullptr;
    UScriptEnv *env = new (std::nothrow) UpdaterEnv(pkgManager, postMessage, false);
    if (env == nullptr) {
        LOG(ERROR) << "Fail to creat env";
        return -1;
    }

    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    uint8_t *transferListBuffer = nullptr;
    size_t transferListSize = 0;
    if (ExtractFileByNameFunc(*env, infos.transferName,
        outStream, transferListBuffer, transferListSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    std::unique_ptr<TransferManager> tm = std::make_unique<TransferManager>();
    auto transferParams = tm->GetTransferParams();
    /* Save Script Env to transfer manager */
    transferParams->env = env;
    std::vector<std::string> lines =
        Updater::Utils::SplitString(std::string(reinterpret_cast<const char*>(transferListBuffer)), "\n");

    LOG(INFO) << "Ready to start a thread to handle new data processing";
    if (ExtractFileByNameFunc(*env, infos.patchDataName, outStream,
        transferParams->patchDataBuffer, transferParams->patchDataSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    LOG(INFO) << "Ready to start a thread to handle new data processing";
    if (InitThread(infos, tm.get()) != 0) {
        LOG(ERROR) << "Failed to create pthread";
        return USCRIPT_ERROR_EXECUTE;
    }
    return DoExecuteUpdateBlock(infos, tm.get(), outStream, lines, targetPath);
}
}