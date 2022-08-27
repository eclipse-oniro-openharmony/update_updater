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

#include "imagepatch_unittest.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "applypatch/block_writer.h"
#include "applypatch/data_writer.h"
#include "log/log.h"
#include "patch/update_patch.h"
#include "pkg_utils.h"
#include "utils.h"

using namespace testing::ext;
using namespace Updater;
namespace UpdaterUt {
bool ImagePatchTest::ReadContentFromFile(const std::string& file, std::string &content) const
{
    int flags = O_RDONLY | O_CLOEXEC;
    int fd = open(file.c_str(), flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return false;
    }

    struct stat st {};
    if (fstat(fd, &st) < 0) {
        close(fd);
        return false;
    }
    content.reserve(st.st_size);
    constexpr size_t bufferSize = 1024;
    char buffer[bufferSize];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, n);
    }
    close(fd);
    return ((n == 0) ? true : false);
}

int ImagePatchTest::TestZipModeImagePatch() const
{
    std::string expectedSHA256 = "980571599cea18fc164d03dfd26df7e666c346c6a40df49c22dcec15f060c984";
    std::string sourceData;
    auto rc = ReadContentFromFile("/data/updater/applypatch/source.zip", sourceData);
    EXPECT_TRUE(rc);
    std::string fileName = Hpackage::GetName("/data/updater/applypatch/source.zip");
    printf("filename: %s\n", fileName.c_str());
    std::string patchFile = "/data/updater/applypatch/zip-patch-file";
    std::string patchContent;
    rc = ReadContentFromFile(patchFile, patchContent);
    EXPECT_TRUE(rc);
    UpdatePatch::PatchParam param = {
        reinterpret_cast<uint8_t *>(sourceData.data()), sourceData.size(),
        reinterpret_cast<uint8_t *>(patchContent.data()), patchContent.size()
    };
    return RunImageApplyPatch(param, "/data/updater/applypatch/out_put_zip.zip", expectedSHA256);
}

int ImagePatchTest::TestNormalModeImagePatch() const
{
    std::string expectedSHA256 = "d5c87f954c3fb45685888d5edd359c27950a1a0acd33c45ad4e284b6a85686e5";
    std::string sourceData;
    auto rc = ReadContentFromFile("/data/updater/diffpatch/patchtest.old", sourceData);
    EXPECT_TRUE(rc);
    std::string patchFile = "/data/updater/diffpatch/patchtest.img_patch";
    std::string patchContent;
    rc = ReadContentFromFile(patchFile, patchContent);
    EXPECT_TRUE(rc);
    UpdatePatch::PatchParam param = {
        reinterpret_cast<uint8_t *>(sourceData.data()), sourceData.size(),
        reinterpret_cast<uint8_t *>(patchContent.data()), patchContent.size()
    };
    return RunImageApplyPatch(param, "/data/updater/diffpatch/out_put_zip.zip", expectedSHA256);
}

int ImagePatchTest::TestGZipModeImagePatch() const
{
    std::string expectedSHA256 = "805486a0df9b8919107ef6bf383452e642aca5d371848e4c7a9b8b59cd741b1f";
    std::string sourceData;
    auto rc = ReadContentFromFile("/data/updater/applypatch/TestGZipModeImagePatch.old.gz", sourceData);
    EXPECT_TRUE(rc);
    std::string patchContent;
    rc = ReadContentFromFile("/data/updater/applypatch/TestGZipModeImagePatch.gz.patch", patchContent);
    EXPECT_TRUE(rc);
    UpdatePatch::PatchParam param = {
        reinterpret_cast<uint8_t *>(sourceData.data()), sourceData.size(),
        reinterpret_cast<uint8_t *>(patchContent.data()), patchContent.size()
    };
    int ret = RunImageApplyPatch(param, "/data/updater/applypatch/out_put_gzip.gzip", expectedSHA256);
    return ret;
}

int ImagePatchTest::TestLZ4ModeImagePatch() const
{
    std::string expectedSHA256 = "ec500f45b48886dd20e1e0042a74954026b9c59e5168e1d6465d928cea7a1064";
    std::string sourceData;
    auto rc = ReadContentFromFile("/data/updater/diffpatch/PatchLz4test_old.lz4", sourceData);
    EXPECT_TRUE(rc);
    std::string patchContent;
    rc = ReadContentFromFile("/data/updater/diffpatch/PatchLz4test_lz4.img_patch", patchContent);
    EXPECT_TRUE(rc);
    UpdatePatch::PatchParam param = {
        reinterpret_cast<uint8_t *>(sourceData.data()), sourceData.size(),
        reinterpret_cast<uint8_t *>(patchContent.data()), patchContent.size()
    };
    RunImageApplyPatch(param, "/data/updater/diffpatch/out_put_lz4.lz4", expectedSHA256);
    return 0;
}

HWTEST_F(ImagePatchTest, TestZipModeImagePatch, TestSize.Level1)
{
    ImagePatchTest test;
    EXPECT_EQ(0, test.TestZipModeImagePatch());
}

HWTEST_F(ImagePatchTest, TestGZipModeImagePatch, TestSize.Level1)
{
    ImagePatchTest test;
    EXPECT_EQ(0, test.TestGZipModeImagePatch());
}

HWTEST_F(ImagePatchTest, TestLZ4ModeImagePatch, TestSize.Level1)
{
    ImagePatchTest test;
    EXPECT_EQ(0, test.TestLZ4ModeImagePatch());
}

HWTEST_F(ImagePatchTest, TestNormalModeImagePatch, TestSize.Level1)
{
    ImagePatchTest test;
    EXPECT_EQ(0, test.TestNormalModeImagePatch());
}
} // namespace updater_ut
