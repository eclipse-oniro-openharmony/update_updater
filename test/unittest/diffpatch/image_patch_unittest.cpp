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
#include "image_patch.h"
#include "diffpatch.h"
#include "unittest_comm.h"

using namespace std;
using namespace Hpackage;
using namespace UpdatePatch;
using namespace testing::ext;

namespace {
class TestCompressedImagePatch : public CompressedImagePatch {
public:
    TestCompressedImagePatch(UpdatePatchWriterPtr writer, const std::vector<uint8_t> &bonusData)
        : CompressedImagePatch(writer, bonusData) {}

    ~TestCompressedImagePatch() override {}

    int32_t ApplyImagePatch(const PatchParam &param, size_t &startOffset) override
    {
        return 0;
    }

    int32_t TestStartReadHeader(const PatchParam &param, PatchHeader &header, size_t &offset)
    {
        return StartReadHeader(param, header, offset);
    }

    int32_t TestDecompressData(PkgManager::PkgManagerPtr &pkgManager, PkgBuffer buffer,
    PkgManager::StreamPtr &stream, bool memory, size_t expandedLen)
    {
        return DecompressData(pkgManager, buffer, stream, memory, expandedLen);
    }
protected:
    int32_t ReadHeader(const PatchParam &param, PatchHeader &header, size_t &offset) override
    {
        return 0;
    }

    std::unique_ptr<FileInfo> GetFileInfo() const override
    {
        ZipFileInfo *fileInfo = new(std::nothrow) ZipFileInfo;
        return std::unique_ptr<FileInfo>((FileInfo *)fileInfo);
    }
};

class CompressedImagePatchUnitTest : public testing::Test {
public:
    CompressedImagePatchUnitTest() {}
    ~CompressedImagePatchUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(CompressedImagePatchUnitTest, TestStartReadHeader, TestSize.Level0)
{
    UpdatePatchWriterPtr writer = nullptr;
    const std::vector<uint8_t> bonusData;
    TestCompressedImagePatch test(writer, bonusData);
    PatchParam patchParam = {
        reinterpret_cast<u_char*>(const_cast<char*>("xxx")), sizeof("xxx"),
        reinterpret_cast<u_char*>(const_cast<char*>("xxx")), sizeof("xxx")
    };
    PatchHeader header = {0, 0, 0, 0, 0};
    size_t offset = 0;
    int32_t ret = test.TestStartReadHeader(patchParam, header, offset);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CompressedImagePatchUnitTest, TestDecompressData, TestSize.Level0)
{
    UpdatePatchWriterPtr writer = nullptr;
    const std::vector<uint8_t> bonusData;
    TestCompressedImagePatch test(writer, bonusData);
    PkgManager::PkgManagerPtr pkgManager = nullptr;
    PkgBuffer buffer;
    PkgManager::StreamPtr stream;
    bool memory = false;
    size_t expandedLen = 0;
    int32_t ret = test.TestDecompressData(pkgManager, buffer, stream, memory, expandedLen);
    EXPECT_EQ(ret, 0);
    expandedLen = 1;
    ret = test.TestDecompressData(pkgManager, buffer, stream, memory, expandedLen);
    EXPECT_EQ(ret, -1);
    pkgManager = PkgManager::CreatePackageInstance();
    ret = test.TestDecompressData(pkgManager, buffer, stream, memory, expandedLen);
    EXPECT_EQ(ret, -1);
    PkgManager::ReleasePackageInstance(pkgManager);
}
}
