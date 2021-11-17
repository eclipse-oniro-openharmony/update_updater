/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#include "daemon_updater.h"
#include "daemon_common.h"
#include "flashd/flashd.h"
#include "flash_utils.h"
#include "flash_define.h"

namespace Hdc {
DaemonUpdater::DaemonUpdater(HTaskInfo hTaskInfo) : HdcTransferBase(hTaskInfo)
{
    commandBegin = CMD_UPDATER_BEGIN;
    commandData = CMD_UPDATER_DATA;
}

DaemonUpdater::~DaemonUpdater()
{
    WRITE_LOG(LOG_DEBUG, "~DaemonUpdater refCount %d", refCount);
}

bool DaemonUpdater::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
#ifndef UPDATER_UT
    if (!HdcTransferBase::CommandDispatch(command, payload, payloadSize)) {
        return false;
    }
#endif
    if (flashHandle_ == nullptr) {
        int ret = flashd::CreateFlashInstance(&flashHandle_, errorMsg_,
            [&](uint32_t type, size_t dataLen, const void *context) {
                SendProgress(dataLen);
            });
        FLASHDAEMON_CHECK(ret == 0, AsyncUpdateFinish(command, -1, errorMsg_);
            return false, "Faild to create flashd");
    }
    switch (command) {
        case CMD_UPDATER_DATA: {
            const uint8_t payloadPrefixReserve = 64;
            string serialStrring((char *)payload, payloadPrefixReserve);
            TransferPayload pld {};
            SerialStruct::ParseFromString(pld, serialStrring);
#ifdef UPDATER_UT
            pld.uncompressSize = pld.compressSize;
#endif
            SendProgress(pld.uncompressSize);
            break;
        }
        case CMD_UPDATER_CHECK: {
            ProcessUpdateCheck(payload, payloadSize);
            break;
        }
        case CMD_UPDATER_ERASE: {
            std::string param(reinterpret_cast<char *>(payload), payloadSize);
            RunUpdateShell(flashd::UPDATEMOD_ERASE, param, "");
            TaskFinish();
            break;
        }
        case CMD_UPDATER_FORMAT: {
            std::string param(reinterpret_cast<char *>(payload), payloadSize);
            RunUpdateShell(flashd::UPDATEMOD_FORMAT, param, "");
            TaskFinish();
            break;
        }
        default:
            WRITE_LOG(LOG_FATAL, "CommandDispatch command %d", command);
            return false;
    }
    return true;
};

void DaemonUpdater::ProcessUpdateCheck(const uint8_t *payload, const int payloadSize)
{
    uint64_t realSize = 0;
    int ret = memcpy_s(&realSize, sizeof(realSize), payload, sizeof(realSize));
    FLASHDAEMON_CHECK(ret == 0, return, "Faild to memcpy");
    string bufString((char *)payload + sizeof(realSize), payloadSize - sizeof(realSize));
    SerialStruct::ParseFromString(ctxNow.transferConfig, bufString);
    ctxNow.master = false;
    ctxNow.fsOpenReq.data = &ctxNow;
    ctxNow.transferBegin = Base::GetRuntimeMSec();
    ctxNow.fileSize = ctxNow.transferConfig.fileSize;
    percentage_ = -1;

    WRITE_LOG(LOG_DEBUG, "ProcessUpdateCheck local function %s size %llu realSize %llu",
        ctxNow.transferConfig.functionName.c_str(), ctxNow.fileSize, realSize);
    uint8_t type = flashd::UPDATEMOD_FLASH;
    if (ctxNow.transferConfig.functionName == CMDSTR_UPDATE_SYSTEM) {
        type = flashd::UPDATEMOD_UPDATE;
        totalSize_ = static_cast<double>(ctxNow.fileSize);
        if (!MatchPackageExtendName(ctxNow.transferConfig.optionalName, ".bin")) {
            totalSize_ += static_cast<double>(realSize); // for decode from zip
        }
        totalSize_ += static_cast<double>(realSize); // for verify
        totalSize_ += static_cast<double>(realSize); // for read to partition
        totalSize_ += static_cast<double>(realSize); // for write partition
    } else if (ctxNow.transferConfig.functionName == CMDSTR_FLASH_PARTITION) {
        totalSize_ = static_cast<double>(ctxNow.fileSize);
        type = flashd::UPDATEMOD_FLASH;
    } else {
        WRITE_LOG(LOG_FATAL, "ProcessUpdateCheck local function %s size %lu realSize %lu",
            ctxNow.transferConfig.functionName.c_str(), ctxNow.fileSize, realSize);
        AsyncUpdateFinish(type, -1, "Invalid command");
        return;
    }
    ctxNow.localPath = ctxNow.transferConfig.optionalName;
    ret = flashd::DoUpdaterPrepare(flashHandle_, type, ctxNow.transferConfig.options, ctxNow.localPath);
    if (ret == 0) {
        refCount++;
        WRITE_LOG(LOG_DEBUG, "ProcessUpdateCheck localPath %s", ctxNow.localPath.c_str());
#ifndef UPDATER_UT
        uv_fs_open(loopTask, &ctxNow.fsOpenReq, ctxNow.localPath.c_str(),
            UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY, S_IRUSR, OnFileOpen);
#endif
    }
    FLASHDAEMON_CHECK(ret == 0, AsyncUpdateFinish(type, ret, errorMsg_), "Faild to prepare for %d", type);
}

void DaemonUpdater::RunUpdateShell(uint8_t type, const std::string &options, const std::string &package)
{
    int ret = flashd::DoUpdaterFlash(flashHandle_, type, options, package);
    AsyncUpdateFinish(type, ret, errorMsg_);
}

void DaemonUpdater::SendProgress(size_t dataLen)
{
    currSize_ += dataLen;
    int32_t percentage = static_cast<int32_t>(currSize_ * (flashd::PERCENT_FINISH - 1) / totalSize_);
    if (static_cast<uint32_t>(percentage) >= flashd::PERCENT_FINISH) {
        WRITE_LOG(LOG_DEBUG, "SendProgress %lf percentage %d", currSize_, percentage);
        return;
    }
    if (percentage_ < percentage) {
        percentage_ = percentage;
        WRITE_LOG(LOG_DEBUG, "SendProgress %lf percentage_ %d", currSize_, percentage_);
        SendToAnother(CMD_UPDATER_PROGRESS, (uint8_t *)&percentage, sizeof(uint32_t));
    }
}

void DaemonUpdater::WhenTransferFinish(CtxFile *context)
{
    uint64_t nMSec = Base::GetRuntimeMSec() - context->transferBegin;
    double fRate = static_cast<double>(context->indexIO) / nMSec; // / /1000 * 1000 = 0
    WRITE_LOG(LOG_DEBUG, "File for %s transfer finish Size:%lld time:%lldms rate:%.2lfkB/s",
              ctxNow.transferConfig.functionName.c_str(), context->indexIO, nMSec, fRate);

    int ret = 0;
    uint8_t type = flashd::UPDATEMOD_UPDATE;
    if (ctxNow.transferConfig.functionName == CMDSTR_UPDATE_SYSTEM) {
        type = flashd::UPDATEMOD_UPDATE;
        ret = flashd::DoUpdaterFlash(flashHandle_, type, ctxNow.transferConfig.options, ctxNow.localPath);
    } else if (ctxNow.transferConfig.functionName == CMDSTR_FLASH_PARTITION) {
        type = flashd::UPDATEMOD_FLASH;
    }
    AsyncUpdateFinish(type, ret, errorMsg_);
    TaskFinish();
}

void DaemonUpdater::AsyncUpdateFinish(uint8_t type, int32_t retCode, const string &result)
{
    WRITE_LOG(LOG_DEBUG, "AsyncUpdateFinish retCode %d result %s", retCode, result.c_str());
    uint32_t percentage = (retCode != 0) ? flashd::PERCENT_CLEAR : flashd::PERCENT_FINISH;
    SendToAnother(CMD_UPDATER_PROGRESS, (uint8_t *)&percentage, sizeof(uint32_t));
    (void)flashd::DoUpdaterFinish(flashHandle_, type, ctxNow.localPath);

    string echo = result;
    echo = Base::ReplaceAll(echo, "\n", " ");
    vector<uint8_t> vecBuf;
    vecBuf.push_back(type);
    if (retCode != 0) {
        vecBuf.push_back(MSG_FAIL);
    } else {
        vecBuf.push_back(MSG_OK);
    }
    vecBuf.insert(vecBuf.end(), (uint8_t *)echo.c_str(), (uint8_t *)echo.c_str() + echo.size());
    SendToAnother(CMD_UPDATER_FINISH, vecBuf.data(), vecBuf.size());
}

#ifdef UPDATER_UT
void DaemonUpdater::DoTransferFinish()
{
    WhenTransferFinish(&ctxNow);
}
#endif
} // namespace Hdc