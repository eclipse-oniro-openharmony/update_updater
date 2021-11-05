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

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include "log.h"
#include "pkg_algorithm.h"
#include "pkg_gzipfile.h"
#include "pkg_lz4file.h"
#include "pkg_manager.h"
#include "pkg_manager_impl.h"
#include "pkg_test.h"
#include "pkg_upgradefile.h"
#include "pkg_utils.h"
#include "pkg_zipfile.h"
#include "securec.h"

using namespace std;
using namespace hpackage;
using namespace updater;
using namespace testing::ext;

namespace {
constexpr uint32_t MAX_FILE_NAME = 256;
constexpr uint32_t CENTRAL_SIGNATURE = 0x02014b50;
constexpr uint32_t END_CENTRAL_SIGNATURE = 0x06054b50;
constexpr uint32_t ZIP_NODE_ID = 100;
constexpr size_t GIANT_NUMBER = 100000;

class TestFile : public PkgFile {
public:
    explicit TestFile(PkgManager::PkgManagerPtr pkgManager, PkgStreamPtr stream)
        : PkgFile(pkgManager, stream, PKG_TYPE_MAX) {}

    virtual ~TestFile() {}

    virtual int32_t AddEntry(const PkgManager::FileInfoPtr file, const PkgStreamPtr inStream)
    {
        PkgFile::GetPkgInfo();
        PkgFile::AddPkgEntry(inStream->GetFileName());
        return 0;
    }

    virtual int32_t SavePackage(size_t &offset)
    {
        return 0;
    }

    virtual int32_t LoadPackage(std::vector<std::string>& fileNames, VerifyFunction verify = nullptr)
    {
        return 0;
    }
};

class PkgPackageTest : public PkgTest {
public:
    PkgPackageTest() {}
    ~PkgPackageTest() override {}

    int TestPkgFile()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        if (pkgManager_ == nullptr) {
            return PKG_SUCCESS;
        }
        PkgManager::StreamPtr stream = nullptr;
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        int ret = pkgManager_->CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
        auto file = std::make_unique<Lz4PkgFile>(pkgManager_, PkgStreamImpl::ConvertPkgStream(stream));
        EXPECT_NE(file, nullptr);
        constexpr uint32_t lz4NodeId = 100;
        auto entry = std::make_unique<Lz4FileEntry>(file.get(), lz4NodeId);
        EXPECT_NE(entry, nullptr);

        EXPECT_NE(((PkgEntryPtr)entry.get())->GetPkgFile(), nullptr);
        FileInfo fileInfo;
        ret = entry->Init(&fileInfo, PkgStreamImpl::ConvertPkgStream(stream));
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestPkgFileInvalid()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        if (pkgManager_ == nullptr) {
            return PKG_SUCCESS;
        }
        PkgManager::StreamPtr stream = nullptr;
        std::string packagePath = TEST_PATH_TO;
        packagePath += testPackageName;
        int ret = pkgManager_->CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
        FileInfo fileInfo;
        std::unique_ptr<TestFile> file = std::make_unique<TestFile>(pkgManager_,
            PkgStreamImpl::ConvertPkgStream(stream));
        EXPECT_NE(file, nullptr);
        ret = file->AddEntry(&fileInfo, PkgStreamImpl::ConvertPkgStream(stream));
        EXPECT_EQ(ret, 0);
        size_t offset = 0;
        ret = file->SavePackage(offset);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestBigZipEntry()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        PkgManager::StreamPtr stream = nullptr;
        std::string packagePath = TEST_PATH_TO;
        uint32_t zipNodeId = 100;
        packagePath += testPackageName;
        pkgManager_->CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
        EXPECT_NE(stream, nullptr);
        std::unique_ptr<TestFile> file = std::make_unique<TestFile>(pkgManager_,
            PkgStreamImpl::ConvertPkgStream(stream));
        EXPECT_NE(file, nullptr);
        std::unique_ptr<ZipFileEntry> entry = std::make_unique<ZipFileEntry>(file.get(), ZIP_NODE_ID);
        EXPECT_NE(entry, nullptr);

        string name = "TestBigZip";
        uint16_t extraSize = 20;
        size_t offsetHalfWord = 2;
        size_t offsetWord = 4;
        size_t offset4Words = 16;
        size_t offset3Words = 12;
        int32_t buffLen = MAX_FILE_NAME + sizeof(LocalFileHeader) + sizeof(DataDescriptor) +
            sizeof(CentralDirEntry) + offsetWord + offset4Words;
        std::vector<uint8_t> buff(buffLen);
        CentralDirEntry* centralDir = (CentralDirEntry *)buff.data();
        centralDir->signature = CENTRAL_SIGNATURE;
        centralDir->versionMade = 0;
        centralDir->versionNeeded = 0;
        centralDir->flags = 0;
        centralDir->compressionMethod = PKG_COMPRESS_METHOD_ZIP;
        centralDir->crc = 0;
        centralDir->modifiedDate = 0;
        centralDir->modifiedTime = 0;
        centralDir->compressedSize = UINT_MAX;
        centralDir->uncompressedSize = UINT_MAX;
        centralDir->nameSize = name.length();
        centralDir->extraSize = extraSize;
        centralDir->commentSize = 0;
        centralDir->diskNumStart = 0;
        centralDir->internalAttr = 0;
        centralDir->externalAttr = 0;
        centralDir->localHeaderOffset = 0;
        int ret = memcpy_s(buff.data() + sizeof(CentralDirEntry), name.length(), name.c_str(), name.length());
        EXPECT_EQ(ret, 0);
        WriteLE16(buff.data() + sizeof(CentralDirEntry) + name.length(), 1);
        WriteLE16(buff.data() + sizeof(CentralDirEntry) + name.length() + offsetHalfWord, offset4Words);
        size_t size = UINT_MAX + GIANT_NUMBER;
        WriteLE64(buff.data() + sizeof(CentralDirEntry) + name.length() + offsetWord, size);
        WriteLE64(buff.data() + sizeof(CentralDirEntry) + name.length() + offset3Words, size);
        size_t decodeLen = 0;
        PkgBuffer buffer(buff);
        entry->DecodeCentralDirEntry(nullptr, buffer, 0, decodeLen);
        return 0;
    }

    void WriteLE64(uint8_t *buff, size_t size) const
    {
        *reinterpret_cast<size_t *>(buff) = size;
    }

    int TestBigZipFile()
    {
        pkgManager_ = static_cast<PkgManagerImpl*>(PkgManager::GetPackageInstance());
        EXPECT_NE(pkgManager_, nullptr);
        string name = "TestBigZipFile";
        PkgManager::StreamPtr stream = nullptr;
        size_t buffLen = sizeof(Zip64EndCentralDirLocator) + sizeof(EndCentralDir) + sizeof(Zip64EndCentralDirRecord);
        pkgManager_->CreatePkgStream(stream, name, buffLen, PkgStream::PkgStreamType_MemoryMap);
        EXPECT_NE(stream, nullptr);
        PkgBuffer data;
        stream->GetBuffer(data);

        uint32_t signNumber = 0x06064b50;
        Zip64EndCentralDirRecord zip64Cdr;
        zip64Cdr.signature = signNumber;
        zip64Cdr.totalEntries = 0;
        zip64Cdr.offset = 0;
        EXPECT_EQ(memcpy_s(data.buffer, sizeof(zip64Cdr), &zip64Cdr, sizeof(zip64Cdr)), 0);

        signNumber = 0x07064b50;
        Zip64EndCentralDirLocator zip64Locator;
        zip64Locator.signature = signNumber;
        zip64Locator.numberOfDisk = 1;
        zip64Locator.endOfCentralDirectoryRecord = 0;
        zip64Locator.totalNumberOfDisks = 1;
        EXPECT_EQ(memcpy_s(data.buffer + sizeof(Zip64EndCentralDirRecord), sizeof(zip64Locator), &zip64Locator,
            sizeof(zip64Locator)),
            0);

        EndCentralDir endDir;
        endDir.signature = END_CENTRAL_SIGNATURE;
        endDir.numDisk = 0;
        endDir.startDiskOfCentralDir = 0;
        endDir.totalEntriesInThisDisk = 1;
        endDir.totalEntries = 0;
        endDir.sizeOfCentralDir = 0;
        endDir.offset = UINT_MAX;
        endDir.commentLen = 0;
        EXPECT_EQ(memcpy_s(data.buffer + sizeof(Zip64EndCentralDirRecord) + sizeof(Zip64EndCentralDirLocator),
            sizeof(endDir), &endDir, sizeof(endDir)),
            0);

        std::unique_ptr<ZipPkgFile> zipFile = std::make_unique<ZipPkgFile>(pkgManager_,
            PkgStreamImpl::ConvertPkgStream(stream));
        EXPECT_NE(zipFile, nullptr);
        std::vector<std::string> components;
        int ret = zipFile->LoadPackage(components);
        EXPECT_EQ(ret, 0);
        return 0;
    }
};

HWTEST_F(PkgPackageTest, TestPkgFile, TestSize.Level1)
{
    PkgPackageTest test;
    EXPECT_EQ(0, test.TestPkgFile());
}

HWTEST_F(PkgPackageTest, TestPkgFileInvalid, TestSize.Level1)
{
    PkgPackageTest test;
    EXPECT_EQ(0, test.TestPkgFileInvalid());
}

HWTEST_F(PkgPackageTest, TestBigZip, TestSize.Level1)
{
    PkgPackageTest test;
    EXPECT_EQ(0, test.TestBigZipEntry());
}

HWTEST_F(PkgPackageTest, TestBigZipFile, TestSize.Level1)
{
    PkgPackageTest test;
    EXPECT_EQ(0, test.TestBigZipFile());
}
}
