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
using namespace Hpackage;
using namespace testing::ext;

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
        UpdatePatch::MemMapInfo data {};
        int32_t ret = PatchMapFile(fileName, data);
        EXPECT_EQ(0, ret);
        return UpdatePatch::GeneraterBufferHash({data.memory, data.length});
    }

    int BlockDiffPatchTest(const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile) const
    {
        int32_t ret = UpdatePatch::UpdateDiff::DiffBlock(TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);
        ret = UpdatePatch::UpdateApplyPatch::ApplyPatch(TEST_PATH_FROM + patchFile,
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
        int32_t ret = UpdatePatch::UpdateDiff::DiffImage(limit, TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);
        ret = UpdatePatch::UpdateApplyPatch::ApplyPatch(TEST_PATH_FROM + patchFile,
            TEST_PATH_FROM + oldFile, TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, ret);

        // 生成新文件的hash值
        std::string expected = GeneraterHash(TEST_PATH_FROM + newFile);
        std::string restoreHash = GeneraterHash(TEST_PATH_FROM + restoreFile);
        EXPECT_EQ(0, restoreHash.compare(expected));
        return 0;
    }

    int32_t TestApplyPatch(const std::string &patchName, const std::string &oldName,
        const std::string &expected, UpdatePatch::UpdateApplyPatch::ImageProcessor writer) const
    {
        PATCH_DEBUG("UpdateApplyPatch::ApplyPatch : %s ", patchName.c_str());
        std::vector<uint8_t> empty;
        UpdatePatch::MemMapInfo patchData {};
        UpdatePatch::MemMapInfo oldData {};
        int32_t ret = PatchMapFile(patchName, patchData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read patch file");
        ret = PatchMapFile(oldName, oldData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read old file");

        PATCH_LOGI("UpdateApplyPatch::ApplyPatch patchData %zu oldData %zu ", patchData.length, oldData.length);
        // check if image patch
        if (memcmp(patchData.memory, UpdatePatch::PKGDIFF_MAGIC,
            std::char_traits<char>::length(UpdatePatch::PKGDIFF_MAGIC)) == 0) {
            UpdatePatch::PatchParam param {};
            param.patch = patchData.memory;
            param.patchSize = patchData.length;
            param.oldBuff = oldData.memory;
            param.oldSize = oldData.length;
            ret = UpdatePatch::UpdateApplyPatch::ApplyImagePatch(param, empty, writer, expected);
            PATCH_CHECK(ret == 0, return -1, "Failed to apply image patch file");
        }
        return 0;
    }

    int ImgageDiffPatchFileTest2(size_t limit, const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile) const
    {
        int32_t ret = UpdatePatch::UpdateDiff::DiffImage(limit, TEST_PATH_FROM + oldFile,
            TEST_PATH_FROM + newFile, TEST_PATH_FROM + patchFile);
        EXPECT_EQ(0, ret);

        std::string expected = GeneraterHash(TEST_PATH_FROM + newFile);
        std::unique_ptr<UpdatePatch::FilePatchWriter> writer =
            std::make_unique<UpdatePatch::FilePatchWriter>(TEST_PATH_FROM + restoreFile);
        PATCH_CHECK(writer != nullptr, return -1, "Failed to create writer");
        writer->Init();

        ret = TestApplyPatch(TEST_PATH_FROM + patchFile, TEST_PATH_FROM + oldFile, expected,
            [&](size_t start, const UpdatePatch::BlockBuffer &data, size_t size) -> int {
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
        PATCH_DEBUG("UpdateApplyPatch::ApplyPatch : %s ", patchName.c_str());
        std::vector<uint8_t> empty;
        UpdatePatch::MemMapInfo patchData {};
        UpdatePatch::MemMapInfo oldData {};
        int32_t ret = PatchMapFile(patchName, patchData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read patch file");
        ret = PatchMapFile(oldName, oldData);
        PATCH_CHECK(ret == 0, return -1, "Failed to read old file");

        PATCH_LOGI("TestApplyBlockPatch patchData %zu oldData %zu ", patchData.length, oldData.length);
        UpdatePatch::PatchBuffer patchInfo = {patchData.memory, 0, patchData.length};
        UpdatePatch::BlockBuffer oldInfo = {oldData.memory, oldData.length};
        if (isBuffer) {
            std::vector<uint8_t> newData;
            ret = UpdatePatch::UpdateApplyPatch::ApplyBlockPatch(patchInfo, oldInfo, newData);
            PATCH_CHECK(ret == 0, return -1, "Failed to apply block patch file");
            std::ofstream stream(newName, std::ios::out | std::ios::binary);
            PATCH_CHECK(!stream.fail(), return -1, "Failed to open %s", newName.c_str());
            stream.write(reinterpret_cast<const char*>(newData.data()), newData.size());
        } else {
            std::unique_ptr<UpdatePatch::FilePatchWriter> writer =
                std::make_unique<UpdatePatch::FilePatchWriter>(newName);
            PATCH_CHECK(writer != nullptr, return -1, "Failed to create writer");
            writer->Init();

            ret = UpdatePatch::UpdateApplyPatch::ApplyBlockPatch(patchInfo, oldInfo, writer.get());
            EXPECT_EQ(0, ret);
            writer->Finish();
        }
        return 0;
    }

    int BlockDiffPatchTest2(const std::string &oldFile,
        const std::string &newFile, const std::string &patchFile, const std::string &restoreFile, bool isBuffer) const
    {
        int32_t ret = UpdatePatch::UpdateDiff::DiffBlock(TEST_PATH_FROM + oldFile,
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

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchTest, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest(
        "../diffpatch/patchtest.old",
        "../diffpatch/patchtest.new",
        "../diffpatch/patchtest.patch",
        "../diffpatch/patchtest.new_1"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchFileTest, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/patchtest.old",
        "../diffpatch/patchtest.new",
        "../diffpatch/patchtest.img_patch",
        "../diffpatch/patchtest.new_2"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchFileWithLimit, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(16,
        "../diffpatch/patchtest.old",
        "../diffpatch/patchtest.new",
        "../diffpatch/patchtest.img_patch",
        "../diffpatch/patchtest.new_3"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchGzFile, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/PatchGztest_old.gz",
        "../diffpatch/PatchGztest_new.gz",
        "../diffpatch/PatchGztest_gz.img_patch",
        "../diffpatch/PatchGztest_gz_new.zip"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/PatchLz4test_old.lz4",
        "../diffpatch/PatchLz4test_new.lz4",
        "../diffpatch/PatchLz4test_lz4.img_patch",
        "../diffpatch/PatchLz4test_lz4_new.lz"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File_1, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchLz4File_1_old.lz",
        "../diffpatch/ImgageDiffPatchLz4File_1_new.lz",
        "../diffpatch/ImgageDiffPatchLz4File_1_lz4.img_patch",
        "../diffpatch/ImgageDiffPatchLz4File_1_lz4_new.lz"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File_2, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(1,
        "../diffpatch/ImgageDiffPatchLz4File_1_old.lz",
        "../diffpatch/ImgageDiffPatchLz4File_1_new.lz",
        "../diffpatch/ImgageDiffPatchLz4File_1_lz4.img_patch",
        "../diffpatch/ImgageDiffPatchLz4File_1_lz4_new.lz"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchLz4File_3, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchLz4File_3_old.lz4",
        "../diffpatch/ImgageDiffPatchLz4File_3_new.lz4",
        "../diffpatch/ImgageDiffPatchLz4File_3_lz4.img_patch",
        "../diffpatch/ImgageDiffPatchLz4File_3_lz4_new.lz"));
}

// 测试包含一个文件时，新增一个文件
HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchZipFile_old.zip",
        "../diffpatch/ImgageDiffPatchZipFile_new.zip",
        "../diffpatch/ImgageDiffPatchZipFile_zip.img_patch",
        "../diffpatch/ImgageDiffPatchZipFile_zip_new.zip"));
}

// 测试使用winrar的压缩文件
HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_1, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchZipFile_1_old.zip",
        "../diffpatch/ImgageDiffPatchZipFile_1_new.zip",
        "../diffpatch/ImgageDiffPatchZipFile_1_zip.img_patch",
        "../diffpatch/ImgageDiffPatchZipFile_1_zip_new.zip"));
}

// 测试包含一个文件时，文件内容不相同
HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_2, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchZipFile_2_old.zip",
        "../diffpatch/ImgageDiffPatchZipFile_2_new.zip",
        "../diffpatch/ImgageDiffPatchZipFile_2_zip.img_patch",
        "../diffpatch/ImgageDiffPatchZipFile_2_zip_new.zip"));
}

// linux 上压缩，多文件测试
HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_3, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchZipFile_3_old.zip",
        "../diffpatch/ImgageDiffPatchZipFile_3_new.zip",
        "../diffpatch/ImgageDiffPatchZipFile_3_zip.img_patch",
        "../diffpatch/ImgageDiffPatchZipFile_3_zip_new.zip"));
}

// linux 上压缩，超大buffer length测试
HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchZipFile_4, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest(0,
        "../diffpatch/ImgageDiffPatchZipFile_4_old.zip",
        "../diffpatch/ImgageDiffPatchZipFile_4_new.zip",
        "../diffpatch/ImgageDiffPatchZipFile_4_zip.img_patch",
        "../diffpatch/ImgageDiffPatchZipFile_4_zip_new.zip"));
}

HWTEST_F(DiffPatchUnitTest, ImgageDiffPatchGzFile2, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.ImgageDiffPatchFileTest2(0,
        "../diffpatch/PatchGztest_old.gz",
        "../diffpatch/PatchGztest_new.gz",
        "../diffpatch/PatchGztest_gz.img_patch",
        "../diffpatch/PatchGztest_gz_new.zip"));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchGzFile, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/PatchGztest_old.gz",
        "../diffpatch/PatchGztest_new.gz",
        "../diffpatch/PatchGztest_gz.img_patch",
        "../diffpatch/PatchGztest_gz_new.zip", true));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchGzFile_1, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/PatchGztest_old.gz",
        "../diffpatch/PatchGztest_new.gz",
        "../diffpatch/PatchGztest_gz.img_patch",
        "../diffpatch/PatchGztest_gz_new.zip", false));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchLz4File, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/PatchLz4test_old.lz4",
        "../diffpatch/PatchLz4test_new.lz4",
        "../diffpatch/PatchLz4test_lz4.img_patch",
        "../diffpatch/PatchLz4test_lz4_new.lz", true));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchLz4File_1, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/PatchLz4test_old.lz4",
        "../diffpatch/PatchLz4test_new.lz4",
        "../diffpatch/PatchLz4test_lz4.img_patch",
        "../diffpatch/PatchLz4test_lz4_new.lz", false));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchTest_0, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/patchtest.old",
        "../diffpatch/patchtest.new",
        "../diffpatch/patchtest.img_patch",
        "../diffpatch/patchtest.new_2", true));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchTest_1, TestSize.Level1)
{
    DiffPatchUnitTest test;
    EXPECT_EQ(0, test.BlockDiffPatchTest2(
        "../diffpatch/patchtest.old",
        "../diffpatch/patchtest.new",
        "../diffpatch/patchtest.img_patch",
        "../diffpatch/patchtest.new_2", false));
}

HWTEST_F(DiffPatchUnitTest, BlockDiffPatchTest_2, TestSize.Level1)
{
    std::vector<uint8_t> testDate;
    testDate.push_back('a');
    EXPECT_EQ(0, UpdatePatch::WriteDataToFile("BlockDiffPatchTest_2.txt", testDate, testDate.size()));
}

HWTEST_F(DiffPatchUnitTest, PatchMapFileTest, TestSize.Level1)
{
    UpdatePatch::MemMapInfo data{};
    string filePath = TEST_PATH_FROM + "diffpatch/non_exist.file";
    EXPECT_EQ(-1, UpdatePatch::PatchMapFile(filePath, data));
}
}
