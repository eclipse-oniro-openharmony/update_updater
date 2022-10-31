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
#include "pkg_gzipfile.h"

using namespace std;

namespace Hpackage {
/* gzip flag byte */
constexpr uint16_t HEADER_CRC = 0x02; /* bit 1 set: CRC16 for the gzip header */
constexpr uint16_t EXTRA_FIELD = 0x04; /* bit 2 set: extra field present */
constexpr uint16_t ORIG_NAME = 0x08; /* bit 3 set: original file name present */
constexpr uint16_t COMMENT = 0x10; /* bit 4 set: file comment present */
constexpr uint16_t ENCRYPTED = 0x20; /* bit 5 set: file is encrypted */
constexpr int32_t DEF_MEM_LEVEL = 8;
constexpr uint16_t GZIP_MAGIC = 0x8b1f;
constexpr int32_t BUFFER_SIZE = 1024;
#ifdef SUPPORT_EXTRA_FIELD
constexpr int32_t EXTRA_FIELD_LEN = 20;
#endif
constexpr int32_t BLOCK_SIZE = 8;

/*
    Each member has the following structure:
     +---+---+---+---+---+---+---+---+---+---+
     |ID1|ID2|CM |FLG|     MTIME     |XFL|OS | (more-->)
     +---+---+---+---+---+---+---+---+---+---+

    (if FLG.FEXTRA set)

     +---+---+=================================+
     | XLEN  |...XLEN bytes of "extra field"...| (more-->)
     +---+---+=================================+

    (if FLG.FNAME set)

     +=========================================+
     |...original file name, zero-terminated...| (more-->)
     +=========================================+

    (if FLG.FCOMMENT set)

     +===================================+
     |...file comment, zero-terminated...| (more-->)
     +===================================+

    (if FLG.FHCRC set)

     +---+---+
     | CRC16 |
     +---+---+
 */
int32_t GZipFileEntry::EncodeHeader(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    PkgStreamPtr outStream = pkgFile_->GetPkgStream();
    PKG_CHECK(outStream != nullptr, return PKG_INVALID_PARAM,
        "Check outstream fail %s", fileInfo_.fileInfo.identity.c_str());
    size_t offset = 0;
    PkgBuffer buffer(BUFFER_SIZE);
    GZipHeader *header = reinterpret_cast<GZipHeader *>(buffer.buffer);
    header->magic = GZIP_MAGIC;
    header->method = Z_DEFLATED;
    header->flags = 0;
    header->mtime = fileInfo_.fileInfo.modifiedTime;
    offset += sizeof(GZipHeader);
#ifdef SUPPORT_EXTRA_FIELD
    header->flags |= EXTRA_FIELD;
    {
        WriteLE16(buffer.buffer + offset, EXTRA_FIELD_LEN);
        offset += sizeof(uint16_t) + EXTRA_FIELD_LEN;
    }
#endif
    header->flags |= ORIG_NAME;
    {
        size_t fileNameLen = 0;
        PkgFile::ConvertStringToBuffer(
            fileInfo_.fileInfo.identity, {buffer.buffer + offset, buffer.length - offset}, fileNameLen);
        offset += fileNameLen;
        buffer.buffer[offset] = 0;
        offset += 1;
    }
#ifdef SUPPORT_EXTRA_FIELD
    header->flags |= COMMENT;
    {
        size_t fileNameLen = 0;
        PkgFile::ConvertStringToBuffer(
            fileInfo_.fileInfo.identity, {buffer.buffer + offset, buffer.length - offset}, fileNameLen);
        offset += fileNameLen;
        buffer.buffer[offset] = 0;
        offset += 1;
    }
#endif
    fileInfo_.fileInfo.headerOffset = startOffset;
    fileInfo_.fileInfo.dataOffset = startOffset + offset;
    int32_t ret = outStream->Write(buffer, offset, startOffset);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail write header for %s", fileInfo_.fileInfo.identity.c_str());
    encodeLen = offset;
    return PKG_SUCCESS;
}

int32_t GZipFileEntry::Pack(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    PkgStreamPtr outStream = pkgFile_->GetPkgStream();
    if (fileInfo_.fileInfo.dataOffset != startOffset) {
        PKG_LOGE("start offset error for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }
    if (algorithm == nullptr || outStream == nullptr || inStream == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }
    fileInfo_.fileInfo.dataOffset = startOffset;
    PkgAlgorithmContext context = {
        {0, startOffset},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        0, fileInfo_.fileInfo.digestMethod
    };
    int32_t ret = algorithm->Pack(inStream, outStream, context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Compress for %s", fileInfo_.fileInfo.identity.c_str());
        return ret;
    }
    fileInfo_.fileInfo.packedSize = context.packedSize;

    /*
    0   1   2   3   4   5   6   7
    +---+---+---+---+---+---+---+---+
    |     CRC32     |     ISIZE     |
    +---+---+---+---+---+---+---+---+
    */
    PkgBuffer buffer(BLOCK_SIZE);
    WriteLE32(buffer.buffer, context.crc);
    WriteLE32(buffer.buffer + sizeof(uint32_t), fileInfo_.fileInfo.unpackedSize);
    ret = outStream->Write(buffer, BLOCK_SIZE, fileInfo_.fileInfo.dataOffset + fileInfo_.fileInfo.packedSize);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write header for %s", fileInfo_.fileInfo.identity.c_str());
        return ret;
    }
    encodeLen = fileInfo_.fileInfo.packedSize + BLOCK_SIZE;
    PKG_LOGI("Pack packedSize:%zu unpackedSize: %zu offset: %zu %zu", fileInfo_.fileInfo.packedSize,
        fileInfo_.fileInfo.unpackedSize, fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);
    return PKG_SUCCESS;
}

int32_t GZipFileEntry::Unpack(PkgStreamPtr outStream)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    PKG_CHECK(algorithm != nullptr, return PKG_INVALID_PARAM, "can not algorithm for %s",
        fileInfo_.fileInfo.identity.c_str());

    PKG_LOGI("packedSize: %zu unpackedSize: %zu  offset header: %zu data: %zu", fileInfo_.fileInfo.packedSize,
        fileInfo_.fileInfo.unpackedSize, fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);

    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    PKG_CHECK(outStream != nullptr && inStream != nullptr, return PKG_INVALID_PARAM,
        "outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());
    PkgAlgorithmContext context = {
        {fileInfo_.fileInfo.dataOffset, 0},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        0, fileInfo_.fileInfo.digestMethod
    };
    int32_t ret = algorithm->Unpack(inStream, outStream, context);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail Decompress for %s", fileInfo_.fileInfo.identity.c_str());

    // Get uncompressed size
    size_t readLen = 0;
    fileInfo_.fileInfo.packedSize = context.packedSize;
    PkgBuffer buffer(BLOCK_SIZE); // Read last 8 bytes at the end of package
    ret = inStream->Read(buffer, context.packedSize + fileInfo_.fileInfo.dataOffset, BLOCK_SIZE, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail to read file %s", inStream->GetFileName().c_str());
    crc32_ = ReadLE32(buffer.buffer);
    fileInfo_.fileInfo.unpackedSize = ReadLE32(buffer.buffer + sizeof(uint32_t));
    PKG_CHECK(crc32_ == context.crc, return ret, "Crc error %u %u", crc32_, context.crc);
    PKG_CHECK(fileInfo_.fileInfo.unpackedSize == context.unpackedSize,
        return ret, "Crc error %u %u", crc32_, context.crc);

    PKG_LOGI("packedSize: %zu unpackedSize: %zu  offset header: %zu data: %zu", fileInfo_.fileInfo.packedSize,
        fileInfo_.fileInfo.unpackedSize, fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);
    outStream->Flush(fileInfo_.fileInfo.unpackedSize);
    algorithm->UpdateFileInfo(&fileInfo_.fileInfo);
    return PKG_SUCCESS;
}

void GZipFileEntry::DecodeHeaderCalOffset(uint8_t flags, const PkgBuffer &buffer, size_t &offset,
    std::string &fileName) const
{
    if (flags & EXTRA_FIELD) {
        uint16_t extLen = ReadLE16(buffer.buffer + offset);
        offset += sizeof(uint16_t) + extLen;
    }
    if (flags & ORIG_NAME) {
        PkgFile::ConvertBufferToString(fileName, {buffer.buffer + offset, buffer.length - offset});
        offset += fileName.size() + 1;
    }
    if (flags & COMMENT) {
        std::string comment;
        PkgFile::ConvertBufferToString(comment, {buffer.buffer + offset, buffer.length - offset});
        offset += comment.size() + 1;
    }
    if (flags & HEADER_CRC) { // Skip CRC
        offset += sizeof(uint16_t);
    }
    return;
}

int32_t GZipFileEntry::DecodeHeader(const PkgBuffer &buffer, size_t headerOffset, size_t dataOffset,
    size_t &decodeLen)
{
    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    if (inStream == nullptr || buffer.buffer == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }
    size_t offset = sizeof(GZipHeader);

    uint8_t flags = *(buffer.buffer + offsetof(GZipHeader, flags));

    DecodeHeaderCalOffset(flags, buffer, offset, fileName_);
    if (fileName_.empty()) {
        fileInfo_.fileInfo.identity = "gzip_";
        fileInfo_.fileInfo.identity.append(std::to_string(nodeId_));
        fileName_ = fileInfo_.fileInfo.identity;
    } else {
        fileInfo_.fileInfo.identity = fileName_;
    }
    fileInfo_.fileInfo.digestMethod = PKG_DIGEST_TYPE_CRC;
    fileInfo_.fileInfo.packMethod = PKG_COMPRESS_METHOD_GZIP;
    fileInfo_.level = Z_BEST_COMPRESSION;
    fileInfo_.method = Z_DEFLATED;
    fileInfo_.windowBits = -MAX_WBITS;
    fileInfo_.memLevel = DEF_MEM_LEVEL;
    fileInfo_.strategy = Z_DEFAULT_STRATEGY;

    fileInfo_.fileInfo.headerOffset = headerOffset;
    fileInfo_.fileInfo.dataOffset = headerOffset + offset;
    // Read data length here.
    // The length read here maybe incorrect, so should adjust it
    // when unpack.
    size_t readLen = 0;
    size_t blockOffset = inStream->GetFileLength() - BLOCK_SIZE;
    int32_t ret = inStream->Read(buffer, blockOffset, buffer.length, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to read file %s", inStream->GetFileName().c_str());
        return ret;
    }
    fileInfo_.fileInfo.unpackedSize = ReadLE32(buffer.buffer + sizeof(uint32_t));
    fileInfo_.fileInfo.packedSize = blockOffset - fileInfo_.fileInfo.dataOffset;
    PKG_LOGI("GZipFileEntry::DecodeHeader dataOffset %zu, packedSize: %zu %zu", fileInfo_.fileInfo.dataOffset,
        fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize);
    decodeLen = offset;
    return PKG_SUCCESS;
}

int32_t GZipPkgFile::AddEntry(const PkgManager::FileInfoPtr file, const PkgStreamPtr inStream)
{
    if (file == nullptr || inStream == nullptr) {
        PKG_LOGE("Fail to check input param");
        return PKG_INVALID_PARAM;
    }
    if (!CheckState({PKG_FILE_STATE_IDLE, PKG_FILE_STATE_WORKING}, PKG_FILE_STATE_CLOSE)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    PKG_LOGI("Add file %s to package", file->identity.c_str());

    GZipFileEntry *entry = static_cast<GZipFileEntry *>(AddPkgEntry(file->identity));
    if (entry == nullptr) {
        PKG_LOGE("Fail create pkg node for %s", file->identity.c_str());
        return PKG_NONE_MEMORY;
    }
    int32_t ret = entry->Init(file, inStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail init entry for %s", file->identity.c_str());
        return ret;
    }

    size_t encodeLen = 0;
    ret = entry->EncodeHeader(inStream, currentOffset_, encodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail encode header for %s", file->identity.c_str());
        return ret;
    }
    currentOffset_ += encodeLen;
    ret = entry->Pack(inStream, currentOffset_, encodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Pack for %s", file->identity.c_str());
        return ret;
    }
    currentOffset_ += encodeLen;
    pkgStream_->Flush(currentOffset_);
    return PKG_SUCCESS;
}

int32_t GZipPkgFile::SavePackage(size_t &offset)
{
    AddSignData(pkgInfo_.digestMethod, currentOffset_, offset);
    return PKG_SUCCESS;
}

int32_t GZipPkgFile::LoadPackage(std::vector<std::string> &fileNames, VerifyFunction verifier)
{
    UNUSED(verifier);
    if (!CheckState({ PKG_FILE_STATE_IDLE }, PKG_FILE_STATE_WORKING)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    PKG_LOGI("LoadPackage %s ", pkgStream_->GetFileName().c_str());
    size_t srcOffset = 0;
    size_t readLen = 0;
    PkgBuffer buffer(BUFFER_SIZE);
    int32_t ret = pkgStream_->Read(buffer, srcOffset, buffer.length, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to read file %s", pkgStream_->GetFileName().c_str());
        return ret;
    }

    GZipHeader *header = reinterpret_cast<GZipHeader *>(buffer.buffer);
    // Check magic number
    if (header->magic != GZIP_MAGIC) {
        PKG_LOGE("Invalid gzip file %s", pkgStream_->GetFileName().c_str());
        return PKG_INVALID_FILE;
    }
    // Does not support encryption
    if ((header->flags & ENCRYPTED) != 0) {
        PKG_LOGE("Not support encrypted ");
        return PKG_INVALID_FILE;
    }

    GZipFileEntry *entry = new GZipFileEntry(this, nodeId_++);
    if (entry == nullptr) {
        PKG_LOGE("Fail create gzip node for %s", pkgStream_->GetFileName().c_str());
        return PKG_LZ4_FINISH;
    }
    ret = entry->DecodeHeader(buffer, srcOffset, srcOffset, readLen);
    srcOffset += readLen;

    // Save entry
    pkgEntryMapId_.insert(std::pair<uint32_t, PkgEntryPtr>(entry->GetNodeId(), entry));
    pkgEntryMapFileName_.insert(std::pair<std::string, PkgEntryPtr>(entry->GetFileName(), entry));
    fileNames.push_back(entry->GetFileName());
    return ret;
}
} // namespace Hpackage
