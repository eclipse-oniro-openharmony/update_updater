/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pkg_lz4file.h"
#include "pkg_algo_lz4.h"

using namespace std;

namespace Hpackage {
int32_t Lz4FileEntry::Init(const PkgManager::FileInfoPtr fileInfo, PkgStreamPtr inStream)
{
    int32_t ret = PkgEntry::Init(&fileInfo_.fileInfo, fileInfo, inStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to check input param");
        return PKG_INVALID_PARAM;
    }
    Lz4FileInfo *info = reinterpret_cast<Lz4FileInfo *>(fileInfo);
    if (info != nullptr) {
        fileInfo_.compressionLevel = info->compressionLevel;
        fileInfo_.blockIndependence = info->blockIndependence;
        fileInfo_.blockSizeID = info->blockSizeID;
        fileInfo_.contentChecksumFlag = info->contentChecksumFlag;
    }
    return PKG_SUCCESS;
}

int32_t Lz4FileEntry::EncodeHeader(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    encodeLen = 0;
    fileInfo_.fileInfo.headerOffset = startOffset;
    fileInfo_.fileInfo.dataOffset = startOffset;
    return PKG_SUCCESS;
}

int32_t Lz4FileEntry::Pack(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    PkgStreamPtr outStream = pkgFile_->GetPkgStream();
    if (fileInfo_.fileInfo.headerOffset != startOffset) {
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
    encodeLen = fileInfo_.fileInfo.packedSize;
    PKG_LOGI("Pack packedSize:%zu unpackedSize: %zu offset: %zu %zu", fileInfo_.fileInfo.packedSize,
        fileInfo_.fileInfo.unpackedSize, fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);
    return PKG_SUCCESS;
}

int32_t Lz4FileEntry::Unpack(PkgStreamPtr outStream)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    if (algorithm == nullptr) {
        PKG_LOGE("Lz4FileEntry::Unpack : can not algorithm for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }

    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    if (outStream == nullptr || inStream == nullptr) {
        PKG_LOGE("Lz4FileEntry::Unpack : outStream or inStream null for %s", fileInfo_.fileInfo.identity.c_str());
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithmContext context = {
        {fileInfo_.fileInfo.dataOffset, 0},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        0, fileInfo_.fileInfo.digestMethod
    };
    int32_t ret = algorithm->Unpack(inStream, outStream, context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Failed decompress for %s", fileInfo_.fileInfo.identity.c_str());
        return ret;
    }
    fileInfo_.fileInfo.packedSize = context.packedSize;
    fileInfo_.fileInfo.unpackedSize = context.unpackedSize;
    PKG_LOGI("packedSize: %zu unpackedSize: %zu  offset header: %zu data: %zu", fileInfo_.fileInfo.packedSize,
        fileInfo_.fileInfo.unpackedSize, fileInfo_.fileInfo.headerOffset, fileInfo_.fileInfo.dataOffset);
    outStream->Flush(fileInfo_.fileInfo.unpackedSize);
    algorithm->UpdateFileInfo(&fileInfo_.fileInfo);
    return PKG_SUCCESS;
}

int32_t Lz4FileEntry::DecodeHeader(const PkgBuffer &buffer, size_t headerOffset, size_t dataOffset,
    size_t &decodeLen)
{
    fileInfo_.fileInfo.identity = "lz4_";
    fileInfo_.fileInfo.identity.append(std::to_string(nodeId_));
    fileName_ = fileInfo_.fileInfo.identity;
    fileInfo_.fileInfo.digestMethod = PKG_DIGEST_TYPE_NONE;
    uint32_t magicNumber = ReadLE32(buffer.buffer);
    if (magicNumber == PkgAlgorithmLz4::LZ4S_MAGIC_NUMBER) {
        fileInfo_.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    } else if (magicNumber == PkgAlgorithmLz4::LZ4B_MAGIC_NUMBER) {
        fileInfo_.fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4_BLOCK;
    }
    fileInfo_.fileInfo.headerOffset = headerOffset;
    fileInfo_.fileInfo.dataOffset = dataOffset;
    fileInfo_.fileInfo.unpackedSize = pkgFile_->GetPkgStream()->GetFileLength();
    fileInfo_.fileInfo.packedSize = pkgFile_->GetPkgStream()->GetFileLength();
    return PKG_SUCCESS;
}

int32_t Lz4PkgFile::AddEntry(const PkgManager::FileInfoPtr file, const PkgStreamPtr inStream)
{
    if (file == nullptr || inStream == nullptr) {
        PKG_LOGE("Fail to check input param");
        return PKG_INVALID_PARAM;
    }
    if (!CheckState({ PKG_FILE_STATE_IDLE, PKG_FILE_STATE_WORKING }, PKG_FILE_STATE_CLOSE)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    PKG_LOGI("Add file %s to package", file->identity.c_str());

    Lz4FileEntry *entry = static_cast<Lz4FileEntry *>(AddPkgEntry(file->identity));
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
    PKG_LOGI("offset:%zu ", currentOffset_);
    pkgStream_->Flush(currentOffset_);
    return PKG_SUCCESS;
}

int32_t Lz4PkgFile::SavePackage(size_t &offset)
{
    AddSignData(pkgInfo_.digestMethod, currentOffset_, offset);
    return PKG_SUCCESS;
}

int32_t Lz4PkgFile::LoadPackage(std::vector<std::string> &fileNames, VerifyFunction verifier)
{
    UNUSED(verifier);
    if (!CheckState({ PKG_FILE_STATE_IDLE }, PKG_FILE_STATE_WORKING)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    PKG_LOGI("LoadPackage %s ", pkgStream_->GetFileName().c_str());

    size_t srcOffset = 0;
    size_t readLen = 0;
    PkgBuffer buffer(sizeof(PkgAlgorithmLz4::LZ4B_MAGIC_NUMBER));
    int32_t ret = pkgStream_->Read(buffer, srcOffset, buffer.length, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to read buffer");
        return ret;
    }
    if (readLen != sizeof(PkgAlgorithmLz4::LZ4B_MAGIC_NUMBER)) {
        PKG_LOGE("Fail to read buffer");
        return PKG_LZ4_FINISH;
    }

    srcOffset += sizeof(PkgAlgorithmLz4::LZ4B_MAGIC_NUMBER);
    uint32_t magicNumber = ReadLE32(buffer.buffer);
    PKG_LOGI("LoadPackage magic 0x%x", magicNumber);
    ret = PKG_INVALID_FILE;
    if (magicNumber == PkgAlgorithmLz4::LZ4S_MAGIC_NUMBER ||
        magicNumber == PkgAlgorithmLz4::LZ4B_MAGIC_NUMBER) {
        Lz4FileEntry *entry = new Lz4FileEntry(this, nodeId_++);
        if (entry == nullptr) {
            PKG_LOGE("Fail create upgrade node for %s", pkgStream_->GetFileName().c_str());
            return PKG_LZ4_FINISH;
        }
        ret = entry->DecodeHeader(buffer, 0, srcOffset, readLen);

        // 保存entry文件
        pkgEntryMapId_.insert(std::pair<uint32_t, PkgEntryPtr>(entry->GetNodeId(), entry));
        pkgEntryMapFileName_.insert(std::pair<std::string, PkgEntryPtr>(entry->GetFileName(), entry));
        fileNames.push_back(entry->GetFileName());
    }
    return ret;
}
} // namespace Hpackage
