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

namespace Updater {
UpdaterEnv *DataWriter::env_ = nullptr;
int DataWriter::OpenPath(const std::string &path)
{
    if (path.empty()) {
        LOG(ERROR) << "Datawriter: partition name is empty.";
        return -1;
    }

    if (access(path.c_str(), W_OK) < 0) {
        LOG(ERROR) << "Datawriter: " << path << " is not writable.";
        return -1;
    }
    char *realPath = realpath(path.c_str(), NULL);
    if (realPath == nullptr) {
        LOG(ERROR) << "realPath is NULL" << " : " << strerror(errno);
        return -1;
    }
    int fd = open(realPath, O_RDWR | O_LARGEFILE);
    free(realPath);
    if (fd < 0) {
        LOG(ERROR) << "Datawriter: open block device " << path << " failed " << " : " << strerror(errno);
        return fd;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        LOG(ERROR) << "Datawriter: seek " << path << "failed " << " : " << strerror(errno);
        close(fd);
        fd = -1;
    }
    return fd;
}

std::unique_ptr<DataWriter> DataWriter::CreateDataWriter(WriteMode mode, const std::string &path,
    uint64_t offset)
{
    switch (mode) {
        case WRITE_RAW:
            return std::make_unique<RawWriter>(path, offset);
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

void DataWriter::SetUpdaterEnv(UpdaterEnv *env)
{
    env_ = env;
}

std::unique_ptr<DataWriter> DataWriter::CreateDataWriter(WriteMode mode, const std::string &path,
    UpdaterEnv *env, uint64_t offset)
{
    env_ = env;
    return CreateDataWriter(mode, path, offset);
}

void DataWriter::ReleaseDataWriter(std::unique_ptr<DataWriter> &writer)
{
    writer.reset();
}
} // namespace Updater
