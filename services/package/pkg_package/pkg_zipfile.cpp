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
#include "pkg_zipfile.h"
#include <ctime>
#include <limits>
#include "pkg_algorithm.h"
#include "pkg_manager.h"
#include "pkg_stream.h"
#include "zip_pkg_parse.h"
#include "zlib.h"

namespace Hpackage {
constexpr uint32_t TM_YEAR_BITS = 9;
constexpr uint32_t TM_MON_BITS = 5;
constexpr uint32_t TM_MIN_BITS = 5;
constexpr uint32_t TM_HOUR_BITS = 11;
constexpr uint32_t BIG_SIZE_HEADER = 20;
constexpr uint32_t START_YEAR = 1900;
constexpr uint32_t MAX_FILE_NAME = 256;
constexpr uint32_t LOCAL_HEADER_SIGNATURE = 0x04034b50;
constexpr uint32_t CENTRAL_SIGNATURE = 0x02014b50;
constexpr uint32_t END_CENTRAL_SIGNATURE = 0x06054b50;
constexpr uint32_t DATA_DESC_SIGNATURE = 0x08074b50;
// mask value that signifies that the entry has a DD
constexpr uint32_t GPBDD_FLAG_MASK = 0x0008;
constexpr uint32_t ZIP_PKG_ALIGNMENT_DEF = 1;
constexpr int32_t DEF_MEM_LEVEL = 8;

int32_t ZipPkgFile::AddEntry(const PkgManager::FileInfoPtr file, const PkgStreamPtr inStream)
{
    if (!CheckState({PKG_FILE_STATE_IDLE, PKG_FILE_STATE_WORKING}, PKG_FILE_STATE_WORKING)) {
        PKG_LOGE("Error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    if (file == nullptr || inStream == nullptr) {
        PKG_LOGE("AddEntry failed, invalid param");
        return PKG_INVALID_PARAM;
    }
    PKG_LOGI("ZipPkgFile::AddEntry %s ", file->identity.c_str());

    int32_t ret = PKG_SUCCESS;
    ZipFileEntry* entry = static_cast<ZipFileEntry*>(AddPkgEntry(file->identity));
    if (entry == nullptr) {
        PKG_LOGE("Failed to create pkg node for %s", file->identity.c_str());
        return PKG_NONE_MEMORY;
    }
    entry->Init(file, inStream);

    size_t encodeLen = 0;
    ret = entry->EncodeHeader(inStream, currentOffset_, encodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to encode for %s", file->identity.c_str());
        return ret;
    }
    currentOffset_ += encodeLen;
    ret = entry->Pack(inStream, currentOffset_, encodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to pack for %s", file->identity.c_str());
        return ret;
    }
    currentOffset_ += encodeLen;
    return PKG_SUCCESS;
}

int32_t ZipPkgFile::SavePackage(size_t &signOffset)
{
    UNUSED(signOffset);
    if (!CheckState({PKG_FILE_STATE_WORKING}, PKG_FILE_STATE_CLOSE)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    int32_t ret = PKG_SUCCESS;
    size_t offset = currentOffset_;
    for (auto &it : pkgEntryMapId_) {
        ZipFileEntry* entry = static_cast<ZipFileEntry*>(it.second);
        if (entry == nullptr) {
            PKG_LOGE("Failed to write CentralDirEntry");
            return PKG_INVALID_PARAM;
        }
        size_t encodeLen = 0;
        entry->EncodeCentralDirEntry(pkgStream_, offset, encodeLen);
        offset += encodeLen;
    }

    std::vector<uint8_t> buff(sizeof(EndCentralDir));
    WriteLE32(buff.data() + offsetof(EndCentralDir, signature), END_CENTRAL_SIGNATURE);
    WriteLE16(buff.data() + offsetof(EndCentralDir, numDisk), 0);
    WriteLE16(buff.data() + offsetof(EndCentralDir, startDiskOfCentralDir), 0);
    WriteLE16(buff.data() + offsetof(EndCentralDir, totalEntriesInThisDisk), pkgEntryMapId_.size());
    WriteLE16(buff.data() + offsetof(EndCentralDir, totalEntries), pkgEntryMapId_.size());
    WriteLE32(buff.data() + offsetof(EndCentralDir, sizeOfCentralDir), offset - currentOffset_);
    WriteLE32(buff.data() + offsetof(EndCentralDir, offset), currentOffset_);
    WriteLE16(buff.data() + offsetof(EndCentralDir, commentLen), 0);
    PkgBuffer buffer(buff);
    ret = pkgStream_->Write(buffer, sizeof(EndCentralDir), offset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to write CentralDirEntry for %s", pkgStream_->GetFileName().c_str());
        return ret;
    }
    currentOffset_ = offset + sizeof(EndCentralDir);
    pkgStream_->Flush(currentOffset_);
    return PKG_SUCCESS;
}

int32_t ZipPkgFile::LoadPackage(std::vector<std::string> &fileNames, const PkgBuffer &buffer,
    uint32_t endDirLen, size_t endDirPos, size_t &readLen)
{
    size_t fileLen = pkgStream_->GetFileLength();
    EndCentralDir endDir;
    endDir.signature = ReadLE32(buffer.buffer + offsetof(EndCentralDir, signature));
    endDir.numDisk = ReadLE16(buffer.buffer  + offsetof(EndCentralDir, numDisk));
    endDir.startDiskOfCentralDir = ReadLE16(buffer.buffer  + offsetof(EndCentralDir, startDiskOfCentralDir));
    endDir.totalEntriesInThisDisk = ReadLE16(buffer.buffer  + offsetof(EndCentralDir, totalEntriesInThisDisk));
    endDir.totalEntries = ReadLE16(buffer.buffer  + offsetof(EndCentralDir, totalEntries));
    endDir.sizeOfCentralDir = ReadLE32(buffer.buffer  + offsetof(EndCentralDir, sizeOfCentralDir));
    endDir.offset = ReadLE32(buffer.buffer  + offsetof(EndCentralDir, offset));
    endDir.commentLen = ReadLE16(buffer.buffer  + offsetof(EndCentralDir, commentLen));
    if ((endDir.numDisk != 0) || (endDir.signature != END_CENTRAL_SIGNATURE) ||
        (endDir.startDiskOfCentralDir != 0)
#ifndef UPDATER_UT
        || (endDir.offset >= fileLen) || (endDir.totalEntriesInThisDisk != endDir.totalEntries) ||
        ((endDir.offset + endDir.sizeOfCentralDir + endDirLen) > fileLen)
#endif
        ) {
        PKG_LOGE("end dir format error %s", pkgStream_->GetFileName().c_str());
        return PKG_INVALID_PKG_FORMAT;
    }
    size_t currentPos = endDir.offset;
    if (endDir.offset == UINT_MAX) {
        int32_t ret = pkgStream_->Read(buffer, endDirPos - sizeof(Zip64EndCentralDirLocator),
            sizeof(Zip64EndCentralDirLocator), readLen);
        uint32_t signature = ReadLE32(buffer.buffer + offsetof(Zip64EndCentralDirLocator, signature));
        if (ret != PKG_SUCCESS || signature != 0x07064b50) {
            return ParseFileEntries(fileNames, endDir, currentPos, fileLen);
        }
        currentPos = ReadLE64(buffer.buffer + offsetof(Zip64EndCentralDirLocator, endOfCentralDirectoryRecord));
        ret = pkgStream_->Read(buffer, currentPos, sizeof(Zip64EndCentralDirRecord), readLen);
        signature = ReadLE32(buffer.buffer + offsetof(Zip64EndCentralDirRecord, signature));
        if (ret == PKG_SUCCESS && signature == 0x06064b50) {
            currentPos = ReadLE64(buffer.buffer + offsetof(Zip64EndCentralDirRecord, offset));
        }
    }
    return ParseFileEntries(fileNames, endDir, currentPos, fileLen);
}

int32_t ZipPkgFile::LoadPackage(std::vector<std::string>& fileNames, VerifyFunction verifier)
{
    UNUSED(verifier);
    PKG_LOGI("LoadPackage %s :%zu", pkgStream_->GetFileName().c_str(), pkgStream_->GetFileLength());
    PKG_CHECK(CheckState({PKG_FILE_STATE_IDLE}, PKG_FILE_STATE_WORKING),
        return PKG_INVALID_STATE, "Error state curr %d ", state_);
    // 先从文件尾部获取 EndCentralDir
    size_t fileLen = pkgStream_->GetFileLength();
    PKG_CHECK(fileLen > 0, return PKG_INVALID_FILE, "invalid file to load");
    PKG_CHECK(fileLen <= SIZE_MAX, return PKG_INVALID_FILE,
        "Invalid file len %zu to load %s", fileLen, pkgStream_->GetFileName().c_str());
    PKG_CHECK(fileLen >= static_cast<size_t>(sizeof(EndCentralDir)), return PKG_INVALID_FILE,
        "Too small to be zip %s", pkgStream_->GetFileName().c_str());

    // 检查最后面是签名信息还是EndCentralDir
    size_t buffSize = sizeof(EndCentralDir);
    if (buffSize < sizeof(Zip64EndCentralDirRecord)) {
        buffSize = sizeof(Zip64EndCentralDirRecord);
    }
    size_t signatureLen = 0;
    uint32_t magic = 0;
    uint32_t endDirLen = sizeof(EndCentralDir);
    size_t endDirPos = fileLen - endDirLen;
    size_t readLen = 0;
    PkgBuffer buffer(buffSize);
    int32_t ret = pkgStream_->Read(buffer, endDirPos, sizeof(EndCentralDir), readLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "read EOCD struct failed %s", pkgStream_->GetFileName().c_str());
    magic = ReadLE32(buffer.buffer);
    if (magic != END_CENTRAL_SIGNATURE) { // 按签名处理
        size_t signatureStart = 0;
        ZipPkgParse zipParse;
        ret = zipParse.ParseZipPkg(pkgStream_, signatureStart, signatureLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Parse zip package signature failed");

        endDirPos -= signatureLen;
        ret = pkgStream_->Read(buffer, endDirPos, sizeof(EndCentralDir), readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "read EOCD struct failed %s", pkgStream_->GetFileName().c_str());
    }

    return LoadPackage(fileNames, buffer, endDirLen, endDirPos, readLen);
}

int32_t ZipPkgFile::ParseFileEntries(std::vector<std::string> &fileNames,
    const EndCentralDir &endDir, size_t currentPos, size_t fileLen)
{
    int32_t ret = PKG_SUCCESS;
    size_t buffLen = MAX_FILE_NAME + sizeof(LocalFileHeader) + sizeof(DataDescriptor)
        + sizeof(CentralDirEntry) + BIG_SIZE_HEADER;
    PkgBuffer buffer(buffLen);

    for (int32_t i = 0; i < endDir.totalEntries; i++) {
        if (fileLen <= currentPos) {
            PKG_LOGE("too small to be zip");
            return PKG_INVALID_FILE;
        }

        ZipFileEntry* entry = new ZipFileEntry(this, nodeId_++);
        if (entry == nullptr) {
            PKG_LOGE("Failed to create zip node for %s", pkgStream_->GetFileName().c_str());
            return PKG_NONE_MEMORY;
        }

        // 从文件中解析出文件头信息，保存在entry中
        size_t decodeLen = 0;
        ret = entry->DecodeHeader(buffer, currentPos, 0, decodeLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("DecodeHeader failed");
            delete entry;
            return ret;
        }

        // 保存entry文件
        pkgEntryMapId_.insert(std::pair<uint32_t, PkgEntryPtr>(entry->GetNodeId(), (PkgEntryPtr)entry));
        pkgEntryMapFileName_.insert(std::pair<std::string, PkgEntryPtr>(entry->GetFileName(), (PkgEntryPtr)entry));
        fileNames.push_back(entry->GetFileName());

        currentPos += decodeLen;
    }
    return ret;
}

int32_t ZipFileEntry::EncodeHeader(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    // 对zip包，数据和数据头信息在连续位置，使用一个打包
    encodeLen = 0;
    fileInfo_.fileInfo.headerOffset = startOffset;
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::Pack(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    PkgStreamPtr outStream = pkgFile_->GetPkgStream();
    PKG_CHECK(fileInfo_.fileInfo.headerOffset == startOffset, return PKG_INVALID_PARAM,
        "Offset error %zu %zu %s", fileInfo_.fileInfo.headerOffset, startOffset, fileInfo_.fileInfo.identity.c_str());
    PKG_CHECK(algorithm != nullptr && outStream != nullptr && inStream != nullptr, return PKG_INVALID_PARAM,
        "outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());

    // 为header申请一个buff，先处理到内存，后面在写入文件
    std::vector<uint8_t> buff(MAX_FILE_NAME + sizeof(LocalFileHeader) + ZIP_PKG_ALIGNMENT_DEF);
    size_t nameLen = 0;
    PkgFile::ConvertStringToBuffer(fileInfo_.fileInfo.identity, {
        buff.data() + sizeof(LocalFileHeader), buff.capacity()
    }, nameLen);

    size_t headerLen = nameLen + sizeof(LocalFileHeader);
    bool hasDataDesc = true;
    if (fileInfo_.method == Z_DEFLATED) {
#ifndef UPDATER_UT
        hasDataDesc = false;
#endif
    }

    fileInfo_.fileInfo.dataOffset = startOffset + headerLen;
    PkgAlgorithmContext context = {
        {0, startOffset + headerLen},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        0, fileInfo_.fileInfo.digestMethod
    };
    int32_t ret = algorithm->Pack(inStream, outStream, context);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "Failed to compress for %s", fileInfo_.fileInfo.identity.c_str());
    // 填充file信息，压缩后的长度和crc
    fileInfo_.fileInfo.packedSize = context.packedSize;
    crc32_ = context.crc;

    // 构建文件头信息，从startOffset开始
    ret = EncodeLocalFileHeader(buff.data(), sizeof(LocalFileHeader), hasDataDesc, nameLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "Failed to encodeFileHeader for %s", fileInfo_.fileInfo.identity.c_str());
    PkgBuffer buffer(buff);
    ret = outStream->Write(buffer, headerLen, startOffset);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "Failed to write header for %s", fileInfo_.fileInfo.identity.c_str());

    if (hasDataDesc) { //  数据描述部分
        uint32_t encodeDataDescLen = 0;
        ret = EncodeDataDescriptor(outStream,
            startOffset + headerLen + fileInfo_.fileInfo.packedSize, encodeDataDescLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret,
            "Failed to encodeDataDescriptor for %s", fileInfo_.fileInfo.identity.c_str());
        headerLen += encodeDataDescLen;
    }
    encodeLen = headerLen + fileInfo_.fileInfo.packedSize;
    PKG_LOGI("Pack packedSize:%zu unpackedSize: %zu offset: %zu %zu",
        fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize,
        fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::EncodeCentralDirEntry(const PkgStreamPtr stream, size_t startOffset, size_t &encodeLen)
{
    std::vector<uint8_t> buff(sizeof(CentralDirEntry) + MAX_FILE_NAME);
    size_t realLen = 0;
    PkgFile::ConvertStringToBuffer(fileInfo_.fileInfo.identity, {
        buff.data() + sizeof(CentralDirEntry), buff.capacity()
    }, realLen);

    CentralDirEntry* centralDir = reinterpret_cast<CentralDirEntry*>(buff.data());
    centralDir->signature = CENTRAL_SIGNATURE;
    centralDir->versionMade = 0;
    centralDir->versionNeeded = 0;
    if (fileInfo_.method == Z_DEFLATED) {
        centralDir->flags |= GPBDD_FLAG_MASK;
    }
    centralDir->compressionMethod = static_cast<uint16_t>(fileInfo_.method);
    centralDir->crc = crc32_;
    uint16_t date;
    uint16_t time;
    ExtraTimeAndDate(fileInfo_.fileInfo.modifiedTime, date, time);
    centralDir->modifiedDate = date;
    centralDir->modifiedTime = time;
    centralDir->compressedSize = fileInfo_.fileInfo.packedSize;
    centralDir->uncompressedSize = fileInfo_.fileInfo.unpackedSize;
    centralDir->nameSize = realLen;
    centralDir->extraSize = 0;
    centralDir->commentSize = 0;
    centralDir->diskNumStart = 0;
    centralDir->internalAttr = 0;
    centralDir->externalAttr = 0;
    centralDir->localHeaderOffset = fileInfo_.fileInfo.headerOffset;
    PkgBuffer buffer(buff);
    int32_t ret = stream->Write(buffer, sizeof(CentralDirEntry) + realLen, startOffset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to write CentralDirEntry for %s", fileInfo_.fileInfo.identity.c_str());
        return ret;
    }
    encodeLen = sizeof(CentralDirEntry) + realLen;
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::EncodeLocalFileHeader(uint8_t *buffer, size_t bufferLen, bool hasDataDesc,
    size_t nameLen)
{
    if (bufferLen < sizeof(LocalFileHeader)) {
        PKG_LOGE("invalid buffer for decode");
        return PKG_INVALID_PARAM;
    }

    LocalFileHeader* header = reinterpret_cast<LocalFileHeader*>(buffer);
    header->signature = LOCAL_HEADER_SIGNATURE;
    header->versionNeeded = 0;
    header->flags = 0;
    header->compressionMethod = static_cast<uint16_t>(fileInfo_.method);
    uint16_t date;
    uint16_t time;
    ExtraTimeAndDate(fileInfo_.fileInfo.modifiedTime, date, time);
    header->modifiedDate = date;
    header->modifiedTime = time;
    header->crc = crc32_;
    header->compressedSize = fileInfo_.fileInfo.packedSize;
    header->uncompressedSize = fileInfo_.fileInfo.unpackedSize;
    header->nameSize = nameLen;
    header->extraSize = 0;
    if (hasDataDesc) {
        header->flags |= GPBDD_FLAG_MASK;
        header->compressedSize = 0u;
        header->uncompressedSize = 0u;
        header->crc = 0u;
    }
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::EncodeDataDescriptor(const PkgStreamPtr stream, size_t startOffset,
    uint32_t &encodeLen) const
{
    int32_t ret = PKG_SUCCESS;
    size_t offset = startOffset;
    DataDescriptor dataDesc = {};
    dataDesc.signature = DATA_DESC_SIGNATURE;
    dataDesc.crc = crc32_;
    dataDesc.compressedSize = fileInfo_.fileInfo.packedSize;
    dataDesc.uncompressedSize = fileInfo_.fileInfo.unpackedSize;
    PkgBuffer buffer((uint8_t *)&dataDesc, sizeof(dataDesc));
    ret = stream->Write(buffer, sizeof(dataDesc), offset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to write DataDescriptor for %s", fileInfo_.fileInfo.identity.c_str());
        return ret;
    }
    offset += sizeof(dataDesc);
    encodeLen = offset - startOffset;
    return ret;
}

/*
    0x0001     2 bytes    Tag for this "extra" block type
    Size       2 bytes    Size of this "extra" block
        Original
    Size       8 bytes    Original uncompressed file size
    Compressed
    Size       8 bytes    Size of compressed data
    Relative Header
    Offset     8 bytes    Offset of local header record
    Disk Start
    Number     4 bytes    Number of the disk on which
    this file starts
*/
int32_t ZipFileEntry::DecodeCentralDirEntry(PkgStreamPtr inStream, PkgBuffer &buffer, size_t currentPos,
    size_t &decodeLen)
{
    size_t readLen = buffer.length;
    PKG_CHECK(readLen >= sizeof(CentralDirEntry), return PKG_INVALID_PKG_FORMAT, "data not not enough %zu", readLen);
    uint32_t signature = ReadLE32(buffer.buffer + offsetof(CentralDirEntry, signature));
    PKG_CHECK(signature == CENTRAL_SIGNATURE, return PKG_INVALID_PKG_FORMAT,
        "Check centralDir signature failed 0x%x", signature);
    uint16_t nameSize = ReadLE16(buffer.buffer + offsetof(CentralDirEntry, nameSize));
    uint16_t extraSize = ReadLE16(buffer.buffer + offsetof(CentralDirEntry, extraSize));
    uint16_t commentSize = ReadLE16(buffer.buffer + offsetof(CentralDirEntry, commentSize));
    size_t currLen = sizeof(CentralDirEntry) + nameSize + extraSize + commentSize;
    PKG_CHECK(currentPos < (std::numeric_limits<size_t>::max() - currLen),
        return PKG_INVALID_PKG_FORMAT, "check centralDir len failed");

    size_t fileNameLength = nameSize;
    PKG_CHECK(nameSize < MAX_FILE_NAME, fileNameLength = MAX_FILE_NAME - 1, "file name size too longer %d", nameSize);
    PKG_CHECK(readLen >= sizeof(CentralDirEntry) + fileNameLength, return PKG_INVALID_PKG_FORMAT,
        "data not not enough %zu", readLen);
    fileInfo_.fileInfo.identity.assign(reinterpret_cast<char*>(buffer.buffer + sizeof(CentralDirEntry)),
                                       fileNameLength);
    fileInfo_.method = static_cast<int32_t>(ReadLE16(buffer.buffer + offsetof(CentralDirEntry, compressionMethod)));
    uint16_t modifiedTime = ReadLE16(buffer.buffer + offsetof(CentralDirEntry, modifiedTime));
    uint16_t modifiedDate = ReadLE16(buffer.buffer + offsetof(CentralDirEntry, modifiedDate));
    CombineTimeAndDate(fileInfo_.fileInfo.modifiedTime, modifiedTime, modifiedDate);
    crc32_ = ReadLE32(buffer.buffer + offsetof(CentralDirEntry, crc));
    fileInfo_.fileInfo.packedSize = ReadLE32(buffer.buffer + offsetof(CentralDirEntry, compressedSize));
    fileInfo_.fileInfo.unpackedSize = ReadLE32(buffer.buffer + offsetof(CentralDirEntry, uncompressedSize));
    fileInfo_.fileInfo.headerOffset = ReadLE32(buffer.buffer + offsetof(CentralDirEntry, localHeaderOffset));
    // 对于zip64，需要解析extra field
    decodeLen = currLen;
    if (extraSize <= 0) {
        return PKG_SUCCESS;
    }
    uint8_t* extraData = buffer.buffer + nameSize + sizeof(CentralDirEntry);
    uint16_t headerId = ReadLE16(extraData);
    if (headerId != 1) { // zip64 扩展
        return PKG_SUCCESS;
    }
    size_t unpackedSize = ReadLE64(extraData + sizeof(uint32_t));
    size_t packedSize = ReadLE64(extraData + sizeof(uint32_t) + sizeof(uint64_t));
    if (fileInfo_.fileInfo.packedSize == UINT_MAX || fileInfo_.fileInfo.unpackedSize == UINT_MAX) {
        fileInfo_.fileInfo.unpackedSize =
            (fileInfo_.fileInfo.unpackedSize == UINT_MAX) ? unpackedSize : fileInfo_.fileInfo.unpackedSize;
        fileInfo_.fileInfo.packedSize =
            (fileInfo_.fileInfo.packedSize == UINT_MAX) ? packedSize : fileInfo_.fileInfo.packedSize;
        fileInfo_.fileInfo.headerOffset = (fileInfo_.fileInfo.headerOffset == UINT_MAX) ?
            ReadLE64(extraData + BIG_SIZE_HEADER) : fileInfo_.fileInfo.headerOffset;
    } else if (fileInfo_.fileInfo.headerOffset == UINT_MAX) {
        fileInfo_.fileInfo.headerOffset = unpackedSize;
    }

    return PKG_SUCCESS;
}

int32_t ZipFileEntry::DecodeLocalFileHeaderCheck(PkgStreamPtr inStream, const PkgBuffer &data,
    size_t currentPos)
{
    uint16_t flags = ReadLE16(data.buffer + offsetof(LocalFileHeader, flags));
    uint32_t crc32 = ReadLE32(data.buffer + offsetof(LocalFileHeader, crc));
    uint32_t packedSize = ReadLE32(data.buffer + offsetof(LocalFileHeader, compressedSize));
    uint32_t unpackedSize = ReadLE32(data.buffer + offsetof(LocalFileHeader, uncompressedSize));
    size_t readLen = 0;
    if ((flags & GPBDD_FLAG_MASK) == GPBDD_FLAG_MASK) {
        currentPos += fileInfo_.fileInfo.packedSize;
        int ret = inStream->Read(data, currentPos, data.length, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("parse entry read centralDir failed");
            return ret;
        }
        if (readLen < sizeof(DataDescriptor)) {
            PKG_LOGE("data not not enough %zu", readLen);
            return PKG_INVALID_PKG_FORMAT;
        }

        uint32_t signature = ReadLE32(data.buffer + offsetof(DataDescriptor, signature));
        if (signature != DATA_DESC_SIGNATURE) {
            PKG_LOGE("check DataDescriptor signature failed");
            return PKG_INVALID_PKG_FORMAT;
        }
        crc32 = ReadLE32(data.buffer + offsetof(DataDescriptor, crc));
        packedSize = ReadLE32(data.buffer + offsetof(DataDescriptor, compressedSize));
        unpackedSize = ReadLE32(data.buffer + offsetof(DataDescriptor, uncompressedSize));
    }
    if (crc32_ != crc32) {
        PKG_LOGE("check crc %u %u failed", crc32_, crc32);
        return PKG_INVALID_PKG_FORMAT;
    }
    if (packedSize != UINT32_MAX) {
        if (fileInfo_.fileInfo.packedSize != static_cast<size_t>(packedSize)) {
            PKG_LOGE("check packedSize %zu %u failed", fileInfo_.fileInfo.packedSize, packedSize);
            return PKG_INVALID_PKG_FORMAT;
        }
        if (fileInfo_.fileInfo.unpackedSize != static_cast<size_t>(unpackedSize)) {
            PKG_LOGE("check unpackedSize %zu %u failed", fileInfo_.fileInfo.unpackedSize, unpackedSize);
            return PKG_INVALID_PKG_FORMAT;
        }
    }
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::DecodeLocalFileHeader(PkgStreamPtr inStream, const PkgBuffer &data, size_t currentPos,
    size_t &decodeLen)
{
    size_t readLen = 0;
    int32_t ret = inStream->Read(data, currentPos, data.length, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "parse entry read centralDir failed");
    PKG_CHECK(readLen >= sizeof(LocalFileHeader), return PKG_INVALID_PKG_FORMAT, "data not not enough %zu", readLen);
    uint32_t signature = ReadLE32(data.buffer + offsetof(LocalFileHeader, signature));
    PKG_CHECK(signature == LOCAL_HEADER_SIGNATURE,
        return PKG_INVALID_PKG_FORMAT, "check localHeader signature failed");

    uint16_t nameSize = ReadLE16(data.buffer + offsetof(LocalFileHeader, nameSize));
    uint16_t extraSize = ReadLE16(data.buffer + offsetof(LocalFileHeader, extraSize));
    size_t currLen = sizeof(LocalFileHeader) + nameSize + extraSize;
    PKG_CHECK(currentPos < (std::numeric_limits<size_t>::max() - currLen),
        return PKG_INVALID_PKG_FORMAT, "check centralDir len failed");
    size_t fileNameLength = nameSize;
    PKG_CHECK(nameSize < MAX_FILE_NAME, fileNameLength = MAX_FILE_NAME - 1, "file name size too longer %d", nameSize);
    PKG_CHECK(readLen >= sizeof(LocalFileHeader) + fileNameLength,
        return PKG_INVALID_PKG_FORMAT, "data not not enough %zu", readLen);
    std::string fileName(reinterpret_cast<char*>(data.buffer + sizeof(LocalFileHeader)), fileNameLength);
    uint16_t compressionMethod = ReadLE16(data.buffer + offsetof(LocalFileHeader, compressionMethod));
    fileInfo_.method = static_cast<int32_t>(compressionMethod);
    fileInfo_.level = Z_BEST_COMPRESSION;
    fileInfo_.method = Z_DEFLATED;
    fileInfo_.windowBits = -MAX_WBITS;
    fileInfo_.memLevel = DEF_MEM_LEVEL;
    fileInfo_.strategy = Z_DEFAULT_STRATEGY;
    PKG_CHECK(!fileInfo_.fileInfo.identity.compare(fileName), return PKG_INVALID_PKG_FORMAT,
        "check file name %s %s failed", fileInfo_.fileInfo.identity.c_str(), fileName.c_str());
    fileName_.assign(fileInfo_.fileInfo.identity);
    decodeLen = currLen;

    // 检查解析的是否正确
    ret = DecodeLocalFileHeaderCheck(inStream, data, currentPos + currLen);
    if (ret != PKG_SUCCESS) {
        return ret;
    }
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::Unpack(PkgStreamPtr outStream)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    if (algorithm == nullptr) {
        PKG_LOGE("ZipFileEntry::Unpack : can not algorithm for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }

    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    if (outStream == nullptr || inStream == nullptr) {
        PKG_LOGE("ZipFileEntry::Unpack : outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithmContext context = {
        {this->fileInfo_.fileInfo.dataOffset, 0},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        crc32_, fileInfo_.fileInfo.digestMethod
    };
    int32_t ret = algorithm->Unpack(inStream, outStream, context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to decompress for %s", fileInfo_.fileInfo.identity.c_str());
        return ret;
    }
    PKG_LOGI("packedSize: %zu unpackedSize: %zu  offset header: %zu data: %zu", fileInfo_.fileInfo.packedSize,
        fileInfo_.fileInfo.unpackedSize, fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);
    outStream->Flush(fileInfo_.fileInfo.unpackedSize);
    algorithm->UpdateFileInfo(&fileInfo_.fileInfo);
    return PKG_SUCCESS;
}

void ZipFileEntry::CombineTimeAndDate(time_t &time, uint16_t modifiedTime, uint16_t modifiedDate) const
{
    struct tm newTime;
    newTime.tm_year = ((modifiedDate >> TM_YEAR_BITS) & 0x7f) + START_YEAR; // 年，tm_year为int临时变量减去1900。
    newTime.tm_mon = (modifiedDate >> TM_MON_BITS) & 0xf; // 月，tm_mon为int临时变量减去1。
    newTime.tm_mday = modifiedDate & 0x1f;         // 日。
    newTime.tm_hour = (modifiedTime >> TM_HOUR_BITS) & 0x1f; // 时。
    newTime.tm_min = (modifiedTime >> TM_MIN_BITS) & 0x2f;   // 分。
    newTime.tm_sec = (modifiedTime << 1) & 0x1f;   // 秒。
    newTime.tm_isdst = 0;                          // 非夏令时。
    time = mktime(&newTime);                      // 将tm结构体转换成time_t格式。
}

int32_t ZipFileEntry::DecodeHeader(const PkgBuffer &buffer, size_t headOffset, size_t dataOffset,
    size_t &decodeLen)
{
    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    if (inStream == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }

    if (headOffset >= (std::numeric_limits<size_t>::max() - buffer.length)) {
        PKG_LOGE("check centralDir len failed");
        return PKG_INVALID_PKG_FORMAT;
    }
    size_t readLen = 0;
    int32_t ret = inStream->Read(buffer, headOffset, buffer.length, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("parse entry read centralDir failed");
        return ret;
    }
    PkgBuffer centralBuff(buffer.buffer, readLen);
    ret = DecodeCentralDirEntry(inStream, centralBuff, headOffset, decodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("decode CentralDir failed");
        return ret;
    }

    size_t headerLen = 0;
    ret = DecodeLocalFileHeader(inStream, buffer, fileInfo_.fileInfo.headerOffset, headerLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("decode LocalFileHeader failed");
        return ret;
    }
    fileInfo_.fileInfo.packMethod = PKG_DIGEST_TYPE_CRC;
    fileInfo_.fileInfo.digestMethod = PKG_COMPRESS_METHOD_ZIP;
    fileInfo_.fileInfo.dataOffset = fileInfo_.fileInfo.headerOffset + headerLen;
    PKG_LOGI("packedSize: %zu unpackedSize: %zu  offset header: %zu data: %zu %s",
        fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize,
        fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset, fileInfo_.fileInfo.identity.c_str());
    return PKG_SUCCESS;
}

int32_t ZipFileEntry::Init(const PkgManager::FileInfoPtr fileInfo, PkgStreamPtr inStream)
{
    fileInfo_.level = Z_BEST_COMPRESSION;
    fileInfo_.method = Z_DEFLATED;
    fileInfo_.windowBits = -MAX_WBITS;
    fileInfo_.memLevel = DEF_MEM_LEVEL;
    fileInfo_.strategy = Z_DEFAULT_STRATEGY;
    int32_t ret = PkgEntry::Init(&fileInfo_.fileInfo, fileInfo, inStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed to check input param");
        return PKG_INVALID_PARAM;
    }
    ZipFileInfo* info = reinterpret_cast<ZipFileInfo*>(fileInfo);
    if (info != nullptr && info->method != -1) {
        fileInfo_.level = info->level;
        fileInfo_.memLevel = info->memLevel;
        fileInfo_.method = info->method;
        fileInfo_.strategy = info->strategy;
        fileInfo_.windowBits = info->windowBits;
    }
    return PKG_SUCCESS;
}
} // namespace Hpackage
