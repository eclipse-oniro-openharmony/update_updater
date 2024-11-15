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
#include <fcntl.h>
#include "log/log.h"
#include "applypatch/store.h"
#include "applypatch/transfer_manager.h"
#include "script_manager.h"
#include "applypatch/partition_record.h"
#include "utils.h"
#include "pkg_manager.h"
#include "updater_env.h"
#include <cstdarg>
#include <securec.h>
#include "hilog/log.h"

constexpr int64_t MAX_BUF_SIZE = 1024;

int HiLogPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[MAX_BUF_SIZE] = {0};
    if (vsnprintf_s(buf, MAX_BUF_SIZE, MAX_BUF_SIZE - 1, fmt, ap) == -1) {
        va_end(ap);
        return 0;
    }
    va_end(ap);
    /* save log to log partition */
    printf("%s", buf);
    return 0;
}

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
constexpr size_t WRITE_FILE_SIZE = 4096;
struct UpdateBlockInfo {
    std::string partitionName;
    std::string transferName;
    std::string newDataName;
    std::string patchDataName;
    std::string devPath;
};

static bool UpdatePathCheck(const std::string &updatePath, size_t length)
{
    if (updatePath.empty() || length == 0) {
        LOG(ERROR) << "updatePath is nullptr.";
        return false;
    }

    char realPath[PATH_MAX + 1] = {0};
    if (realpath(updatePath.c_str(), realPath) == nullptr) {
        LOG(ERROR) << "realPath is NULL" << " : " << strerror(errno);
        return false;
    }

    if (access(realPath, F_OK) != 0) {
        LOG(ERROR) << "package does not exist!";
        return false;
    }

    return true;
}

static int GetUpdateBlockInfo(UpdateBlockInfo &infos, const std::string &packagePath,
    const std::string &srcImage, const std::string &targetPath)
{
    if (!UpdatePathCheck(packagePath, packagePath.length())) {
        LOG(ERROR) << packagePath << " is empty.";
        return -1;
    }

    if (!UpdatePathCheck(srcImage, srcImage.length())) {
        LOG(ERROR) << srcImage << " is empty.";
        return -1;
    }

    if (!UpdatePathCheck(targetPath, targetPath.length())) {
        LOG(INFO) << "need to make store";
        if (Updater::Utils::MkdirRecursive(targetPath, S_IRWXU) != 0) {
            LOG(ERROR) << "Failed to make store";
            return -1;
        }
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
    LOG(INFO) << "outBuf data size is: " << buffSize;
    if (outBuf == nullptr) {
        LOG(ERROR) << "Error to get outBuf";
        return USCRIPT_ERROR_EXECUTE;
    }

    return USCRIPT_SUCCESS;
}

static int32_t ExecuteTransferCommand(int fd, const std::vector<std::string> &lines,
    TransferManagerPtr tm, const std::string &partitionName, const std::string &targetPath)
{
    auto transferParams = tm->GetTransferParams();
    auto writerThreadInfo = transferParams->writerThreadInfo.get();

    transferParams->storeBase = targetPath + partitionName + "_tmp";
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
        (void)Utils::DeleteFile(transferParams->storeBase);
    }
    return Uscript::USCRIPT_SUCCESS;
}

static int32_t DoExecuteUpdateBlock(const UpdateBlockInfo &infos, TransferManagerPtr tm,
    const std::vector<std::string> &lines, const std::string &targetPath, const std::string &dstImage)
{
    int fd = open(dstImage.c_str(), O_RDWR | O_LARGEFILE);
    if (fd == -1) {
        LOG(ERROR) << "Failed to open block";
        return Uscript::USCRIPT_ERROR_EXECUTE;
    }
    int32_t ret = ExecuteTransferCommand(fd, lines, tm, infos.partitionName, targetPath);

    fsync(fd);
    close(fd);
    fd = -1;
    if (ret == Uscript::USCRIPT_SUCCESS) {
        PartitionRecord::GetInstance().RecordPartitionUpdateStatus(infos.partitionName, true);
    }

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

static int CreateFixedSizeEmptyFile(const UpdateBlockInfo &infos, const std::string &filename, int64_t size)
{
    if (size <= 0) {
        LOG(ERROR) << "size is " << size;
        return -1;
    }
    if (!Updater::Utils::CopyFile(infos.devPath, filename)) {
        LOG(ERROR) << "copy " << infos.devPath << " to " << filename << " failed";
        return -1;
    }
    size_t fileSize = Updater::Utils::GetFileSize(infos.devPath);
    if (fileSize >= (static_cast<size_t>(size))) {
        LOG(INFO) << "no need copy";
        return 0;
    }
    std::ofstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open file for writing.";
        return -1;
    }

    /* fill the remaining space with zero values */
    size_t writeFileTmp = ((static_cast<size_t>(size)) - fileSize) / WRITE_FILE_SIZE;
    char zerolist[WRITE_FILE_SIZE] = {0};
    while (writeFileTmp > 0) {
        file.write(zerolist, WRITE_FILE_SIZE);
        writeFileTmp--;
    }
    writeFileTmp = ((static_cast<size_t>(size)) - fileSize) % WRITE_FILE_SIZE;
    char zero = 0;
    while (writeFileTmp > 0) {
        file.write(&zero, 1);
        writeFileTmp--;
    }

    file.close();
    return 0;
}

static std::string GetFileName(const std::string &srcImage)
{
    std::vector<std::string> lines =
        Updater::Utils::SplitString(std::string(srcImage), "/");
    LOG(INFO) << "lines.size is " << lines.size();
    if (lines.size() == 0) {
        return nullptr;
    }
    return lines[lines.size() - 1];
}

static int32_t ExecuteUpdateBlock(Uscript::UScriptEnv &env, const UpdateBlockInfo &infos,
    const std::string targetPath, std::string destImage)
{
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    uint8_t *transferListBuffer = nullptr;
    size_t transferListSize = 0;
    uint8_t *fileSizeBuffer = nullptr;
    size_t fileListSize = 0;
    const std::string fileName = "anco_size";
    if (ExtractFileByNameFunc(env, fileName, outStream, fileSizeBuffer,
                              fileListSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }
    env.GetPkgManager()->ClosePkgStream(outStream);
    std::string str(reinterpret_cast<char*>(fileSizeBuffer), fileListSize);
    int64_t maxStashSize = 0;
    if (!Utils::ConvertToLongLong(str, maxStashSize)) {
        LOG(ERROR) << "ConvertToLongLong failed";
        return USCRIPT_ERROR_EXECUTE;
    }
    if (CreateFixedSizeEmptyFile(infos, destImage, maxStashSize) != 0) {
        LOG(ERROR) << "Failed to create empty file";
        return USCRIPT_ERROR_EXECUTE;
    }

    if (ExtractFileByNameFunc(env, infos.transferName,
        outStream, transferListBuffer, transferListSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    std::unique_ptr<TransferManager> tm = std::make_unique<TransferManager>();
    auto transferParams = tm->GetTransferParams();
    /* Save Script Env to transfer manager */
    transferParams->env = &env;
    std::vector<std::string> lines =
        Updater::Utils::SplitString(std::string(reinterpret_cast<const char*>(transferListBuffer)), "\n");
    env.GetPkgManager()->ClosePkgStream(outStream);

    if (ExtractFileByNameFunc(env, infos.patchDataName, outStream,
        transferParams->patchDataBuffer, transferParams->patchDataSize) != USCRIPT_SUCCESS) {
        return USCRIPT_ERROR_EXECUTE;
    }

    LOG(INFO) << "Ready to start a thread to handle new data processing";
    if (InitThread(infos, tm.get()) != 0) {
        LOG(ERROR) << "Failed to create pthread";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    int32_t ret = DoExecuteUpdateBlock(infos, tm.get(), lines, targetPath, destImage);
    env.GetPkgManager()->ClosePkgStream(outStream);
    return ret;
}

int RestoreOriginalFile(const std::string &packagePath, const std::string &srcImage, const std::string &targetPath)
{
    UpdateBlockInfo infos {};
    if (GetUpdateBlockInfo(infos, packagePath, srcImage, targetPath) != 0) {
        return USCRIPT_ERROR_EXECUTE;
    }
    std::string destName = GetFileName(srcImage);
    std::string destImage = targetPath + "/" + destName;

    PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return USCRIPT_ERROR_EXECUTE;
    }

    std::vector<std::string> components;
    std::string pckPath = packagePath;
    int32_t ret = pkgManager->LoadPackage(pckPath, components, PkgFile::PKG_TYPE_ZIP);
    if (ret != PKG_SUCCESS) {
        LOG(ERROR) << "Fail to load package";
        return USCRIPT_ERROR_EXECUTE;
    }
    PostMessageFunction postMessage = nullptr;
    UScriptEnv *env = new (std::nothrow) UpdaterEnv(pkgManager, postMessage, false);
    if (env == nullptr) {
        LOG(ERROR) << "Fail to creat env";
        return USCRIPT_ERROR_EXECUTE;
    }

    int result = ExecuteUpdateBlock(*env, infos, targetPath, destImage);
    if (result != 0) {
        (void)Utils::DeleteFile(destImage);
        LOG(ERROR) << "restore original file fail.";
    }
    (void)Utils::DeleteFile(packagePath);
    (void)Utils::DeleteFile(infos.devPath);
    delete env;
    env = nullptr;
    return result;
}
}