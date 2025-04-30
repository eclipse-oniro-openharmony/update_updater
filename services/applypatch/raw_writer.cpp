/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "raw_writer.h"
#include <cerrno>
#include <cstdio>
#include <string>
#include <unistd.h>
#include "log/log.h"

namespace Updater {
extern "C" __attribute__((constructor)) void RegisterRawWriter(void)
{
    DataWriter::RegisterDataWriter("WRITE_RAW",
        [](const std::string &path, const std::string &partName, uint64_t startAddr,
            uint64_t offset) -> std::unique_ptr<DataWriter> {
                return std::make_unique<RawWriter>(path, startAddr, offset);
            });
}

bool RawWriter::Write(const uint8_t *addr, size_t len, [[maybe_unused]] const void *context)
{
    if (addr == nullptr) {
        LOG(ERROR) << "RawWriter: invalid address.";
        return false;
    }

    if (len == 0) {
        LOG(WARNING) << "RawWriter: write length is 0, skip.";
        return false;
    }

    if (fd_ < 0) {
        fd_ = OpenPath(path_);
        if (fd_ < 0) {
            return false;
        }
    }

    if (WriteInternal(fd_, addr, len) < 0) {
        return false;
    }
    return true;
}

int RawWriter::WriteInternal(int fd, const uint8_t *data, size_t len)
{
    ssize_t written = 0;
    size_t rest = len;

    int ret = lseek64(fd, offset_, SEEK_SET);
    if (ret == -1) {
        LOG(ERROR) << "RawWriter: failed to seek file to " << offset_ << " : " << strerror(errno);
        return -1;
    }

    while (rest > 0) {
        written = write(fd, data, rest);
        if (written < 0) {
            LOG(ERROR) << "RawWriter: failed to write data of len " << len << " : " << strerror(errno);
            return -1;
        }
        data += written;
        rest -= static_cast<size_t>(written);
    }
    offset_ += static_cast<off64_t>(len);
    return 0;
}

bool RawWriter::Sync(void)
{
    if (fd_ < 0) {
        LOG(ERROR) << "invalid fd " << fd_;
        return false;
    }
    if (fsync(fd_) != 0) {
        LOG(ERROR) << "fsync failed, errno is " << errno;
        return false;
    }
    LOG(INFO) << "fsync successfully " << fd_;
    return true;
}
} // namespace Updater
