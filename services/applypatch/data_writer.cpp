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
#include "applypatch/data_writer.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>
#include "applypatch/block_writer.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "raw_writer.h"

namespace updater {
UpdaterEnv *DataWriter::env_ = nullptr;
int DataWriter::OpenPartition(const std::string &partitionName)
{
    if (partitionName.empty()) {
        LOG(ERROR) << "Datawriter: partition name is empty.";
        return -1;
    }
    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    if (devPath.empty()) {
        LOG(ERROR) << "Datawriter: cannot find device path for partition \'" <<
            partitionName.substr(1, partitionName.size()) << "\'.";
        return -1;
    }

    if (access(devPath.c_str(), W_OK) < 0) {
        LOG(ERROR) << "Datawriter: " << devPath << " is not writable.";
        return -1;
    }

    int fd = open(devPath.c_str(), O_WRONLY | O_EXCL);
    UPDATER_FILE_CHECK(fd >= 0, "Datawriter: open block device " << devPath << " failed ", return fd);
    UPDATER_CHECK_FILE_OP(lseek(fd, 0, SEEK_SET) != -1, "Datawriter: seek " << devPath << "failed ", fd, fd = -1);
    return fd;
}

std::unique_ptr<DataWriter> DataWriter::CreateDataWriter(WriteMode mode, const std::string &partitionName)
{
    switch (mode) {
        case WRITE_RAW:
        {
            std::unique_ptr<RawWriter> writer(std::make_unique<RawWriter>(partitionName));
            return std::move(writer);
        }
        case WRITE_DECRYPT:
            LOG(WARNING) << "Unsupported writer mode.";
            break;
        default:
            break;
    }
    return nullptr;
}

UpdaterEnv *DataWriter::GetUpdaterEnv()
{
    return env_;
}

std::unique_ptr<DataWriter> DataWriter::CreateDataWriter(WriteMode mode, const std::string &partitionName,
    UpdaterEnv *env)
{
    env_ = env;
    return CreateDataWriter(mode, partitionName);
}

void DataWriter::ReleaseDataWriter(std::unique_ptr<DataWriter> &writer)
{
    writer.reset();
}
} // namespace updater
