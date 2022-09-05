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
#include "image_writer.h"

#include <cerrno>
#include <cstdio>
#include <dlfcn.h>
#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>

#include "applypatch/raw_writer.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "ptable_manager.h"
#include "updater/updater_const.h"

namespace Flashd {
using namespace Updater;
using namespace std::placeholders;

bool FlashdWriterRaw::GetDataWriter(const std::string &partition)
{
#ifdef UPDATER_USE_PTABLE
    DevicePtable &devicePtb = DevicePtable::GetInstance();
    Ptable::PtnInfo ptnInfo;
    if (!devicePtb.GetPartionInfoByName(partition, ptnInfo)) {
        LOG(ERROR) << "FlashdWriterRaw: cannot find the lun index of partition: " << partition;
        return false;
    }
    char lunIndexName = 'a' + ptnInfo.lun;
    const std::string writePath = std::string(PREFIX_UFS_NODE) + lunIndexName;
    writer_ = DataWriter::CreateDataWriter(WRITE_RAW, writePath, ptnInfo.startAddr);
#else
    writer_ = DataWriter::CreateDataWriter(WRITE_RAW, GetBlockDeviceByMountPoint(partition));
#endif
    if (writer_ == nullptr) {
        LOG(ERROR) << partition << " FlashdWriterRaw writer error";
        return false;
    }
    return true;
}

int FlashdWriterRaw::Write(const std::string &partition, const uint8_t *data, size_t len)
{
    if (writer_ == nullptr) {
        if (!GetDataWriter(partition)) {
            return -ENOSPC;
        }
    }

    if (!writer_->Write(data, len, nullptr)) {
        LOG(ERROR) << "writer " << partition << " failed ";
        return -1;
    }

    return 0;
}

FlashdImageWriter::FlashdImageWriter()
{
    FlashdWriterGet writerGet;
    writerGet.checkImage = std::bind(&FlashdImageWriter::IsRawImage, this, _1, _2, _3);
    writerGet.getWriter = std::bind(&FlashdImageWriter::GetRawWriter, this);
    writerGet_.emplace_back(std::move(writerGet));
}

FlashdImageWriter &FlashdImageWriter::GetInstance()
{
    static FlashdImageWriter instance;
    return instance;
}

void FlashdImageWriter::RegisterUserWriter(CheckImageProcess checkImage, GetWriterProcess getWriter)
{
    FlashdWriterGet writerGet = { checkImage, getWriter };
    writerGet_.insert(writerGet_.begin(), std::move(writerGet));
}

std::unique_ptr<FlashdWriter> FlashdImageWriter::GetWriter(const std::string &partition,
    const uint8_t *data, size_t len) const
{
    // init writer
    for (auto &iter : writerGet_) {
        if (iter.checkImage(partition, data, len)) {
            return iter.getWriter();
        }
    }
    return nullptr;
}

bool FlashdImageWriter::IsRawImage(const std::string &partition, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] size_t len) const
{
    // default raw
    LOG(INFO) << partition << " is raw instruction";
    return true;
}

std::unique_ptr<FlashdWriter> FlashdImageWriter::GetRawWriter() const
{
    // init raw writer
    return std::make_unique<FlashdWriterRaw>();
}
} // namespace Flashd
