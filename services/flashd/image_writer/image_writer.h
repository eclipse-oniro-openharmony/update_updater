/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef FLASHD_IMAGE_WRITER_H
#define FLASHD_IMAGE_WRITER_H

#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>

#include "applypatch/data_writer.h"
#include "macros.h"

namespace Flashd {
using namespace Updater;
class FlashdWriter {
public:
    virtual ~FlashdWriter() {}
    virtual int Write(const std::string &partition, const uint8_t *data, size_t len) = 0;
};

class FlashdWriterRaw : public FlashdWriter {
public:
    int Write(const std::string &partition, const uint8_t *data, size_t len) override;

private:
    bool GetDataWriter(const std::string &partition);
    std::unique_ptr<DataWriter> writer_ = nullptr;
};

class FlashdImageWriter {
    DISALLOW_COPY_MOVE(FlashdImageWriter);
public:
    using CheckImageProcess = std::function<bool (const std::string &, const uint8_t *, size_t)>;
    using GetWriterProcess = std::function<std::unique_ptr<FlashdWriter>()>;
    struct FlashdWriterGet {
        CheckImageProcess checkImage {};
        GetWriterProcess getWriter {};
    };
    static FlashdImageWriter &GetInstance();
    std::unique_ptr<FlashdWriter> GetWriter(const std::string &partition, const uint8_t *data, size_t len) const;
    void RegisterUserWriter(CheckImageProcess checkImage, GetWriterProcess getWriter);

private:
    FlashdImageWriter();
    ~FlashdImageWriter() {}
    bool IsRawImage(const std::string &partition, const uint8_t *data, size_t len) const;
    std::unique_ptr<FlashdWriter> GetRawWriter() const;
    std::vector<FlashdWriterGet>  writerGet_ = {};
};
} // Flashd
#endif // FLASHD_IMAGE_WRITER_H
