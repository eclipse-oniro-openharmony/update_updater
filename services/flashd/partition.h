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

#ifndef FLASHD_PARTITION_H
#define FLASHD_PARTITION_H

#include <string>

#include "image_writer/image_writer.h"

namespace Flashd {
class Partition {
public:
    explicit Partition(const std::string &devName) : devName_(devName) {}
    Partition(const std::string &devName, std::unique_ptr<FlashdWriter> writer) : devName_(devName),
        writer_(std::move(writer)) {}
    ~Partition() {};
    int DoFlash(const uint8_t *buffer, int bufferSize) const;
    int DoFormat() const;
    int DoErase() const;

private:
    uint64_t GetBlockDeviceSize(int fd) const;

    std::string devName_ = "";
    std::unique_ptr<FlashdWriter> writer_ = nullptr;
};
}
#endif