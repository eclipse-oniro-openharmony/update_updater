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

#include "image_patch.h"
#include <memory>
#include <string>
#include <vector>
#include "diffpatch.h"
#include "lz4_adapter.h"
#include "openssl/sha.h"
#include "securec.h"
#include "zip_adapter.h"

using namespace Hpackage;

namespace UpdatePatch {
uint32_t g_tmpFileId = 0;

int32_t NormalImagePatch::ApplyImagePatch(const PatchParam &param, size_t &startOffset)
{
    size_t offset = startOffset;
    if (offset + PATCH_NORMAL_MIN_HEADER_LEN > param.patchSize) {
        PATCH_LOGE("Failed to check datalen");
        return -1;
    }

    size_t srcStart = static_cast<size_t>(ReadLE<int64_t>(param.patch + offset));
    offset += sizeof(int64_t);
    size_t srcLen = static_cast<size_t>(ReadLE<int64_t>(param.patch + offset));
    offset += sizeof(int64_t);
    size_t patchOffset = static_cast<size_t>(ReadLE<int64_t>(param.patch + offset));
    offset += sizeof(int64_t);
    if (srcStart + srcLen > param.oldSize) {
        PATCH_LOGE("error, srcStart: %zu srcLen: %zu , param.oldSize: %zu, patchOffset: %zu",
            srcStart, srcLen, param.oldSize, patchOffset);
        return -1;
    }

    PatchBuffer patchInfo = {param.patch, patchOffset, param.patchSize};
    BlockBuffer oldInfo = {param.oldBuff + srcStart, srcLen};
    int32_t ret = UpdateApplyPatch::ApplyBlockPatch(patchInfo, oldInfo, writer_);
    if (ret != 0) {
        PATCH_LOGE("Failed to apply bsdiff patch");
        return -1;
    }
    startOffset = offset;
    return 0;
}

int32_t RowImagePatch::ApplyImagePatch(const PatchParam &param, size_t &startOffset)
{
    size_t offset = startOffset;
    if (offset + sizeof(int32_t) > param.patchSize) {
        PATCH_LOGE("Failed to check datalen");
        return -1;
    }
    size_t dataLen = static_cast<size_t>(ReadLE<uint32_t>(param.patch + offset));
    if (offset + dataLen > param.patchSize) {
        PATCH_LOGE("Failed to check datalen");
        return -1;
    }
    offset += sizeof(uint32_t);

    BlockBuffer data = {param.patch + offset, dataLen};
    int32_t ret = writer_->Write(0, data, dataLen);
    if (ret != 0) {
        PATCH_LOGE("Failed to write chunk");
        return -1;
    }
    PATCH_LOGI("RowImagePatch startOffset %zu dataLen %zu", startOffset, dataLen);
    PATCH_DEBUG("ApplyImagePatch hash %zu %s",  dataLen, GeneraterBufferHash(data).c_str());
    startOffset = offset + dataLen;
    return 0;
}

int32_t CompressedImagePatch::StartReadHeader(const PatchParam &param, PatchHeader &header, size_t &offset)
{
    int32_t ret = ReadHeader(param, header, offset);
    if (ret != 0) {
        PATCH_LOGE("Failed to read header");
        return -1;
    }
    PATCH_LOGI("ApplyImagePatch srcStart %zu srcLen %zu patchOffset: %zu expandedLen:%zu %zu",
        header.srcStart, header.srcLength, header.patchOffset, header.expandedLen, header.targetSize);
    if (header.srcStart + header.srcLength > param.oldSize) {
        PATCH_LOGE("Failed to check patch");
        return -1;
    }
    return 0;
}

int32_t CompressedImagePatch::ApplyImagePatch(const PatchParam &param, size_t &startOffset)
{
    size_t offset = startOffset;
    // read header
    PatchHeader header {};
    if (StartReadHeader(param, header, offset) != 0) {
        return -1;
    }
    // decompress old data
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    Hpackage::PkgManager::StreamPtr stream = nullptr;
    BlockBuffer oldData = { param.oldBuff + header.srcStart, header.srcLength };
    if (DecompressData(pkgManager, oldData, stream, true, header.expandedLen) != 0) {
        PATCH_LOGE("Failed to decompress data");
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        return -1;
    }
    // prepare new data
    std::unique_ptr<Hpackage::FileInfo> info = GetFileInfo();
    if (info == nullptr) {
        PATCH_LOGE("Failed to get file info");
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        return -1;
    }
    info->packedSize = header.targetSize;
    info->unpackedSize = header.expandedLen;
    std::unique_ptr<CompressedFileRestore> zipWriter = std::make_unique<CompressedFileRestore>(info.get(), writer_);
    if (zipWriter == nullptr || zipWriter->Init() != 0) {
        PATCH_LOGE("Failed to create zip writer");
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        return -1;
    }
    // apply patch
    PatchBuffer patchInfo = {param.patch, header.patchOffset, param.patchSize};
    if (UpdateApplyPatch::ApplyBlockPatch(patchInfo, stream, zipWriter.get()) != 0) {
        PATCH_LOGE("Failed to apply bsdiff patch");
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        return -1;
    }
    // compress new data
    size_t originalSize = 0;
    size_t compressSize = 0;
    zipWriter->CompressData(originalSize, compressSize);
    PATCH_LOGI("ApplyImagePatch unpackedSize %zu %zu", originalSize, compressSize);
    if (originalSize != header.targetSize) {
        PATCH_LOGE("Failed to apply bsdiff patch");
        Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
        return -1;
    }
    startOffset = offset;
    Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
    return 0;
}

int32_t CompressedImagePatch::DecompressData(Hpackage::PkgManager::PkgManagerPtr &pkgManager, PkgBuffer buffer,
    Hpackage::PkgManager::StreamPtr &stream, bool memory, size_t expandedLen) const
{
    if (expandedLen == 0) {
        PATCH_LOGE("Decompress data is null");
        return 0;
    }
    std::unique_ptr<Hpackage::FileInfo> info = GetFileInfo();
    if (pkgManager == nullptr || info == nullptr) {
        PATCH_LOGE("Failed to get pkg manager or file info");
        return -1;
    }

    info->packedSize = buffer.length;
    info->unpackedSize = expandedLen;
    info->identity = std::to_string(g_tmpFileId++);

    // 申请内存stream，用于解压老文件
    int32_t ret = pkgManager->CreatePkgStream(stream, info->identity,
        expandedLen, memory ? PkgStream::PkgStreamType_MemoryMap : PkgStream::PkgStreamType_Write);
    if (stream == nullptr) {
        PATCH_LOGE("Failed to create stream");
        return -1;
    }

    ret = pkgManager->DecompressBuffer(info.get(), buffer, stream);
    if (ret != 0) {
        pkgManager->ClosePkgStream(stream);
        PATCH_LOGE("Can not decompress buff");
        return -1;
    }

    if (bonusData_.size() == 0) {
        return 0;
    }
    if (info->unpackedSize > (expandedLen - bonusData_.size())) {
        PATCH_LOGE("Source inflation short");
        return -1;
    }
    if (memory) { // not support for none memory
        PkgBuffer memBuffer;
        if (stream->GetBuffer(memBuffer) != 0) {
            pkgManager->ClosePkgStream(stream);
            PATCH_LOGE("Can not get memory buff");
            return -1;
        }
        ret = memcpy_s(memBuffer.buffer + info->unpackedSize,
            expandedLen - info->unpackedSize, bonusData_.data(), bonusData_.size());
    }
    return ret;
}

int32_t ZipImagePatch::ReadHeader(const PatchParam &param, PatchHeader &header, size_t &offset)
{
    if (offset + PATCH_DEFLATE_MIN_HEADER_LEN > param.patchSize) {
        PATCH_LOGE("Failed to check datalen");
        return -1;
    }
    header.srcStart = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.srcLength = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.patchOffset = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.expandedLen = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.targetSize = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);

    level_ = ReadLE<int32_t>(param.patch + offset);
    offset += sizeof(int32_t);
    method_ = ReadLE<int32_t>(param.patch + offset);
    offset += sizeof(int32_t);
    windowBits_ = ReadLE<int32_t>(param.patch + offset);
    offset += sizeof(int32_t);
    memLevel_ = ReadLE<int32_t>(param.patch + offset);
    offset += sizeof(int32_t);
    strategy_ = ReadLE<int32_t>(param.patch + offset);
    offset += sizeof(int32_t);

    PATCH_LOGI("ZipImagePatch::ReadHeader level_:%d method_:%d windowBits_:%d memLevel_:%d strategy_:%d",
        level_, method_, windowBits_, memLevel_, strategy_);
    return 0;
}

std::unique_ptr<Hpackage::FileInfo> ZipImagePatch::GetFileInfo() const
{
    Hpackage::ZipFileInfo *fileInfo = new(std::nothrow) ZipFileInfo;
    if (fileInfo == nullptr) {
        PATCH_LOGE("Failed to new file info");
        return nullptr;
    }
    fileInfo->fileInfo.packMethod = PKG_COMPRESS_METHOD_ZIP;
    fileInfo->fileInfo.digestMethod = PKG_DIGEST_TYPE_NONE;
    fileInfo->fileInfo.packedSize = 0;
    fileInfo->fileInfo.unpackedSize = 0;
    fileInfo->fileInfo.identity = std::to_string(g_tmpFileId++);
    fileInfo->level = level_;
    fileInfo->method = method_;
    fileInfo->windowBits = windowBits_;
    fileInfo->memLevel = memLevel_;
    fileInfo->strategy = strategy_;
    return std::unique_ptr<Hpackage::FileInfo>((FileInfo *)fileInfo);
}

int32_t Lz4ImagePatch::ReadHeader(const PatchParam &param, PatchHeader &header, size_t &offset)
{
    if (offset + PATCH_LZ4_MIN_HEADER_LEN > param.patchSize) {
        PATCH_LOGE("Failed to check datalen");
        return -1;
    }
    header.srcStart = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.srcLength = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.patchOffset = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.expandedLen = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);
    header.targetSize = static_cast<size_t>(ReadLE<uint64_t>(param.patch + offset));
    offset += sizeof(uint64_t);

    compressionLevel_ = static_cast<int32_t>(ReadLE<int32_t>(param.patch + offset));
    offset += sizeof(int32_t);
    method_ = static_cast<int32_t>(ReadLE<int32_t>(param.patch + offset));
    offset += sizeof(int32_t);
    blockIndependence_ = static_cast<int32_t>(ReadLE<int32_t>(param.patch + offset));
    offset += sizeof(int32_t);
    contentChecksumFlag_ = static_cast<int32_t>(ReadLE<int32_t>(param.patch + offset));
    offset += sizeof(int32_t);
    blockSizeID_ = static_cast<int32_t>(ReadLE<int32_t>(param.patch + offset));
    offset += sizeof(int32_t);
    autoFlush_ = static_cast<int32_t>(ReadLE<int32_t>(param.patch + offset));
    offset += sizeof(int32_t);
    PATCH_LOGI("ReadHeader BLOCK_LZ4 level_:%d method_:%d %d contentChecksumFlag_:%d blockSizeID_:%d %d",
        compressionLevel_, method_, blockIndependence_, contentChecksumFlag_, blockSizeID_, autoFlush_);
    return 0;
}

std::unique_ptr<Hpackage::FileInfo> Lz4ImagePatch::GetFileInfo() const
{
    Hpackage::Lz4FileInfo *fileInfo = new(std::nothrow) Lz4FileInfo;
    if (fileInfo == nullptr) {
        PATCH_LOGE("Failed to new file info");
        return nullptr;
    }
    fileInfo->fileInfo.packMethod = (method_ == LZ4B_MAGIC) ? PKG_COMPRESS_METHOD_LZ4_BLOCK : PKG_COMPRESS_METHOD_LZ4;
    fileInfo->fileInfo.digestMethod = PKG_DIGEST_TYPE_NONE;
    fileInfo->fileInfo.packedSize = 0;
    fileInfo->fileInfo.unpackedSize = 0;
    fileInfo->fileInfo.identity = std::to_string(g_tmpFileId++);
    fileInfo->compressionLevel = static_cast<int8_t>(compressionLevel_);
    fileInfo->blockIndependence = static_cast<int8_t>(blockIndependence_);
    fileInfo->contentChecksumFlag = static_cast<int8_t>(contentChecksumFlag_);
    fileInfo->blockSizeID = static_cast<int8_t>(blockSizeID_);
    fileInfo->autoFlush = static_cast<int8_t>(autoFlush_);
    return std::unique_ptr<Hpackage::FileInfo>((FileInfo *)fileInfo);
}

int32_t CompressedFileRestore::Init()
{
    SHA256_Init(&sha256Ctx_);
    if (fileInfo_->packMethod == PKG_COMPRESS_METHOD_ZIP) {
        deflateAdapter_.reset(new ZipAdapter(writer_, 0, fileInfo_));
    } else if (fileInfo_->packMethod == PKG_COMPRESS_METHOD_LZ4) {
        deflateAdapter_.reset(new Lz4FrameAdapter(writer_, 0, fileInfo_));
    } else if (fileInfo_->packMethod == PKG_COMPRESS_METHOD_LZ4_BLOCK) {
        deflateAdapter_.reset(new Lz4BlockAdapter(writer_, 0, fileInfo_));
    }
    if (deflateAdapter_ == nullptr) {
        PATCH_LOGE("Failed to create zip adapter");
        return -1;
    }
    return deflateAdapter_->Open();
}

int32_t CompressedFileRestore::Write(size_t start, const BlockBuffer &buffer, size_t size)
{
    if (size == 0) {
        return 0;
    }
    dataSize_ += size;
    SHA256_Update(&sha256Ctx_, buffer.buffer, size);
    BlockBuffer data = { buffer.buffer, size };
    return deflateAdapter_->WriteData(data);
}

int32_t CompressedFileRestore::CompressData(size_t &originalSize, size_t &compressSize)
{
    int32_t ret = deflateAdapter_->FlushData(compressSize);
    if (ret != 0) {
        PATCH_LOGE("Failed to flush data");
        return -1;
    }
    originalSize = dataSize_;

    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    SHA256_Final(digest.data(), &sha256Ctx_);
    BlockBuffer buffer = { digest.data(), digest.size() };
    std::string hexDigest = ConvertSha256Hex(buffer);
    PATCH_LOGI("CompressedFileRestore hash %zu %s ", dataSize_, hexDigest.c_str());
    return 0;
}
} // namespace UpdatePatch
