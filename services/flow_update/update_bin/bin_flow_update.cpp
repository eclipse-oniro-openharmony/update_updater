/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "bin_flow_update.h"

#include <algorithm>
#include <functional>

#include "log.h"
#include "scope_guard.h"
#include "pkg_manager/pkg_stream.h"
#include "pkg_package/pkg_pkgfile.h"
#include "securec.h"
#include "utils.h"

namespace Updater {
using namespace Uscript;
using namespace Hpackage;
using namespace std::placeholders;

constexpr const char *UPDATE_BIN_FILE = "update.bin";

BinFlowUpdate::BinFlowUpdate(uint32_t maxBufSize)
{
    static_assert(maxBufSize == SETV0AL, "maxBufSize can not be 0");
    maxBufSize_ = maxBufSize;
    buffer_ = new uint8_t[maxBufSize_];
    updateBinProcess_.emplace(BIN_UPDATE_STEP_PRE, std::bind(&BinFlowUpdate::BinUpdatePreWrite, this, _1, _2));
    updateBinProcess_.emplace(BIN_UPDATE_STEP_DO, std::bind(&BinFlowUpdate::BinUpdateDoWrite, this, _1, _2));
    updateBinProcess_.emplace(BIN_UPDATE_STEP_POST, std::bind(&BinFlowUpdate::BinUpdatePostWrite, this, _1, _2));
    pkgManager_ = PkgManager::CreatePackageInstance();
}

BinFlowUpdate::~BinFlowUpdate()
{
    if (buffer_ != nullptr) {
        delete[] buffer_;
        buffer_ = nullptr;
    }

    if (pkgManager_ != nullptr) {
        PkgManager::ReleasePackageInstance(pkgManager_);
    }
}

int32_t BinFlowUpdate::StartBinFlowUpdate(uint8_t *data, uint32_t len)
{
    if (data == nullptr || len == 0) {
        LOG(ERROR) << "para error";
        return -1;
    }
    uint32_t remainLen = len;
    while (remainLen > 0) {
        // 1: add remained data
        if (!AddRemainData(data + len - remainLen, remainLen)) {
            LOG(ERROR) << "AddRemainData error";
            return -1;
        }

        // 2: parse head
        if (!headInit_ && UpdateBinHead(buffer_, curlen_) != 0) {
            LOG(ERROR) << "ParseHead error";
            return -1;
        }

        // 3: process data
        updateInfo_.needNewData = false;
        while (curlen_ > 0 && !updateInfo_.needNewData) {
            if (auto ret = UpdateBinData(buffer_, curlen_); ret != 0) {
                LOG(ERROR) << "ProcessData error";
                return ret;
            }
        }
    }
    return 0;
}

bool BinFlowUpdate::AddRemainData(uint8_t *data, uint32_t &len)
{
    uint32_t copySize = std::min(static_cast<size_t>(len), static_cast<size_t>(maxBufSize_ - curlen_));
    if (memcpy_s(buffer_ + curlen_, maxBufSize_, data, copySize) != EOK) {
        LOG(ERROR) << "AddRemainData memcpy failed" << " : " << strerror(errno);
        return false;
    }
    curlen_ += copySize;
    len -= copySize;
    return true;
}


uint32_t BinFlowUpdate::UpdateBinHead(uint8_t *data, uint32_t &len)
{
    PkgManager::StreamPtr stream = nullptr;
    PkgBuffer buffer(data, len);
    if (auto ret = pkgManager_->CreatePkgStream(stream, UPDATE_BIN_FILE, buffer); ret != PKG_SUCCESS) {
        LOG(ERROR) << "ParseHead failed";
        return -1;
    }

    ON_SCOPE_EXIT(closeStream) {
        pkgManager_->ClosePkgStream(stream);
    };

    if (auto ret = pkgManager_->LoadPackageWithStream(UPDATE_BIN_FILE, Utils::GetCertName(), 
        updateInfo_.componentNames, PkgFile::PKG_TYPE_UPGRADE, stream); ret != PKG_SUCCESS) {
        LOG(ERROR) << "LoadPackage fail ret :"<< ret;
        return ret;
    }

    const PkgInfo *pkgInfo = pkgManager_->GetPackageInfo(UPDATE_BIN_FILE);
    if (pkgInfo == nullptr || pkgInfo->updateFileHeadLen == 0 || len < pkgInfo->updateFileHeadLen) {
        LOG(ERROR) << "GetPackageInfo failed";
        return -1;
    }

    if (len > pkgInfo->updateFileHeadLen &&
        memmove_s(data, len, data + pkgInfo->updateFileHeadLen, len - pkgInfo->updateFileHeadLen) != EOK) {
        LOG(ERROR) << "memmove failed";
        return -1;
    }

    len -= pkgInfo->updateFileHeadLen;
    headInit_ = true;
    return 0;
}

std::unique_ptr<DataWriter> BinFlowUpdate::GetDataWriter(const std::string &partition)
{
    static std::string lastPartition {};
    static int lastIndex = 0;
    if (lastPartition == partition) {
        lastIndex++;
        lastPartition = partition + std::to_string(lastIndex);
    } else {
        lastPartition = partition;
        lastIndex = 0;
    }
    const std::string writePath = "/data/updater" + lastPartition;
    FILE *pFile = fopen(writePath.c_str(), "w+");
    if (pFile != nullptr) {
        uint8_t data[1] {};
        fwrite(data, 1, sizeof(data), pFile);
        fclose(pFile);
    }
    LOG(INFO) << "GetDataWriter writePath " << writePath.c_str();
    return DataWriter::CreateDataWriter(WRITE_RAW, writePath, static_cast<uint64_t>(0));
}

int BinFlowUpdate::BinUpdatePreWrite(uint8_t *data, uint32_t &len)
{
    if (procCompIndex_ > updateInfo_.componentNames.size()) {
        LOG(ERROR) << "PreWriteBin index error cur:" << procCompIndex_ << " max:" << updateInfo_.componentNames.size();
        return -1;
    }

    LOG(INFO) << "PreWriteBin name "<< updateInfo_.componentNames[procCompIndex_];
    updateInfo_.imageWriteLen = 0;
    updateInfo_.writer = GetDataWriter(updateInfo_.componentNames[procCompIndex_]);
    if (updateInfo_.writer == nullptr) {
        LOG(ERROR) << "GetDataWriter error";
        return -1;
    }

    updateInfo_.info = pkgManager_->GetFileInfo(updateInfo_.componentNames[procCompIndex_]);
    if (updateInfo_.info == nullptr) {
        LOG(ERROR) << ("Can not get file info");
        return -1;
    }

    updateInfo_.updateStep = BIN_UPDATE_STEP_DO;
    return 0;
}

int BinFlowUpdate::BinUpdateDoWrite(uint8_t *data, uint32_t &len)
{
    size_t writeLen = std::min(static_cast<size_t>(updateInfo_.info->unpackedSize - updateInfo_.imageWriteLen),
        static_cast<size_t>(len));
    LOG(INFO) << "DoWriteBin len " << len << " unpackedSize " << updateInfo_.info->unpackedSize << " already write " <<
        updateInfo_.imageWriteLen;

    // sha256, < 4M , updateInfo_.needNewData = true;
    if (!updateInfo_.writer->Write(data, writeLen, nullptr)) {
        LOG(ERROR) << "Write failed";
        return -1;
    }

    if (memmove_s(data, len, data + writeLen, len - writeLen) != EOK) {
        LOG(ERROR) << "memmove failed";
        return -1;
    }

    updateInfo_.imageWriteLen += writeLen;
    len -= writeLen;
    if (updateInfo_.imageWriteLen == updateInfo_.info->unpackedSize) {
        LOG(INFO) << "DoWriteBin all len " << updateInfo_.imageWriteLen;
        updateInfo_.updateStep = BIN_UPDATE_STEP_POST;
    } else if (updateInfo_.imageWriteLen > updateInfo_.info->unpackedSize) {
        LOG(INFO) << "DoWriteBin write len " << updateInfo_.imageWriteLen <<
            " all len " << updateInfo_.info->unpackedSize;
        return -1;
    }

    return 0;
}

int BinFlowUpdate::BinUpdatePostWrite(uint8_t *data, uint32_t &len)
{
    procCompIndex_++;
    updateInfo_.updateStep = BIN_UPDATE_STEP_PRE;
    return 0;
}

int BinFlowUpdate::UpdateBinData(uint8_t *data, uint32_t &len)
{
    auto it = updateBinProcess_.find(updateInfo_.updateStep);
    if (it == updateBinProcess_.end() || it->second == nullptr) {
        LOG(ERROR) << "cannot find " << updateInfo_.updateStep;
        return -1;
    }

    return it->second(data, len);
}
} // Updater
