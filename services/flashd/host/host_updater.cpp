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
#include "host_updater.h"
#include "common.h"
#include "transfer.h"
#include "serial_struct.h"

namespace Hdc {
static const int PERCENT_FINISH = 100;
static const uint32_t PERCENT_CLEAR = ((uint32_t)-1);
HostUpdater::HostUpdater(HTaskInfo hTaskInfo) : HdcTransferBase(hTaskInfo)
{
    commandBegin = CMD_UPDATER_BEGIN;
    commandData = CMD_UPDATER_DATA;
}

HostUpdater::~HostUpdater() {}

void HostUpdater::RunQueue(CtxFile &context)
{
    refCount++;
    context.localPath = context.taskQueue.back();
    uv_fs_open(loopTask, &context.fsOpenReq, context.localPath.c_str(), O_RDONLY, 0, OnFileOpen);
    context.master = true;
}

bool HostUpdater::BeginTransfer(CtxFile &context,
    const std::string &function, const char *payload, int minParam, int fileIndex)
{
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(payload, &argc);
    if (argv == nullptr || argc < minParam || fileIndex >= argc) {
        LogMsg(MSG_FAIL, "Invalid param for cmd \"%s\"", function.c_str());
        delete[]((char *)argv);
        return false;
    }
    int maxParam = minParam;
    if (strstr(payload, "-f") != nullptr) {
        maxParam += 1;
    }
    if (argc != maxParam) {
        LogMsg(MSG_FAIL, "Invalid param for cmd \"%s\"", function.c_str());
        delete[]((char *)argv);
        return false;
    }

    context.transferConfig.functionName = function;
    context.transferConfig.options = payload;
    if (strcmp(argv[fileIndex], "-f") == 0) {
        context.localPath = argv[fileIndex + 1];
    } else {
        context.localPath = argv[fileIndex];
    }

    if (MatchPackageExtendName(context.localPath, ".img")) {
        context.transferConfig.compressType = COMPRESS_NONE;
    } else if (function == CMDSTR_FLASH_PARTITION) {
        context.transferConfig.compressType = COMPRESS_NONE;
    } else if (MatchPackageExtendName(context.localPath, ".bin")) {
        context.transferConfig.compressType = COMPRESS_NONE;
    } else if (!(MatchPackageExtendName(context.localPath, ".zip") ||
        MatchPackageExtendName(context.localPath, ".lz4") ||
        MatchPackageExtendName(context.localPath, ".gz2"))) {
        LogMsg(MSG_FAIL, "Invaid file \"%s\" for cmd \"%s\"", context.localPath.c_str(), function.c_str());
        delete[]((char *)argv);
        return false;
    }

    WRITE_LOG(LOG_DEBUG, "BeginTransfer function: %s localPath: %s command: %s ",
        context.transferConfig.functionName.c_str(), context.localPath.c_str(), payload);
    // check path
    bool ret = Base::CheckDirectoryOrPath(context.localPath.c_str(), true, true);
    if (!ret) {
        LogMsg(MSG_FAIL, "Invaid file \"%s\" for cmd \"%s\"", context.localPath.c_str(), function.c_str());
        delete[]((char *)argv);
        return false;
    }
    context.taskQueue.push_back(context.localPath);
    RunQueue(context);
    return true;
}

std::string HostUpdater::GetFileName(const std::string &fileName) const
{
    int32_t pos = fileName.find_last_of('/');
    if (pos < 0) {
        pos = fileName.find_last_of('\\');
    }
    return fileName.substr(pos + 1, fileName.size());
}

void HostUpdater::CheckMaster(CtxFile *context)
{
    uv_fs_t fs;
    Base::ZeroStruct(fs.statbuf);
    uv_fs_fstat(nullptr, &fs, context->fsOpenReq.result, nullptr);
    context->transferConfig.fileSize = fs.statbuf.st_size;
    uv_fs_req_cleanup(&fs);

    WRITE_LOG(LOG_DEBUG, "CheckMaster %s %llu", context->transferConfig.functionName.c_str(), fs.statbuf.st_size);
    context->transferConfig.optionalName = GetFileName(context->localPath);
    std::string bufString = SerialStruct::SerializeToString(context->transferConfig);

    const uint64_t verdorSize = static_cast<uint64_t>(1024 * 1024) * 256;
    const uint64_t systemSize = static_cast<uint64_t>(1024 * 1024) * 1500;
    const uint64_t minSize = static_cast<uint64_t>(1024 * 1024) * 10;
    uint64_t realSize = verdorSize;
    if (fs.statbuf.st_size > minSize) {
        realSize += systemSize;
    }
    std::vector<uint8_t> buffer(sizeof(realSize) + bufString.size());
    int ret = memcpy_s(buffer.data(), buffer.size(), &realSize, sizeof(realSize));
    int ret2 = memcpy_s(buffer.data() + sizeof(realSize), buffer.size(), bufString.c_str(), bufString.size());
    if ((ret == 0) && (ret2 == 0)) {
        SendToAnother(CMD_UPDATER_CHECK, (uint8_t *)buffer.data(), buffer.size());
    }
}

bool HostUpdater::CheckCmd(const std::string &function, const char *payload, int param)
{
    int argc = 0;
    char **argv = Base::SplitCommandToArgs(payload, &argc);
    if (argv == nullptr) {
        LogMsg(MSG_FAIL, "Can not parser cmd \"%s\"", function.c_str());
        return false;
    }
    delete[]((char *)argv);
    if (argc < param) {
        LogMsg(MSG_FAIL, "Invalid param for cmd \"%s\"", function.c_str());
        return false;
    }

    int maxParam = param;
    if (strstr(payload, "-f") != nullptr) {
        maxParam += 1;
    }
    if (strstr(payload, "-t") != nullptr) {
        maxParam += 1;
        maxParam += 1;
    }
    if (argc != maxParam) {
        LogMsg(MSG_FAIL, "Invalid param for cmd \"%s\"", function.c_str());
        return false;
    }
    return true;
}

bool HostUpdater::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    const int cmdFroErase = 2;
    const int cmdFroFormat = 2;
    if (!HdcTransferBase::CommandDispatch(command, payload, payloadSize)) {
        return false;
    }
    bool ret = true;
    switch (command) {
        case CMD_UPDATER_BEGIN: {
            std::string s("  Processing:   0%%");
            bSendProgress = true;
            SendRawData(reinterpret_cast<uint8_t *>(s.data()), s.size());
            break;
        }
        case CMD_UPDATER_UPDATE_INIT:
            ret = BeginTransfer(ctxNow, CMDSTR_UPDATE_SYSTEM, reinterpret_cast<const char *>(payload), 1, 0);
            break;
        case CMD_UPDATER_FLASH_INIT:
            ret = BeginTransfer(ctxNow, CMDSTR_FLASH_PARTITION,
                reinterpret_cast<const char *>(payload), 2, 1); // 2 cmd min param for flash
            break;
        case CMD_UPDATER_FINISH:
            ret = CheckUpdateContinue(command, payload, payloadSize);
            break;
        case CMD_UPDATER_ERASE: {
            if (!CheckCmd(CMDSTR_ERASE_PARTITION, reinterpret_cast<const char *>(payload), cmdFroErase)) {
                return false;
            }
            SendToAnother(CMD_UPDATER_ERASE, payload, payloadSize);
            ctxNow.taskQueue.push_back(reinterpret_cast<char *>(payload));
            break;
        }
        case CMD_UPDATER_FORMAT: {
            if (!CheckCmd(CMDSTR_FORMAT_PARTITION, reinterpret_cast<const char *>(payload), cmdFroFormat)) {
                return false;
            }
            SendToAnother(CMD_UPDATER_FORMAT, payload, payloadSize);
            ctxNow.taskQueue.push_back(reinterpret_cast<char *>(payload));
            break;
        }
        case CMD_UPDATER_PROGRESS:
            if (payloadSize >= (int)sizeof(uint32_t)) {
                ProcessProgress(*(uint32_t *)payload);
            }
            break;
        default:
            break;
    }
    return ret;
}

void HostUpdater::ProcessProgress(uint32_t percentage)
{
    if (!bSendProgress) {
        return;
    }
    std::string backStr = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
    std::string breakStr = "\n";
    WRITE_LOG(LOG_INFO, "ProcessProgress %d", percentage);
    const int bufferSize = 128;
    std::vector<char> buffer(bufferSize);
    if (percentage == PERCENT_CLEAR) { // clear
        SendRawData(reinterpret_cast<uint8_t *>(backStr.data()), backStr.size());
        SendRawData(reinterpret_cast<uint8_t *>(breakStr.data()), breakStr.size());
        bSendProgress = false;
        return;
    }
    int len = sprintf_s(buffer.data(), buffer.size() - 1, "%s  Processing:   %3d%%", backStr.c_str(), percentage);
    if (len <= 0) {
        return;
    }
    SendRawData(reinterpret_cast<uint8_t *>(buffer.data()), len);
    if (percentage == PERCENT_FINISH) {
        SendRawData(reinterpret_cast<uint8_t *>(breakStr.data()), breakStr.size());
        bSendProgress = false;
    }
}

bool HostUpdater::CheckUpdateContinue(const uint16_t command, const uint8_t *payload, int payloadSize)
{
    if (static_cast<size_t>(payloadSize) < sizeof(uint16_t)) {
        return false;
    }
    MessageLevel level = (MessageLevel)payload[1];
    if ((level == MSG_OK) && bSendProgress) {
        ProcessProgress(PERCENT_FINISH);
    }
    std::string info((char*)(payload + sizeof(uint16_t)), payloadSize - sizeof(uint16_t));
    if (!info.empty()) {
        LogMsg(level, "%s", info.c_str());
    }
    WRITE_LOG(LOG_DEBUG, "CheckUpdateContinue %d %s", level, info.c_str());
    ctxNow.taskQueue.pop_back();
    if (singalStop || !ctxNow.taskQueue.size()) {
        return false;
    }
    RunQueue(ctxNow);
    return true;
}
} // namespace Hdc