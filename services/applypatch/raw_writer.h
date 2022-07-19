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

#ifndef UPDATER_RAW_WRITER_H
#define UPDATER_RAW_WRITER_H

#include <cstdio>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include "applypatch/data_writer.h"

namespace Updater {
class RawWriter : public DataWriter {
public:
    bool Write(const uint8_t *addr, size_t len, const void *context) override;

    RawWriter(const std::string path, uint64_t offset) : fd_(-1), path_(path), offset_(offset) {}

    virtual ~RawWriter()
    {
        offset_ = 0;
        if (fd_ > 0) {
            fsync(fd_);
            close(fd_);
        }
        fd_ = -1;
    }
private:
    int WriteInternal(int fd, const uint8_t *data, size_t len);

    RawWriter(const RawWriter&) = delete;

    const RawWriter& operator=(const RawWriter&) = delete;
    int fd_;
    std::string path_;
    off64_t offset_;
};
} // namespace Updater
#endif /* UPDATER_RAW_WRITER_H */
