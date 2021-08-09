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

#include <gtest/gtest.h>
#include "applypatch/data_writer.h"
#include "unittest_comm.h"
#include "update_diff.h"
#include "update_patch.h"

using namespace std;
using namespace hpackage;
using namespace updatepatch;

namespace {
class DiffPatchUnitTest : public testing::Test {
public:
    DiffPatchUnitTest() {}
    ~DiffPatchUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
public:
    std::string GeneraterHash(const std::string &fileName) const
    {
        MemMapInfo data {};
        int32_t ret = PatchMapFile(fileName, data);
        EXPECT_EQ(0, ret);
        return GeneraterBufferHash({
            data.memory, data.length
        });
    }

    int BlockDiffPatchTest(const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile) const
    {
        int32_t ret = updatepatch::UpdateDiff::DiffBlock(TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);
        ret = updatepatch::UpdatePatch::ApplyPatch(TEST_PATH_FROM + patchFile,
            TEST_PATH_FROM + oldFile, TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, ret);

        std::string expected = GeneraterHash(TEST_PATH_FROM + newFile);
        std::string restoreHash = GeneraterHash(TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, memcmp(expected.c_str(), restoreHash.c_str(), restoreHash.size()));
        return 0;
    }

    int ImgageDiffPatchFileTest(size_t limit, const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile) const
    {
        int32_t ret = updatepatch::UpdateDiff::DiffImage(limit, TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);
        ret = updatepatch::UpdatePatch::ApplyPatch(TEST_PATH_FROM + patchFile,
            TEST_PATH_FROM + oldFile, TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, ret);

        // 生成新文件的hash值
        std::string expected = GeneraterHash(TEST_PATH_FROM + newFile);
        std::string restoreHash = GeneraterHash(TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, restoreHash.compare(expected));
        return 0;
    }

    int32_t TestApplyPatch(const std::string &patchName, const std::string &oldName,
        const std::string &expected, updatepatch::UpdatePatch::ImageProcessor writer) const
    {
        PATCH_DEBUG("UpdatePatch::ApplyPatch : %s ", patchName.c_str());
        std::vector<uint8_t> empty;
        MemMapInfo patchData {};
        MemMapInfo oldData {};
        int32_t ret = PatchMapFile(patchName, patchData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read patch file");
        ret = PatchMapFile(oldName, oldData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read old file");

        PATCH_LOGI("UpdatePatch::ApplyPatch patchData %zu oldData %zu ", patchData.length, oldData.length);
        // check if image patch
        if (memcmp(patchData.memory, PKGDIFF_MAGIC.c_str(), PKGDIFF_MAGIC.size()) == 0) {
            PatchParam param {};
            param.patch = patchData.memory;
            param.patchSize = patchData.length;
            param.oldBuff = oldData.memory;
            param.oldSize = oldData.length;
            ret = updatepatch::UpdatePatch::ApplyImagePatch(param, empty, writer, expected);
            PATCH_CHECK(ret == 0, return -1, "Failed to apply image patch file");
        }
        return 0;
    }

    int ImgageDiffPatchFileTest2(size_t limit, const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile) const
    {
        int32_t ret = updatepatch::UpdateDiff::DiffImage(limit, TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);

        std::string expected = GeneraterHash(TEST_PATH_FROM + newFile);
        std::unique_ptr<FilePatchWriter> writer = std::make_unique<FilePatchWriter>(TEST_PATH_FROM + restoreFile);
        PATCH_CHECK(writer != nullptr, return -1, "Failed to create writer");
        writer->Init();

        ret = TestApplyPatch(TEST_PATH_FROM + patchFile, TEST_PATH_FROM + oldFile, expected,
            [&](size_t start, const BlockBuffer &data, size_t size) -> int {
                return writer->Write(start, data, size);
            });
        EXPECT_EQ(0, ret);
        writer->Finish();
        std::string restoreHash = GeneraterHash(TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, restoreHash.compare(expected));
        return 0;
    }

    int32_t TestApplyBlockPatch(const std::string &patchName,
        const std::string &oldName, const std::string &newName, bool isBuffer) const
    {
        PATCH_DEBUG("UpdatePatch::ApplyPatch : %s ", patchName.c_str());
        std::vector<uint8_t> empty;
        MemMapInfo patchData {};
        MemMapInfo oldData {};
        int32_t ret = PatchMapFile(patchName, patchData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read patch file");
        ret = PatchMapFile(oldName, oldData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read old file");

        PATCH_LOGI("TestApplyBlockPatch patchData %zu oldData %zu ", patchData.length, oldData.length);
        PatchBuffer patchInfo = {patchData.memory, 0, patchData.length};
        BlockBuffer oldInfo = {oldData.memory, oldData.length};
        if (isBuffer) {
            std::vector<uint8_t> newData;
            ret = updatepatch::UpdatePatch::ApplyBlockPatch(patchInfo, oldInfo, newData);
            PATCH_CHECK(ret == 0, return -1, "Failed to apply block patch file");
            std::ofstream stream(newName, std::ios::out | std::ios::binary);
            PATCH_CHECK(!stream.fail(), return -1, "Failed to open %s", newName.c_str());
            stream.write(reinterpret_cast<const char*>(newData.data()), newData.size());
        } else {
            std::unique_ptr<FilePatchWriter> writer = std::make_unique<FilePatchWriter>(newName);
            PATCH_CHECK(writer != nullptr, return -1, "Failed to create writer");
            writer->Init();

            ret = UpdatePatch::ApplyBlockPatch(patchInfo, oldInfo, writer.get());
            EXPECT_EQ(0, ret);
            writer->Finish();
        }
        return 0;
    }

    int BlockDiffPatchTest2(const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile, bool isBuffer) const
    {
        int32_t ret = updatepatch::UpdateDiff::DiffBlock(TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);

        ret = TestApplyBlockPatch(TEST_PATH_FROM + patchFile,
            TEST_PATH_FROM + oldFile, TEST_PATH_FROM + restoreFile, isBuffer);
        EXPECT_EQ(0, ret);

        std::string expected = GeneraterHash(TEST_PATH_FROM + newFile);
        std::string restoreHash = GeneraterHash(TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, memcmp(expected.c_str(), restoreHash.c_str(), restoreHash.size()));
        return 0;
    }
};

TEST_F(DiffPatchUnitTest, BlockDiffPatchTest)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest(
        "../diffpatch/2.log.old",
        "../diffpatch/2.log.new",
        "../diffpatch/2.log.patch",
        "../diffpatch/2.log.new_1"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchFileTest)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/2.log.old",
        "../diffpatch/2.log.new",
        "../diffpatch/2.log.img_patch",
        "../diffpatch/2.log.new_2"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchFileWithLimit)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(16,
        "../diffpatch/2.log.old",
        "../diffpatch/2.log.new",
        "../diffpatch/2.log.img_patch",
        "../diffpatch/2.log.new_3"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchGzFile)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/5_old.gz",
        "../diffpatch/5_new.gz",
        "../diffpatch/5_gz.img_patch",
        "../diffpatch/5_gz_new.zip"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/4_old.lz4",
        "../diffpatch/4_new.lz4",
        "../diffpatch/4_lz4.img_patch",
        "../diffpatch/4_lz4_new.lz"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File_1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/4_1_old.lz",
        "../diffpatch/4_1_new.lz",
        "../diffpatch/4_1_lz4.img_patch",
        "../diffpatch/4_1_lz4_new.lz"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File_2)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(1,
        "../diffpatch/4_1_old.lz",
        "../diffpatch/4_1_new.lz",
        "../diffpatch/4_1_lz4.img_patch",
        "../diffpatch/4_1_lz4_new.lz"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File_3)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/7_old.lz4",
        "../diffpatch/7_new.lz4",
        "../diffpatch/7_lz4.img_patch",
        "../diffpatch/7_lz4_new.lz"));
}

// 测试包含一个文件时，新增一个文件
TEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/2_old.zip",
        "../diffpatch/2_new.zip",
        "../diffpatch/2_zip.img_patch",
        "../diffpatch/2_zip_new.zip"));
}

// 测试使用winrar的压缩文件
TEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/2_1_old.zip",
        "../diffpatch/2_1_new.zip",
        "../diffpatch/2_1_zip.img_patch",
        "../diffpatch/2_1_zip_new.zip"));
}

// 测试包含一个文件时，文件内容不相同
TEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_2)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/2_2_old.zip",
        "../diffpatch/2_2_new.zip",
        "../diffpatch/2_2_zip.img_patch",
        "../diffpatch/2_2_zip_new.zip"));
}

// linux 上压缩，多文件测试
TEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_3)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/2_3_old.zip",
        "../diffpatch/2_3_new.zip",
        "../diffpatch/2_3_zip.img_patch",
        "../diffpatch/2_3_zip_new.zip"));
}

// linux 上压缩，超大buffer length测试
TEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_4)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/6_old.zip",
        "../diffpatch/6_new.zip",
        "../diffpatch/6_zip.img_patch",
        "../diffpatch/6_zip_new.zip"));
}

TEST_F(DiffPatchUnitTest, ImgageDiffPatchGzFile2)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest2(0,
        "../diffpatch/5_old.gz",
        "../diffpatch/5_new.gz",
        "../diffpatch/5_gz.img_patch",
        "../diffpatch/5_gz_new.zip"));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchGzFile)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/5_old.gz",
        "../diffpatch/5_new.gz",
        "../diffpatch/5_gz.img_patch",
        "../diffpatch/5_gz_new.zip", true));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchGzFile_1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/5_old.gz",
        "../diffpatch/5_new.gz",
        "../diffpatch/5_gz.img_patch",
        "../diffpatch/5_gz_new.zip", false));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchLz4File)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/4_old.lz4",
        "../diffpatch/4_new.lz4",
        "../diffpatch/4_lz4.img_patch",
        "../diffpatch/4_lz4_new.lz", true));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchLz4File_1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/4_old.lz4",
        "../diffpatch/4_new.lz4",
        "../diffpatch/4_lz4.img_patch",
        "../diffpatch/4_lz4_new.lz", false));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchTest_0)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/2.log.old",
        "../diffpatch/2.log.new",
        "../diffpatch/2.log.img_patch",
        "../diffpatch/2.log.new_2", true));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchTest_1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/2.log.old",
        "../diffpatch/2.log.new",
        "../diffpatch/2.log.img_patch",
        "../diffpatch/2.log.new_2", false));
}

TEST_F(DiffPatchUnitTest, BlockDiffPatchTest_2)
{
    std::vector<uint8_t> testDate;
    testDate.push_back('a');
    EXPECT_EQ(0, WriteDataToFile("BlockDiffPatchTest_2.txt", testDate, testDate.size()));
}
}
