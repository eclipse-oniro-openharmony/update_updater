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

#ifndef IMAGEPATH_UNITTEST_H
#define IMAGEPATH_UNITTEST_H
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <libgen.h>
#include <memory>
#include <string>
#include <vector>
#include "applypatch/block_set.h"
#include "applypatch/block_writer.h"
#include "applypatch/data_writer.h"
#include "log.h"
#include "mount.h"
#include "patch/update_patch.h"
#include "utils.h"

constexpr int O_BINARY = 0;
namespace UpdaterUt {
#define UNUSED(x) (void)(x)
using namespace Updater;
class FileWriter : public DataWriter {
public:
    virtual bool Write(const uint8_t *addr, size_t len, const void *context)
    {
        UNUSED(context);
        write(fd_, addr, len);

        if (fsync(fd_) == -1) {
            LOG(ERROR) << "Failed to fsync ";
            return -1;
        }
        currentBlockLeft_ -= len;
        totalWritten_ += len;
        return true;
    }
    virtual ~FileWriter() {}
    FileWriter(int fd, BlockSet &bs) : fd_(fd), bs_(bs), totalWritten_(0), currentBlockLeft_(0) {}
    FileWriter(const FileWriter&) = delete;
    const FileWriter& operator=(const FileWriter&) = delete;
private:
    int fd_;
    BlockSet bs_;
    size_t totalWritten_;
    size_t currentBlockLeft_;
};

class ImagePatchTest : public ::testing::Test {
public:
    ImagePatchTest() = default;
    virtual ~ImagePatchTest() {
    }
    int TestZipModeImagePatch() const;
    int TestGZipModeImagePatch() const;
    int TestLZ4ModeImagePatch() const;
    int TestNormalModeImagePatch() const;
    int RunImageApplyPatch(UpdatePatch::PatchParam &param, const std::string &target,
        const std::string &expectedSHA256) const
    {
        mode_t mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        int fd = open(target.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_BINARY, mode);
        EXPECT_GE(fd, 0);
        BlockSet targetBlk;
        targetBlk.ParserAndInsert({
            "2", "0", "1"
        });
        std::unique_ptr<FileWriter> writer = std::make_unique<FileWriter>(fd, targetBlk);
        std::vector<uint8_t> empty;
        int32_t ret = UpdatePatch::UpdateApplyPatch::ApplyImagePatch(param, empty,
            [&](size_t start, const UpdatePatch::BlockBuffer &data, size_t size) -> int {
                bool ret = writer->Write(data.buffer, size, nullptr);
                return ret ? 0 : -1;
            }, expectedSHA256);
        close(fd);
        return ret;
    }

protected:
    void SetUp()
    {
        LoadSpecificFstab("/data/updater/applypatch/etc/fstab.imagepatch");
        std::string basePath = "/data/updater/imgpatch";
        Updater::Utils::MkdirRecursive(basePath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    void TearDown() {}
    void TestBody() {}

private:
    bool ReadContentFromFile(const std::string &file, std::string &content) const;
};
} // namespace updater_ut
#endif // IMAGEPATH_UNITTEST_H
