/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "flashd_define.h"
#include "hdi/client/update_hdi_client.h"
#include "updater/updater.h"
#include "updater/updater_const.h"
using namespace std;
namespace Hdc {
namespace {
constexpr uint8_t PAYLOAD_FIX_RESERVER = 64;
}

std::atomic<bool> DaemonUpdater::isRunning_ = false;

DaemonUpdater::DaemonUpdater(HTaskInfo hTaskInfo) : HdcTransferBase(hTaskInfo)
{
    commandBegin = CMD_UPDATER_BEGIN;
    commandData = CMD_UPDATER_DATA;
}

DaemonUpdater::~DaemonUpdater()
{
    FLASHD_LOGI("~DaemonUpdater refCount %d", refCount);
}

bool DaemonUpdater::CommandDispatch(const uint16_t command, uint8_t *payload, const int payloadSize)
{
    if (IsDeviceLocked()) {
        std::string echo = "operation is not allowed";
        vector<uint8_t> buffer;
        buffer.push_back(command);
        buffer.push_back(Hdc::MSG_FAIL);
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(const_cast<char *>(echo.c_str())),
            reinterpret_cast<uint8_t *>(const_cast<char *>(echo.c_str())) + echo.size());
        SendToAnother(Hdc::CMD_UPDATER_FINISH, buffer.data(), buffer.size());
        FLASHD_LOGE("The devic is locked and operation is not allowed");
        return false;
    }

    if (!isInit_) {
        Init();
        isInit_ = true;
    }

    auto iter = cmdFunc_.find(command);
    if (iter == cmdFunc_.end()) {
        FLASHD_LOGE("command is invalid, command = %d", command);
        return false;
    }
    iter->second(payload, payloadSize);
    return true;
}

bool DaemonUpdater::SendToHost(Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg)
{
    if (!DaemonUpdater::isRunning_) {
        FLASHD_LOGW("flasd is not runing");
        return true;
    }

    if (state == Flashd::UpdaterState::DOING) {
        uint32_t temp = 0;
        std::stringstream percentageStream(msg);
        if (!(percentageStream >> temp)) {
            temp = 0;
        }
        uint8_t percentage = static_cast<uint8_t>(temp);
        SendToAnother(Hdc::CMD_UPDATER_PROGRESS, &percentage, sizeof(percentage));
        return true;
    }

    if (state == Flashd::UpdaterState::FAIL || state == Flashd::UpdaterState::SUCCESS) {
        uint8_t percentage = (state == Flashd::UpdaterState::SUCCESS) ? Flashd::PERCENT_FINISH : Flashd::PERCENT_CLEAR;
        SendToAnother(Hdc::CMD_UPDATER_PROGRESS, &percentage, sizeof(percentage));

        std::string echo = Hdc::Base::ReplaceAll(msg, "\n", " ");
        vector<uint8_t> buffer;
        buffer.push_back(static_cast<uint8_t>(type));
        buffer.push_back((state == Flashd::UpdaterState::SUCCESS) ? Hdc::MSG_OK : Hdc::MSG_FAIL);
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(const_cast<char *>(echo.c_str())),
            reinterpret_cast<uint8_t *>(const_cast<char *>(echo.c_str())) + echo.size());
        SendToAnother(Hdc::CMD_UPDATER_FINISH, buffer.data(), buffer.size());
        TaskFinish();
        if (commander_ != nullptr) {
            commander_->PostCommand();
        }
        DaemonUpdater::isRunning_ = false;
    }
    return true;
}

std::unique_ptr<Flashd::Commander> DaemonUpdater::CreateCommander(const std::string &cmd)
{
    if (DaemonUpdater::isRunning_) {
        FLASHD_LOGE("flashd has been running");
        return nullptr;
    }
    DaemonUpdater::isRunning_ = true;
    auto callback = [this](Flashd::CmdType type, Flashd::UpdaterState state, const std::string &msg) {
        SendToHost(type, state, msg);
    };
    return Flashd::CommanderFactory::GetInstance().CreateCommander(cmd, callback);
}

void DaemonUpdater::CheckCommand(const uint8_t *payload, int payloadSize)
{
    if (payloadSize < static_cast<int>(sizeof(int64_t))) {
        FLASHD_LOGE("payloadSize is invalid");
        return;
    }

    string bufString(reinterpret_cast<const char *>(payload + sizeof(int64_t)), payloadSize - sizeof(int64_t));
    SerialStruct::ParseFromString(ctxNow.transferConfig, bufString);

    ctxNow.master = false;
    ctxNow.fsOpenReq.data = &ctxNow;
    ctxNow.fileSize = ctxNow.transferConfig.fileSize;

    FLASHD_LOGI("functionName = %s, options = %s, fileSize = %u", ctxNow.transferConfig.functionName.c_str(),
        ctxNow.transferConfig.options.c_str(), ctxNow.transferConfig.fileSize);

    commander_ = CreateCommander(ctxNow.transferConfig.functionName.c_str());
    if (commander_ == nullptr) {
        FLASHD_LOGE("commander_ is null for cmd = %s", ctxNow.transferConfig.functionName.c_str());
        return;
    }
    commander_->DoCommand(ctxNow.transferConfig.options, ctxNow.transferConfig.fileSize);

    SendToAnother(commandBegin, nullptr, 0);
    refCount++;
}

void DaemonUpdater::DataCommand(const uint8_t *payload, int payloadSize) const
{
    if (commander_ == nullptr) {
        FLASHD_LOGE("commander_ is null");
        return;
    }

    if (payloadSize <= PAYLOAD_FIX_RESERVER) {
        FLASHD_LOGE("payloadSize is invaild");
        return;
    }

    string serialStrring(reinterpret_cast<const char *>(payload), PAYLOAD_FIX_RESERVER);
    TransferPayload pld = {};
    SerialStruct::ParseFromString(pld, serialStrring);
    commander_->DoCommand(payload + PAYLOAD_FIX_RESERVER, pld.uncompressSize);
}

void DaemonUpdater::EraseCommand(const uint8_t *payload, int payloadSize)
{
    commander_ = CreateCommander(CMDSTR_ERASE_PARTITION);
    if (commander_ == nullptr) {
        FLASHD_LOGE("commander_ is null for cmd = %s", CMDSTR_ERASE_PARTITION);
        return;
    }
    commander_->DoCommand(payload, payloadSize);
}

void DaemonUpdater::FormatCommand(const uint8_t *payload, int payloadSize)
{
    commander_ = CreateCommander(CMDSTR_FORMAT_PARTITION);
    if (commander_ == nullptr) {
        FLASHD_LOGE("commander_ is null for cmd = %s", CMDSTR_FORMAT_PARTITION);
        return;
    }
    commander_->DoCommand(payload, payloadSize);
}

void DaemonUpdater::Init()
{
    cmdFunc_.emplace(CMD_UPDATER_CHECK, bind(&DaemonUpdater::CheckCommand, this, placeholders::_1, placeholders::_2));
    cmdFunc_.emplace(CMD_UPDATER_DATA, bind(&DaemonUpdater::DataCommand, this, placeholders::_1, placeholders::_2));
    cmdFunc_.emplace(CMD_UPDATER_ERASE, bind(&DaemonUpdater::EraseCommand, this, placeholders::_1, placeholders::_2));
    cmdFunc_.emplace(CMD_UPDATER_FORMAT, bind(&DaemonUpdater::FormatCommand, this, placeholders::_1, placeholders::_2));
}

bool DaemonUpdater::IsDeviceLocked() const
{
    bool isLocked = true;
    if (auto ret = Updater::UpdateHdiClient::GetInstance().GetLockStatus(isLocked); ret != 0) {
        FLASHD_LOGE("GetLockStatus fail, ret = %d", ret);
        return true;
    }
    return isLocked;
}
} // namespace Hdc