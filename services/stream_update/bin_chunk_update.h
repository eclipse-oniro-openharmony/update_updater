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
#ifndef BIN_CHUNK_UPDATE
#define BIN_CHUNK_UPDATE

#include <cstdio>
#include <functional>
#include <string>
#include <sys/wait.h>
#include <vector>
#include <map>
#include <future>

#include "package/pkg_manager.h"
#include "applypatch/transfer_manager.h"
#include "pkg_package/pkg_pkgfile.h"
#include "pkg_manager/pkg_stream.h"

namespace Updater {

struct __attribute__((packed)) PkgTlvHH {
    uint16_t type;
    uint16_t length;
};

struct __attribute__((packed)) PkgTlvHI {
    uint16_t type;
    uint32_t length;
};

using UpdateResultCode = enum {
    STREAM_UPDATE_SUCCESS = 0,
    STREAM_UPDATE_FAILURE = 1,
    STREAM_UPDATE_COMPLETE = 2
};

using BinUpdateTip = enum {
    BIN_UPDATE_ZIP_TIP = 0xaa,
    BIN_UPDATE_HEAD_TIP = 0x01,
    BIN_UPDATE_DATA_TIP = 0x12,
    BIN_UPDATE_HASH_TIP = 0x16
};

using ChunkInstallStep = enum {
    CHUNK_INSTALL_STEP_PRE = 0,
    CHUNK_INSTALL_STEP_DO,
    CHUNK_INSTALL_STEP_POST
};

struct BinChunkUpdateInfo {
    std::vector<std::string> componentNames;
    bool needNewData = false;
    ChunkInstallStep updateStep = CHUNK_INSTALL_STEP_PRE;
    int srcFd;
    int targetFd;
    std::unique_ptr<TransferParams> transferParams;
    std::string curPartition;
    std::string cmdLine;
    int patitionNum;
    Hpackage::DigestAlgorithm::DigestAlgorithmPtr algorithm;
};

struct PartitionHashInfo {
    std::map<std::string, std::string> hashValues;
    std::map<std::string, uint64_t> dataLenInfos;
};

class BinChunkUpdate {
public:
    explicit BinChunkUpdate(uint32_t maxBufSize);
    virtual ~BinChunkUpdate();
    UpdateResultCode StartBinChunkUpdate(const uint8_t *data, uint32_t len, uint32_t &dealLen);
private:
    UpdateResultCode ProcessBufferData();

    UpdateResultCode ChunkInstallPreWrite(uint8_t *data, uint32_t &len);
    UpdateResultCode ChunkInstallDoWrite(uint8_t *data, uint32_t &len);
    UpdateResultCode ChunkInstallPostWrite(uint8_t *data, uint32_t &len);

    UpdateResultCode UpdateBinHead(uint8_t *data, uint32_t &len);
    UpdateResultCode UpdateBinData(uint8_t *data, uint32_t &len);
    UpdateResultCode UpdateBinHash(uint8_t *data, uint32_t &len);
    UpdateResultCode UpdateBinOther(uint8_t *data, uint32_t &len);

    bool AddRemainData(const uint8_t *data, uint32_t &len);
    bool MoveRemainingData();

    bool ReadPartitionData(uint8_t *data, uint32_t &len);
    bool OpenDevPath();
    bool InitTransferParams();

    // 处理安装分区
    bool ProcessPartition(uint8_t *data, uint32_t &len, uint32_t &offset);
    // 处理安装命令
    bool ProcessCmdLine(uint8_t *data, uint32_t &len, uint32_t &offset);
    // 处理安装数据
    bool ProcessInstallData(uint8_t *data, uint32_t &len, uint32_t &offset);
    // 执行安装命令
    bool ExecuteCmdLine();

    bool ProcessPartitionNum(uint8_t *data, uint32_t &len, uint32_t &offset);

    bool ProcessPartitionData(uint8_t *data, uint32_t &len, uint32_t &offset, PartitionHashInfo &hashInfos);

    bool ProcessSignature(uint8_t *data, uint32_t &len, uint32_t &offset,
        std::vector<uint8_t> &signData);

    bool ReadHash(uint8_t *data, uint32_t &len, uint32_t &offset, std::string &hashBuf);

    bool ReadDataLength(uint8_t *data, uint32_t &len, uint32_t &offset,
        const std::string &patition, std::map<std::string, uint64_t> &dataLenInfos);

    bool VerifySignature(std::vector<uint8_t> &signData);

    bool VerifyPartitionHashes(const PartitionHashInfo &hashInfos, std::vector<std::future<bool>> &futures);

    bool VerifyPartitionHash(const std::string& partitionName, const std::string &expectedHash,
        const std::map<std::string, uint64_t> &dataLenInfos);
    std::string ComputeFileHash(const std::string& partitionName, const std::map<std::string, uint64_t> &dataLenInfos);

    Hpackage::PkgManager::PkgManagerPtr pkgManager_;
    uint8_t *buffer_ = nullptr;
    uint32_t maxBufSize_ = 0;
    uint32_t curlen_ = 0;
    uint32_t offset_ = 0;

    std::map<ChunkInstallStep, std::function<UpdateResultCode (uint8_t *, uint32_t &)>> chunkInstallProcess_;
    BinChunkUpdateInfo updateInfo_ {};
};
} // Updater
#endif /* BIN_FLOW_UPDATE */
