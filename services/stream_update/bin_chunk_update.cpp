/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "bin_chunk_update.h"

#include <algorithm>
#include <fcntl.h>
#include <functional>
#include <iomanip> // 用于 std::setw 和 std::setfill
#include <iostream>
#include <sstream> // 用于 std::ostringstream
#include <sys/stat.h>

#include "applypatch/command_process.h"
#include "applypatch/store.h"
#include "fs_manager/mount.h"
#include "log.h"
#include "scope_guard.h"
#include "slot_info/slot_info.h"
#include "utils.h"

namespace Updater {
using namespace Uscript;
using namespace Hpackage;
using namespace std::placeholders;

constexpr const char *UPDATE_BIN_FILE = "update.bin";
constexpr const size_t HASH_BUFFER_SIZE = 50 * 1024;
constexpr uint16_t HEADER_TYPE_BYTE = 2;
constexpr uint16_t TOTAL_TL_BYTES = 6;
constexpr uint8_t ZIP_HEADER_TLV_TYPE = 0xaa;

BinChunkUpdate::BinChunkUpdate(uint32_t maxBufSize)
{
    LOG(DEBUG) << "BinChunkUpdate::BinChunkUpdate enter";
    maxBufSize_ = maxBufSize;
    buffer_ = new uint8_t[maxBufSize_];
    chunkInstallProcess_.emplace(CHUNK_INSTALL_STEP_PRE, [this](uint8_t *data, uint32_t &len) {
        return this->ChunkInstallPreWrite(data, len);
    });
    chunkInstallProcess_.emplace(CHUNK_INSTALL_STEP_DO, [this](uint8_t *data, uint32_t &len) {
        return this->ChunkInstallDoWrite(data, len);
    });
    chunkInstallProcess_.emplace(CHUNK_INSTALL_STEP_POST, [this](uint8_t *data, uint32_t &len) {
        return this->ChunkInstallPostWrite(data, len);
    });
    pkgManager_ = PkgManager::CreatePackageInstance();
}

BinChunkUpdate::~BinChunkUpdate()
{
    PkgManager::ReleasePackageInstance(pkgManager_);
    if (buffer_ != nullptr) {
        delete[] buffer_;
        buffer_ = nullptr;
    }
}

UpdateResultCode BinChunkUpdate::StartBinChunkUpdate(const uint8_t *data, uint32_t len, uint32_t &dealLen)
{
    LOG(DEBUG) << "BinChunkUpdate::StartBinChunkUpdate enter";
    UpdateResultCode ret = STREAM_UPDATE_SUCCESS;
    if (data == nullptr || len == 0 || pkgManager_ == nullptr) {
        LOG(ERROR) << "para error";
        return STREAM_UPDATE_FAILURE;
    }
    uint32_t remainLen = len;
    uint32_t leftLen = curlen_;
    LOG(DEBUG) << "BinChunkUpdate::StartBinChunkUpdate leftLen:" << leftLen;
    dealLen = 0;

    if (ProcessHeader(data) == STREAM_UPDATE_FAILURE) {
        return STREAM_UPDATE_FAILURE;
    }

    if (SkipTargetData(data, len, dealLen) == STREAM_UPDATE_SUCCESS) {
        return STREAM_UPDATE_SUCCESS;
    }

    while (remainLen > 0) {
        if (!AddRemainData(data + len - remainLen, remainLen)) {
            LOG(ERROR) << "AddRemainData error";
            return STREAM_UPDATE_FAILURE;
        }
        updateInfo_.needNewData = false;

        // 处理缓冲区的数据
        ret = ProcessBufferData();
        if (ret == STREAM_UPDATE_FAILURE) {
            return ret;
        }

        // 移动剩余数据
        if (!MoveRemainingData()) {
            return STREAM_UPDATE_FAILURE;
        }
    }

    dealLen = len + leftLen - curlen_;
    LOG(DEBUG) << "BinChunkUpdate StartBinChunkUpdate dealLen:" << dealLen << " len:" << len << " curlen_:" << curlen_
              << " leftLen:" << leftLen;
    return ret;
}

UpdateResultCode BinChunkUpdate::ProcessHeader(const uint8_t *data)
{
    if (firstBuffer) {
        int type = ReadLE16(data);
        LOG(INFO) << "type = " << type;
        if (type != ZIP_HEADER_TLV_TYPE) {
            LOG(INFO) << "Not support type " << type;
            skipLength_ = 0;
            firstBuffer = false;
            return STREAM_UPDATE_SUCCESS;
        }
        firstBuffer = false;
        skipLength_ = ReadLE32(data + HEADER_TYPE_BYTE) + TOTAL_TL_BYTES;
        LOG(INFO) << "Skipped chunk: type=0xaa, length=" << skipLength_;
    } else {
        LOG(INFO) << "no need process length";
    }
    return STREAM_UPDATE_SUCCESS;
}

UpdateResultCode BinChunkUpdate::SkipTargetData(const uint8_t *data, uint32_t len, uint32_t &dealLen)
{
    if (skipLength_ <= 0) {
        LOG(ERROR) << "no valid skipRemaining_ = ";
        return STREAM_UPDATE_FAILURE;
    }
    const size_t skip = std::min<size_t>(skipLength_, len);
    if (skipLength_ < len) {
        LOG(INFO) << "Add remain data to buffer_" << skipLength_;
        if (memmove_s(buffer_, len - skipLength_, data + skipLength_, len - skipLength_) != EOK) {
            LOG(ERROR) << "memmove failed";
            skipLength_ = 0;
            return STREAM_UPDATE_FAILURE;
        }
        dealLen = skipLength_;
        curlen_ = len - skipLength_;
        skipLength_ = 0;
        return STREAM_UPDATE_SUCCESS;
    }
    skipLength_ -= skip;
    LOG(INFO) << "skipRemaining_ = " << skipLength_;
    dealLen = len;
    return STREAM_UPDATE_SUCCESS;
}

UpdateResultCode BinChunkUpdate::ProcessBufferData()
{
    UpdateResultCode ret = STREAM_UPDATE_SUCCESS;
    while ((curlen_ - offset_) > 0 && !updateInfo_.needNewData) {
        LOG(DEBUG) << "BinChunkUpdate::StartBinChunkUpdate curlen_:" << curlen_ << " offset_:" << offset_;
        uint16_t type = ReadLE16(buffer_ + offset_);
        LOG(DEBUG) << "BinChunkUpdate::StartBinChunkUpdate type:" << type;

        switch (type) {
            case BIN_UPDATE_HEAD_TIP:
                ret = UpdateBinHead(buffer_, curlen_);
                break;
            case BIN_UPDATE_DATA_TIP:
                ret = UpdateBinData(buffer_, curlen_);
                break;
            case BIN_UPDATE_HASH_TIP:
                ret = UpdateBinHash(buffer_, curlen_);
                break;
            default:
                ret = UpdateBinOther(buffer_, curlen_);
                break;
        }

        if (ret == STREAM_UPDATE_FAILURE) {
            LOG(ERROR) << "Update failed for type: " << type;
            return ret;
        }
    }

    return ret;
}

bool BinChunkUpdate::MoveRemainingData()
{
    LOG(DEBUG) << "curlen_:" << curlen_ << " offset_:" << offset_;

    if ((curlen_ - offset_) > 0) {
        if (memmove_s(buffer_, curlen_, buffer_ + offset_, curlen_ - offset_) != EOK) {
            LOG(ERROR) << "memmove failed";
            return false;
        }
        curlen_ -= offset_;
    } else {
        if (memset_s(buffer_, maxBufSize_, 0, maxBufSize_) != 0) {
            LOG(ERROR) << "memset_s failed";
            return false;
        }
        curlen_ = 0;
    }

    return true;
}

bool BinChunkUpdate::AddRemainData(const uint8_t *data, uint32_t &len)
{
    if (data == nullptr || len == 0) {
        LOG(ERROR) << "AddRemainData para error";
        return false;
    }
    uint32_t copySize = std::min(static_cast<size_t>(len), static_cast<size_t>(maxBufSize_ - curlen_));
    LOG(DEBUG) << "BinChunkUpdate AddRemainData curlen_:" << curlen_ << " copySize:" << copySize;
    if (memcpy_s(buffer_ + curlen_, maxBufSize_, data, copySize) != EOK) {
        LOG(ERROR) << "AddRemainData memcpy failed" << " : " << strerror(errno);
        return false;
    }
    curlen_ += copySize;
    len -= copySize;
    offset_ = 0;
    return true;
}

UpdateResultCode BinChunkUpdate::UpdateBinHash(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash enter";
    uint32_t offset = offset_;
    PartitionHashInfo hashInfos;
    std::vector<uint8_t> signData;
    std::vector<std::future<bool>> futures;

    // 初始化 SHA256 算法
    updateInfo_.algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(PKG_DIGEST_TYPE_SHA256);
    if (updateInfo_.algorithm == nullptr) {
        LOG(ERROR) << "algorithm is null";
        return STREAM_UPDATE_FAILURE;
    }
    updateInfo_.algorithm->Init();

    // 读取分区数量
    if (!ProcessPartitionNum(data, len, offset)) {
        return STREAM_UPDATE_SUCCESS;
    }

    // 读取分区hash数据
    if (!ProcessPartitionData(data, len, offset, hashInfos)) {
        return STREAM_UPDATE_SUCCESS;
    }

    // 读取签名
    if (!ProcessSignature(data, len, offset, signData)) {
        return STREAM_UPDATE_SUCCESS;
    }

    offset_ = offset;

    // 签名验证
    if (!VerifySignature(signData)) {
        return STREAM_UPDATE_FAILURE;
    }

    // 完整性验证（异步处理哈希验证）
    if (!VerifyPartitionHashes(hashInfos, futures)) {
        return STREAM_UPDATE_FAILURE;
    }

    int result = remove("/data/updater/test.txt");
    if (result != 0) {
        LOG(ERROR) << "Failed to remove /data/updater/test.txt, error: " << strerror(errno);
    }

    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash exit";
    return STREAM_UPDATE_COMPLETE;
}

bool BinChunkUpdate::ProcessPartitionNum(uint8_t *data, uint32_t &len, uint32_t &offset)
{
    PkgTlvHH tlv;

    // 读取TLV头部信息
    if ((len - offset) < sizeof(PkgTlvHH)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv.type = ReadLE16(data + offset);
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash tlv.type:" << tlv.type;
    tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash tlv.length:" << tlv.length;
    updateInfo_.algorithm->Update({data + offset, sizeof(PkgTlvHH)}, sizeof(PkgTlvHH));
    offset += sizeof(PkgTlvHH);

    // 读取分区数量
    if ((len - offset) < tlv.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    updateInfo_.patitionNum = ReadLE16(data + offset);
    LOG(INFO) << "BinChunkUpdate::UpdateBinHash patitionNum:" << updateInfo_.patitionNum;
    updateInfo_.algorithm->Update({data + offset, tlv.length}, tlv.length);
    offset += tlv.length;

    return true;
}

bool BinChunkUpdate::ProcessPartitionData(uint8_t *data, uint32_t &len, uint32_t &offset, PartitionHashInfo &hashInfos)
{
    PkgTlvHH tlv;

    for (auto i = 0; i < updateInfo_.patitionNum; i++) {
        // 读取分区名称
        if ((len - offset) < sizeof(PkgTlvHH)) {
            LOG(DEBUG) << "needNewData";
            updateInfo_.needNewData = true;
            return false;
        }

        tlv.type = ReadLE16(data + offset);
        tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
        updateInfo_.algorithm->Update({data + offset, sizeof(PkgTlvHH)}, sizeof(PkgTlvHH));
        offset += sizeof(PkgTlvHH);

        std::string patition;
        PkgFileImpl::ConvertBufferToString(patition, {data + offset, tlv.length});
        LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash patition:" << patition;
        updateInfo_.algorithm->Update({data + offset, tlv.length}, tlv.length);
        offset += tlv.length;

        // 读取哈希值
        std::string hashBuf;
        if (!ReadHash(data, len, offset, hashBuf)) {
            return false;
        }

        hashInfos.hashValues[patition] = hashBuf;

        // 读取数据长度
        if (!ReadDataLength(data, len, offset, patition, hashInfos.dataLenInfos)) {
            return false;
        }
    }

    return true;
}

bool BinChunkUpdate::ProcessSignature(uint8_t *data, uint32_t &len, uint32_t &offset,
    std::vector<uint8_t> &signData)
{
    PkgTlvHI tlv2;

    if ((len - offset) < sizeof(PkgTlvHI)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }
    tlv2.type = ReadLE16(data + offset);
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash tlv2.type:" << tlv2.type;
    tlv2.length = ReadLE32(data + offset + sizeof(uint16_t));
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHash tlv2.length:" << tlv2.length;
    offset += sizeof(PkgTlvHI);
    if ((len - offset) < tlv2.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }
    signData.assign(data + offset, data + offset + tlv2.length);
    offset += tlv2.length;

    return true;
}

bool BinChunkUpdate::ReadHash(uint8_t *data, uint32_t &len, uint32_t &offset, std::string &hashBuf)
{
    PkgTlvHH tlv;
    
    if ((len - offset) < sizeof(PkgTlvHH)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv.type = ReadLE16(data + offset);
    tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
    updateInfo_.algorithm->Update({data + offset, sizeof(PkgTlvHH)}, sizeof(PkgTlvHH));
    offset += sizeof(PkgTlvHH);

    if ((len - offset) < tlv.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    PkgFileImpl::ConvertBufferToString(hashBuf, {data + offset, tlv.length});
    updateInfo_.algorithm->Update({data + offset, tlv.length}, tlv.length);
    offset += tlv.length;

    return true;
}

bool BinChunkUpdate::ReadDataLength(uint8_t *data, uint32_t &len, uint32_t &offset,
    const std::string &patition, std::map<std::string, uint64_t> &dataLenInfos)
{
    PkgTlvHH tlv;
    
    if ((len - offset) < sizeof(PkgTlvHH)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv.type = ReadLE16(data + offset);
    tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
    updateInfo_.algorithm->Update({data + offset, sizeof(PkgTlvHH)}, sizeof(PkgTlvHH));
    offset += sizeof(PkgTlvHH);

    if ((len - offset) < tlv.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    uint64_t dataLen = ReadLE64(data + offset);
    updateInfo_.algorithm->Update({data + offset, tlv.length}, tlv.length);
    offset += tlv.length;

    dataLenInfos[patition] = dataLen;

    return true;
}

bool BinChunkUpdate::VerifySignature(std::vector<uint8_t> &signData)
{
    // 计算最终的哈希值
    PkgBuffer digest(DigestAlgorithm::GetDigestLen(PKG_DIGEST_TYPE_SHA256));
    updateInfo_.algorithm->Final(digest);

    // 获取用于验证签名的算法
    SignAlgorithm::SignAlgorithmPtr signAlgorithm =
        PkgAlgorithmFactory::GetVerifyAlgorithm(Utils::GetCertName(), PKG_DIGEST_TYPE_SHA256);
    if (signAlgorithm == nullptr) {
        LOG(ERROR) << "BinChunkUpdate Invalid sign algo";
        return false;
    }

    // 验证签名
    if (signAlgorithm->VerifyDigest(digest.data, signData) != 0) {
        LOG(ERROR) << "BinChunkUpdate VerifyDigest failed";
        return false;
    }

    LOG(INFO) << "BinChunkUpdate VerifyDigest success";
    return true;
}

bool BinChunkUpdate::VerifyPartitionHashes(const PartitionHashInfo &hashInfos,
    std::vector<std::future<bool>> &futures)
{
    // 使用异步任务来验证每个分区的哈希
    for (const auto &pair : hashInfos.hashValues) {
        futures.push_back(std::async(std::launch::async, &BinChunkUpdate::VerifyPartitionHash, this, pair.first,
                                     pair.second, std::ref(hashInfos.dataLenInfos)));
    }

    // 等待所有异步任务完成
    for (auto &future : futures) {
        if (!future.get()) {
            LOG(ERROR) << "BinChunkUpdate partition verify hash fail";
            return false;
        }
    }

    return true;
}

UpdateResultCode BinChunkUpdate::UpdateBinOther(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinOther enter";
    return STREAM_UPDATE_FAILURE;
}

UpdateResultCode BinChunkUpdate::UpdateBinHead(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHead enter";
    PkgManager::StreamPtr stream = nullptr;
    PkgBuffer buffer(data, len);
    if (auto ret = pkgManager_->CreatePkgStream(stream, UPDATE_BIN_FILE, buffer); ret != PKG_SUCCESS) {
        LOG(ERROR) << "ParseHead failed";
        return STREAM_UPDATE_FAILURE;
    }

    if (auto ret = pkgManager_->LoadPackageWithStream(UPDATE_BIN_FILE, Utils::GetCertName(), updateInfo_.componentNames,
        PkgFile::PKG_TYPE_UPGRADE, stream); ret != PKG_SUCCESS) {
        LOG(ERROR) << "LoadPackage fail ret :" << ret;
        return STREAM_UPDATE_FAILURE;
    }

    const PkgInfo *pkgInfo = pkgManager_->GetPackageInfo(UPDATE_BIN_FILE);
    if (pkgInfo == nullptr || pkgInfo->updateFileHeadLen == 0 || len < pkgInfo->updateFileHeadLen) {
        LOG(ERROR) << "GetPackageInfo failed";
        return STREAM_UPDATE_FAILURE;
    }
    offset_ = pkgInfo->updateFileHeadLen;
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinHead exit pkgInfo->updateFileHeadLen:" << pkgInfo->updateFileHeadLen;
    return STREAM_UPDATE_SUCCESS;
}

UpdateResultCode BinChunkUpdate::ChunkInstallPreWrite(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::ChunkInstallPreWrite enter";

    if (!ReadPartitionData(data, len)) {
        return STREAM_UPDATE_SUCCESS;
    }

    if (!OpenDevPath()) {
        return STREAM_UPDATE_FAILURE;
    }

    if (!InitTransferParams()) {
        return STREAM_UPDATE_FAILURE;
    }

    updateInfo_.updateStep = CHUNK_INSTALL_STEP_DO;
    return STREAM_UPDATE_SUCCESS;
}

bool BinChunkUpdate::ReadPartitionData(uint8_t *data, uint32_t &len)
{
    PkgTlvHH tlv;
    uint32_t offset = offset_;

    if ((len - offset) < sizeof(PkgTlvHH)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv.type = ReadLE16(data + offset);
    tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
    offset += sizeof(PkgTlvHH);

    if ((len - offset) < tlv.length) {
        updateInfo_.needNewData = true;
        return false;
    }

    updateInfo_.curPartition = "";
    PkgFileImpl::ConvertBufferToString(updateInfo_.curPartition, {data + offset, tlv.length});
    LOG(DEBUG) << "PreWriteBin name " << updateInfo_.curPartition;
    
    return true;
}

bool BinChunkUpdate::OpenDevPath()
{
    #ifndef UPDATER_UT
    std::string devPath = GetBlockDeviceByMountPoint(updateInfo_.curPartition);
    std::string srcPath;
    std::string targetPath;
    srcPath = devPath;
    targetPath = devPath;

    if (updateInfo_.curPartition != "/userdata") {
        std::string suffix = Utils::GetUpdateSuffix();
        targetPath += suffix;
        suffix = Utils::GetUpdateActiveSuffix();
        srcPath += suffix;
    }

    LOG(DEBUG) << "ChunkInstallPreWrite curPartition:" << updateInfo_.curPartition
        << " srcPath:" << srcPath << " targetPath:" << targetPath;

    updateInfo_.srcFd = open(srcPath.c_str(), O_RDWR | O_LARGEFILE);
    if (updateInfo_.srcFd < 0) {
        LOG(ERROR) << "open srcPath error";
        return false;
    }

    updateInfo_.targetFd = open(targetPath.c_str(), O_RDWR | O_LARGEFILE);
    if (updateInfo_.targetFd < 0) {
        LOG(ERROR) << "open targetPath error";
        return false;
    }
    #else
    int fd = open("/data/updater/test.txt", O_RDWR | O_LARGEFILE | O_CREAT);
    if (fd < 0) {
        LOG(ERROR) << "open test file error";
        return false;
    }
    updateInfo_.srcFd = fd;
    updateInfo_.targetFd = fd;
    #endif
    return true;
}

bool BinChunkUpdate::InitTransferParams()
{
    updateInfo_.transferParams = std::make_unique<TransferParams>();
    updateInfo_.transferParams->storeBase = std::string("/data/updater/") + updateInfo_.curPartition + "_tmp";
    updateInfo_.transferParams->canWrite = true;

    int32_t ret = Store::CreateNewSpace(updateInfo_.transferParams->storeBase, true);
    if (ret == -1) {
        LOG(ERROR) << "Error to create new store space";
        return false;
    }

    return true;
}

UpdateResultCode BinChunkUpdate::ChunkInstallDoWrite(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::ChunkInstallDoWrite enter";
    uint32_t offset = offset_;

    // 处理安装分区
    if (!ProcessPartition(data, len, offset)) {
        return STREAM_UPDATE_SUCCESS;
    }

    // 处理安装命令
    if (!ProcessCmdLine(data, len, offset)) {
        return STREAM_UPDATE_SUCCESS;
    }

    // 处理安装数据
    if (!ProcessInstallData(data, len, offset)) {
        return STREAM_UPDATE_SUCCESS;
    }

    // 安装数据
    if (!ExecuteCmdLine()) {
        return STREAM_UPDATE_FAILURE;
    }

    offset_ = offset;
    return STREAM_UPDATE_SUCCESS;
}

bool BinChunkUpdate::ProcessPartition(uint8_t *data, uint32_t &len, uint32_t &offset)
{
    PkgTlvHH tlv;
    if ((len - offset) < sizeof(PkgTlvHH)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv.type = ReadLE16(data + offset);
    LOG(DEBUG) << "tlv.type:" << tlv.type;
    if (tlv.type == BIN_UPDATE_HASH_TIP) {
        updateInfo_.updateStep = CHUNK_INSTALL_STEP_POST;
        return false;
    }

    tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
    LOG(DEBUG) << "tlv.length:" << tlv.length;
    offset += sizeof(PkgTlvHH);

    if ((len - offset) < tlv.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    std::string partition;
    PkgFileImpl::ConvertBufferToString(partition, {data + offset, tlv.length});
    LOG(DEBUG) << "partition:" << partition;
    offset += tlv.length;

    if (partition != updateInfo_.curPartition) {
        updateInfo_.updateStep = CHUNK_INSTALL_STEP_POST;
        return false;
    }

    return true;
}

bool BinChunkUpdate::ProcessCmdLine(uint8_t *data, uint32_t &len, uint32_t &offset)
{
    PkgTlvHH tlv;
    if ((len - offset) < sizeof(PkgTlvHH)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv.type = ReadLE16(data + offset);
    LOG(DEBUG) << "tlv.type:" << tlv.type;
    tlv.length = ReadLE16(data + offset + sizeof(uint16_t));
    LOG(DEBUG) << "tlv.length:" << tlv.length;
    offset += sizeof(PkgTlvHH);

    if ((len - offset) < tlv.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    updateInfo_.cmdLine = "";
    PkgFileImpl::ConvertBufferToString(updateInfo_.cmdLine, {data + offset, tlv.length});
    LOG(DEBUG) << "cmdLine:" << updateInfo_.cmdLine;
    offset += tlv.length;

    return true;
}

bool BinChunkUpdate::ProcessInstallData(uint8_t *data, uint32_t &len, uint32_t &offset)
{
    PkgTlvHI tlv2;
    if ((len - offset) < sizeof(PkgTlvHI)) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    tlv2.type = ReadLE16(data + offset);
    LOG(DEBUG) << "tlv2.type:" << tlv2.type;
    tlv2.length = ReadLE32(data + offset + sizeof(uint16_t));
    LOG(DEBUG) << "tlv2.length:" << tlv2.length;
    offset += sizeof(PkgTlvHI);

    if ((len - offset) < tlv2.length) {
        LOG(DEBUG) << "needNewData";
        updateInfo_.needNewData = true;
        return false;
    }

    updateInfo_.transferParams->dataBuffer = data + offset;
    updateInfo_.transferParams->dataBufferSize = tlv2.length;

    offset += tlv2.length;
    return true;
}

bool BinChunkUpdate::ExecuteCmdLine()
{
    std::shared_ptr<Command> cmd = std::make_shared<Command>(updateInfo_.transferParams.get());
    cmd->SetIsStreamCmd(true);
    cmd->SetSrcFileDescriptor(updateInfo_.srcFd);
    cmd->SetTargetFileDescriptor(updateInfo_.targetFd);
    cmd->Init(updateInfo_.cmdLine);

    CommandFunction *cf = CommandFunctionFactory::GetInstance().GetCommandFunction(cmd->GetCommandHead());
    CommandResult ret = cf->Execute(const_cast<Command &>(*cmd.get()));
    if (SUCCESS != ret) {
        LOG(ERROR) << "cf->Execute fail";
        return false;
    }

    LOG(INFO) << "cf->Execute SUCCESS";
    return true;
}

UpdateResultCode BinChunkUpdate::ChunkInstallPostWrite(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::ChunkInstallPostWrite enter";
    if (updateInfo_.srcFd > 0) {
        fsync(updateInfo_.srcFd);
        close(updateInfo_.srcFd);
    }
    if (updateInfo_.targetFd > 0) {
        fsync(updateInfo_.targetFd);
        close(updateInfo_.targetFd);
    }
    Store::DoFreeSpace(updateInfo_.transferParams->storeBase);
    (void)Utils::DeleteFile(updateInfo_.transferParams->storeBase);
    updateInfo_.updateStep = CHUNK_INSTALL_STEP_PRE;
    return STREAM_UPDATE_SUCCESS;
}

UpdateResultCode BinChunkUpdate::UpdateBinData(uint8_t *data, uint32_t &len)
{
    LOG(DEBUG) << "BinChunkUpdate::UpdateBinData enter";
    UpdateResultCode ret;
    do {
        auto it = chunkInstallProcess_.find(updateInfo_.updateStep);
        if (it == chunkInstallProcess_.end() || it->second == nullptr) {
            LOG(ERROR) << "cannot find " << updateInfo_.updateStep;
            return STREAM_UPDATE_FAILURE;
        }
        ret = it->second(data, len);
    } while (!(updateInfo_.needNewData || ret == STREAM_UPDATE_FAILURE ||
        updateInfo_.updateStep == CHUNK_INSTALL_STEP_PRE));

    LOG(DEBUG) << "BinChunkUpdate::UpdateBinData exit";
    return ret;
}

// 计算文件哈希值的函数
std::string BinChunkUpdate::ComputeFileHash(const std::string &partitionName,
                                            const std::map<std::string, uint64_t> &dataLenInfos)
{
    LOG(DEBUG) << "BinChunkUpdate::ComputeFileHash enter";
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);

    auto it = dataLenInfos.find(partitionName);
    if (it == dataLenInfos.end()) {
        LOG(ERROR) << "ComputeFileHash cannot find dataLenInfos " << partitionName;
        return "";
    }
    uint64_t dataLen = it->second;
    #ifndef UPDATER_UT
    std::string devPath = GetBlockDeviceByMountPoint(partitionName);
    if (partitionName != "/userdata") {
        std::string suffix = Utils::GetUpdateSuffix();
        devPath += suffix;
    }
    #else
    std::string devPath = "/data/updater/test.txt";
    #endif
    std::ifstream file(devPath, std::ios::binary);
    if (!file) {
        LOG(ERROR) << "Failed to open file: " << partitionName;
        return "";
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    while (dataLen > 0) {
        std::vector<char> buffer(HASH_BUFFER_SIZE);
        size_t size = std::min(static_cast<size_t>(dataLen), buffer.size());
        file.read(buffer.data(), size);
        SHA256_Update(&sha256, buffer.data(), file.gcount());
        dataLen -= file.gcount();
    }

    SHA256_Final(hash.data(), &sha256);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    const int kByteWidth = 2;
    for (auto byte : hash) {
        oss << std::setw(kByteWidth) << static_cast<int>(byte);
    }

    return oss.str();
}

// 比对哈希值的函数
bool BinChunkUpdate::VerifyPartitionHash(const std::string &partitionName, const std::string &expectedHash,
                                         const std::map<std::string, uint64_t> &dataLenInfos)
{
    LOG(INFO) << "BinChunkUpdate::VerifyPartitionHash enter";
    std::string actualHash = ComputeFileHash(partitionName, dataLenInfos);

    LOG(INFO) << "actualHash:" << actualHash << " expectedHash:" << expectedHash;

    if (actualHash != expectedHash) {
        LOG(ERROR) << "Error verifying hash for partition " << partitionName;
        return false;
    }
    return true;
}

} // namespace Updater
