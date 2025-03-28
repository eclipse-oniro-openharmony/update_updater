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
#include <pthread.h>
#include "securec.h"
#include "applypatch/data_writer.h"
#include "applypatch/partition_record.h"
#include "applypatch/update_progress.h"
#include "dump.h"
#include "log.h"
#include "package/hash_data_verifier.h"
#include "pkg_manager.h"
#ifdef UPDATER_USE_PTABLE
#include "ptable_manager.h"
#endif
#include "script_instruction.h"
#include "script_manager.h"
#include "slot_info/slot_info.h"
#include "updater_main.h"
#include "updater/updater_const.h"
#include "update_bin/bin_process.h"
#include "scope_guard.h"
#include "bin_chunk_update.h"

#define TYPE_ZIP_HEADER  0xaa

using namespace Uscript;
using namespace Hpackage;
using namespace Updater;

namespace Updater {
constexpr uint32_t BUFFER_SIZE = 50 * 1024;
constexpr uint32_t MAX_UPDATER_BUFFER_SIZE = 2 * BUFFER_SIZE;
constexpr uint32_t BYTE_SHIFT_8 = 8;
constexpr uint32_t BYTE_SHIFT_16 = 16;
constexpr uint32_t BYTE_SHIFT_24 = 24;
constexpr uint32_t SECOND_BUFFER = 2;
constexpr uint32_t THIRD_BUFFER = 3;

enum UpdateStatus {
    UPDATE_STATE_INIT = 0,
    UPDATE_STATE_ONGOING,
    UPDATE_STATE_FAILED,
    UPDATE_STATE_SUCCESSFUL,
    UPDATE_STATE_MAX
};

bool ReadLE16(std::istream& is, uint16_t& value)
{
    char buf[2] = {0};
    if (!is.read(buf, sizeof(buf))) {
        return false;
    }
    value = static_cast<uint16_t>(static_cast<unsigned char>(buf[0])) |
           (static_cast<uint16_t>(static_cast<unsigned char>(buf[1])) << BYTE_SHIFT_8);
    return true;
}

bool ReadLE32(std::istream& is, uint32_t& value)
{
    char buf[4] = {0};
    if (!is.read(buf, sizeof(buf))) {
        return false;
    }
    value = static_cast<uint32_t>(static_cast<unsigned char>(buf[0])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[1])) << BYTE_SHIFT_8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[SECOND_BUFFER])) << BYTE_SHIFT_16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[THIRD_BUFFER])) << BYTE_SHIFT_24);
    return true;
}
static int ProcessUpdateFile(const std::string &packagePath, FILE* pipeWrite)
{
    std::shared_ptr<Updater::BinChunkUpdate> binChunkUpdate_ {};
    // 打开输入文件
    std::ifstream in_file(packagePath, std::ios::binary);
    if (!in_file) {
        LOG(ERROR) << "Error: Failed to open " << packagePath;
        return UPDATE_ERROR;
    }

    uint16_t type = 0;
    if (!ReadLE16(in_file, type)) {
        LOG(ERROR) << "Failed to read type";
        return UPDATE_ERROR;
    }
    
    if (type != TYPE_ZIP_HEADER) {
        LOG(ERROR) << "Unsupported header type: 0x" << std::hex << type;
        in_file.close();
        return UPDATE_ERROR;
    }
    uint32_t length = 0;
    if (!ReadLE32(in_file, length)) {
        LOG(ERROR) << "Failed to read length";
        return UPDATE_ERROR;
    }

    if (!in_file.seekg(length, std::ios::cur)) {
        in_file.close();
        LOG(ERROR) << "Failed to seekg length";
        return UPDATE_ERROR;
    }

    // 读取剩余数据
    std::vector<uint8_t> buffer_stream(BUFFER_SIZE);
    binChunkUpdate_ = std::make_unique<Updater::BinChunkUpdate>(MAX_UPDATER_BUFFER_SIZE);
    while (!in_file.eof()) {
        in_file.read(reinterpret_cast<char*>(buffer_stream.data()), buffer_stream.size());
        size_t readBytes = in_file.gcount();
        uint32_t dealLen = 0;
        if (readBytes > 0) {
            UpdateResultCode ret = binChunkUpdate_->StartBinChunkUpdate(
                buffer_stream.data(), static_cast<uint32_t>(readBytes), dealLen);
            if (STREAM_UPDATE_SUCCESS == ret) {
                LOG(INFO) << "StreamInstallProcesser ThreadExecuteFunc STREM_UPDATE_SUCCESS";
            } else if (STREAM_UPDATE_FAILURE == ret) {
                LOG(ERROR) << "StreamInstallProcesser ThreadExecuteFunc STREM_UPDATE_FAILURE";
                return UPDATE_ERROR;
            } else if (STREAM_UPDATE_COMPLETE == ret) {
                LOG(INFO) << "StreamInstallProcesser ThreadExecuteFunc STREM_UPDATE_COMPLETE";
                break;
            }
        }
    }
    in_file.close();
    return UPDATE_SUCCESS;
}

int ProcessUpdaterStream(bool retry, int pipeFd, const std::string &packagePath, const std::string &keyPath)
{
    UPDATER_INIT_RECORD;
    UpdaterInit::GetInstance().InvokeEvent(UPDATER_BINARY_INIT_EVENT);
    Dump::GetInstance().RegisterDump("DumpHelperLog", std::make_unique<DumpHelperLog>());

    // 初始化管道
    std::unique_ptr<FILE, decltype(&fclose)> pipeWrite(fdopen(pipeFd, "w"), fclose);
    if (pipeWrite == nullptr) {
        LOG(ERROR) << "Fail to fdopen, err: " << strerror(errno);
        UPDATER_LAST_WORD(strerror(errno), "Fail to fdopen");
        return EXIT_INVALID_ARGS;
    }

    int ret = -1;
    Detail::ScopeGuard guard([&] {
        (void)fprintf(pipeWrite.get(), "subProcessResult:%d\n", ret);
        (void)fflush(pipeWrite.get());
    });
    setlinebuf(pipeWrite.get());

    // 初始化包管理器
    PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        UPDATER_LAST_WORD(EXIT_INVALID_ARGS, "pkgManager is nullptr");
        return EXIT_INVALID_ARGS;
    }

    // 加载包
    std::vector<std::string> components;
    int loadRet = pkgManager->LoadPackage(packagePath, keyPath, components);
    if (loadRet != PKG_SUCCESS) {
        LOG(ERROR) << "Fail to load package";
        PkgManager::ReleasePackageInstance(pkgManager);
        UPDATER_LAST_WORD("Fail to load package", packagePath, keyPath);
        return EXIT_INVALID_ARGS;
    }

#ifdef UPDATER_USE_PTABLE
    // 分区表操作
    if (!PackagePtable::GetInstance().WritePtableWithFile()) {
        LOG(ERROR) << "write ptable with file fail";
        PkgManager::ReleasePackageInstance(pkgManager);
        UPDATER_LAST_WORD("Error to write ptable with file");
        return EXIT_EXEC_SCRIPT_ERROR;
    }
    if (!DevicePtable::GetInstance().LoadPartitionInfo()) {
        PkgManager::ReleasePackageInstance(pkgManager);
        return EXIT_EXEC_SCRIPT_ERROR;
    }
#endif

    // 调用核心函数并传递管道
    ret = ProcessUpdateFile(packagePath, pipeWrite.get());
    if (ret != UPDATE_SUCCESS) {
        LOG(ERROR) << "ProcessUpdaterStream failed with code: " << ret;
    }

    // 释放资源
    PkgManager::ReleasePackageInstance(pkgManager);
    return ret;
}
} // Updater