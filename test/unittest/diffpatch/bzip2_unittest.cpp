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
#include "bzip2_adapter.h"
#include "diffpatch.h"
#include "lz4_adapter.h"
#include "unittest_comm.h"
#include "update_patch.h"
#include "zip_adapter.h"

using namespace std;
using namespace Hpackage;
using namespace UpdatePatch;
using namespace testing::ext;

namespace {
#define LZ4_BLOCK_SIZE(blockId) (1 << (8 + (2 * (blockId))))

class TestPatchWriter : public UpdatePatchWriter {
public:
    explicit TestPatchWriter(std::vector<uint8_t> &buffer) : UpdatePatchWriter(), buffer_(buffer) {}
    ~TestPatchWriter() override {}

    int32_t Init() override
    {
        return 0;
    };
    int32_t Finish() override
    {
        return 0;
    };
    int32_t Write(size_t start, const BlockBuffer &data, size_t len) override
    {
        if (len == 0) {
            return 0;
        }
        bufferSize += len;
        if ((start + bufferSize) > buffer_.size()) {
            buffer_.resize(IGMDIFF_LIMIT_UNIT * ((start + bufferSize) / IGMDIFF_LIMIT_UNIT + 1));
        }
        return memcpy_s(buffer_.data() + start, buffer_.size(), data.buffer, len);
    }
private:
    size_t bufferSize {0};
    std::vector<uint8_t> &buffer_;
};

class BZip2AdapterUnitTest : public testing::Test {
public:
    BZip2AdapterUnitTest() {}
    ~BZip2AdapterUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
public:
    int BZip2AdapterBufferTest() const
    {
        MemMapInfo data {};
        std::string fileName = TEST_PATH_FROM;
        fileName += "test_script.us";
        int32_t ret = PatchMapFile(fileName, data);
        EXPECT_EQ(0, ret);

        std::vector<uint8_t> compressedData;
        BZipBuffer2Adapter adapter(compressedData, 0);
        adapter.Open();
        // compress data 1
        BlockBuffer srcData = {data.memory, data.length};
        ret = adapter.WriteData(srcData);
        EXPECT_EQ(0, ret);
        size_t compressedData1 = 0;
        ret = adapter.FlushData(compressedData1);
        EXPECT_EQ(0, ret);
        adapter.Close();

        // compress data 2
        BZipBuffer2Adapter adapter2(compressedData, compressedData1);
        adapter2.Open();
        ret = adapter2.WriteData(srcData);
        EXPECT_EQ(0, ret);
        size_t compressedData2 = 0;
        ret = adapter2.FlushData(compressedData2);
        EXPECT_EQ(0, ret);
        adapter2.Close();

        PATCH_LOGI("compressedData size %zu %zu %zu", compressedData.size(), compressedData1, compressedData2);
        // decompress data 1
        BlockBuffer compressedInfo = {compressedData.data(), compressedData.size()};
        BZip2BufferReadAdapter readAdapter(0, compressedData1, compressedInfo);
        readAdapter.Open();

        std::vector<uint8_t> dataArray(data.length);
        BlockBuffer data1 = {dataArray.data(), data.length};
        ret = readAdapter.ReadData(data1);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0, memcmp(data1.buffer, data.memory, data1.length));

        // decompress data 2
        BZip2BufferReadAdapter readAdapter2(compressedData1, compressedData2, compressedInfo);
        readAdapter2.Open();
        ret = readAdapter2.ReadData(data1);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0, memcmp(data1.buffer, data.memory, data1.length));

        adapter.Close();
        readAdapter.Close();
        return 0;
    }

    int BZip2AdapterAddMoreTest() const
    {
        MemMapInfo data {};
        std::string fileName = TEST_PATH_FROM;
        fileName += "test_script.us";
        int32_t ret = PatchMapFile(fileName, data);
        EXPECT_EQ(0, ret);

        std::vector<uint8_t> compressedData;
        BZipBuffer2Adapter adapter(compressedData, 0);
        adapter.Open();
        // compress data 1
        BlockBuffer srcData = {data.memory, data.length};
        ret = adapter.WriteData(srcData);
        EXPECT_EQ(0, ret);
        // compress data 2
        ret = adapter.WriteData(srcData);
        EXPECT_EQ(0, ret);
        // compress data 3
        ret = adapter.WriteData(srcData);
        EXPECT_EQ(0, ret);
        size_t compressedData1 = 0;
        ret = adapter.FlushData(compressedData1);
        EXPECT_EQ(0, ret);
        adapter.Close();

        PATCH_LOGI("compressedData size %zu %zu", compressedData.size(), compressedData1);

        BlockBuffer compressedInfo = {compressedData.data(), compressedData.size()};
        BZip2BufferReadAdapter readAdapter(0, compressedData1, compressedInfo);
        readAdapter.Open();

        // decompress data 1
        std::vector<uint8_t> dataArray(data.length);
        BlockBuffer data1 = {dataArray.data(), data.length};
        ret = readAdapter.ReadData(data1);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0, memcmp(data1.buffer, data.memory, data1.length));

        // decompress data 2
        ret = readAdapter.ReadData(data1);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0, memcmp(data1.buffer, data.memory, data1.length));

        // decompress data 3
        ret = readAdapter.ReadData(data1);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0, memcmp(data1.buffer, data.memory, data1.length));

        adapter.Close();
        readAdapter.Close();
        return 0;
    }

    int32_t CompressData(Hpackage::PkgManager::FileInfoPtr info,
        const BlockBuffer &buffer, std::vector<uint8_t> &outData, size_t &bufferSize)
    {
        Hpackage::PkgManager *pkgManager = Hpackage::PkgManager::GetPackageInstance();
        PATCH_CHECK(pkgManager != nullptr, return -1, "Can not get manager ");
        Hpackage::PkgManager::StreamPtr stream1 = nullptr;
        pkgManager->CreatePkgStream(stream1, "gzip", [&outData, &bufferSize](const PkgBuffer &data,
            size_t size, size_t start, bool isFinish, const void *context) ->int {
                if (isFinish) {
                    return 0;
                }
                bufferSize += size;
                if ((start + bufferSize) > outData.size()) {
                    outData.resize(IGMDIFF_LIMIT_UNIT * ((start + bufferSize) / IGMDIFF_LIMIT_UNIT + 1));
                }
                return memcpy_s(outData.data() + start, outData.size(), data.buffer, size);
            }, nullptr);
        int32_t ret = pkgManager->CompressBuffer(info, {buffer.buffer, buffer.length}, stream1);
        PATCH_CHECK(ret == 0, return -1, "Can not Compress buff ");
        PATCH_DEBUG("UpdateDiff::MakePatch totalSize: %zu", bufferSize);
        return 0;
    }

    int DeflateAdapterTest(const std::string &fileName, Hpackage::PkgManager::FileInfoPtr info)
    {
        std::vector<uint8_t> outData;
        size_t dataSize = 0;
        std::unique_ptr<TestPatchWriter> testPatchWriter(new TestPatchWriter(outData));
        PATCH_CHECK(testPatchWriter != nullptr, return -1, "Failed to create data writer");

        MemMapInfo memInfo {};
        int32_t ret = PatchMapFile(TEST_PATH_FROM + fileName, memInfo);
        PATCH_CHECK(ret == 0, return -1, "Failed to map file");

        //
        std::vector<uint8_t> outData1;
        size_t dataSize1 = 0;
        ret = CompressData(info, {memInfo.memory, memInfo.length}, outData1, dataSize1);
        PATCH_CHECK(ret == 0, return -1, "Failed to compress file");

        info->unpackedSize = memInfo.length;

        std::unique_ptr<DeflateAdapter> deflateAdapter;
        if (info->packMethod == PKG_COMPRESS_METHOD_ZIP) {
            deflateAdapter.reset(new ZipAdapter(testPatchWriter.get(), 0, info));
        } else if (info->packMethod == PKG_COMPRESS_METHOD_LZ4) {
            deflateAdapter.reset(new Lz4FrameAdapter(testPatchWriter.get(), 0, info));
        } else if (info->packMethod == PKG_COMPRESS_METHOD_LZ4_BLOCK) {
            deflateAdapter.reset(new Lz4BlockAdapter(testPatchWriter.get(), 0, info));
        }
        PATCH_CHECK(deflateAdapter != nullptr, return -1, "Failed to create deflate adapter");
        deflateAdapter->Open();

        size_t offset = 0;
        while (offset < memInfo.length) {
            size_t writeSize = (memInfo.length > (offset + DeflateAdapter::BUFFER_SIZE)) ?
                DeflateAdapter::BUFFER_SIZE : (memInfo.length - offset);
            BlockBuffer data = {memInfo.memory + offset, writeSize};
            ret = deflateAdapter->WriteData(data);
            PATCH_CHECK(ret == 0, return -1, "Failed to compress data");
            offset += writeSize;
        }
        deflateAdapter->FlushData(dataSize);

        PATCH_LOGI("data size %zu %zu", dataSize, dataSize1);

        // compare
        if (dataSize == dataSize1 && memcmp(outData.data(), outData1.data(), dataSize1) == 0) {
            return 0;
        }
        return 1;
    }
};

HWTEST_F(BZip2AdapterUnitTest, BZip2AdapterBufferTest, TestSize.Level1)
{
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.BZip2AdapterBufferTest());
}

HWTEST_F(BZip2AdapterUnitTest, BZip2AdapterAddMoreTest, TestSize.Level1)
{
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.BZip2AdapterAddMoreTest());
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForZip, TestSize.Level1)
{
    ZipFileInfo zipInfo {};
    zipInfo.fileInfo.packMethod = PKG_COMPRESS_METHOD_ZIP;
    zipInfo.method = 8;
    zipInfo.level = 6;
    zipInfo.windowBits = -15;
    zipInfo.memLevel = 8;
    zipInfo.strategy = 0;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.new", &zipInfo.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    lz4Info.compressionLevel = 2;
    lz4Info.blockIndependence = 0;
    lz4Info.contentChecksumFlag = 0;
    lz4Info.blockSizeID = 0;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4_2, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    lz4Info.compressionLevel = 2;
    lz4Info.blockIndependence = 0;
    lz4Info.contentChecksumFlag = 0;
    lz4Info.blockSizeID = 7;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4_3, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    lz4Info.compressionLevel = 2;
    lz4Info.blockIndependence = 0;
    lz4Info.contentChecksumFlag = 0;
    lz4Info.blockSizeID = 4;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4_4, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    lz4Info.compressionLevel = 2;
    lz4Info.blockIndependence = 0;
    lz4Info.contentChecksumFlag = 0;
    lz4Info.blockSizeID = 5;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4_5, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    lz4Info.compressionLevel = 2;
    lz4Info.blockIndependence = 0;
    lz4Info.contentChecksumFlag = 0;
    lz4Info.blockSizeID = 5;
    lz4Info.autoFlush = 0;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4Block, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4_BLOCK;
    lz4Info.compressionLevel = 2;
    lz4Info.blockIndependence = 1;
    lz4Info.contentChecksumFlag = 1;
    lz4Info.blockSizeID = 5;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4Block_2, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4_BLOCK;
    lz4Info.compressionLevel = 5;
    lz4Info.blockIndependence = 1;
    lz4Info.contentChecksumFlag = 1;
    lz4Info.blockSizeID = 5;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4Block_3, TestSize.Level1)
{
    Lz4FileInfo lz4Info {};
    lz4Info.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4_BLOCK;
    lz4Info.compressionLevel = 5;
    lz4Info.blockIndependence = 1;
    lz4Info.contentChecksumFlag = 1;
    lz4Info.blockSizeID = 0;
    BZip2AdapterUnitTest test;
    EXPECT_EQ(0, test.DeflateAdapterTest("../diffpatch/patchtest.test", &lz4Info.fileInfo));
}

HWTEST_F(BZip2AdapterUnitTest, DeflateAdapterTestForLz4Block_4, TestSize.Level1)
{
    DeflateAdapter adapterTest;
    BlockBuffer srcTestData;
    size_t offTest = 0;
    adapterTest.Open();
    adapterTest.WriteData(srcTestData);
    adapterTest.FlushData(offTest);
}
}
