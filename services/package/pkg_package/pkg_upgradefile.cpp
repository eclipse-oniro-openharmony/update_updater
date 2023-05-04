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
#include "pkg_upgradefile.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include "dump.h"
#include "pkg_lz4file.h"
#include "pkg_manager.h"
#include "pkg_pkgfile.h"
#include "pkg_stream.h"
#include "pkg_utils.h"
#include "pkg_zipfile.h"
#include "securec.h"

#define TLV_CHECK_AND_RETURN(tlv, tlvType, len, fileLen)                                         \
    do {                                                                                         \
        if (!((tlv)->length < (fileLen) && (tlv)->length >= (len) && (tlv)->type == (tlvType) && \
              ((tlv)->length + sizeof(PkgTlv)) < (fileLen))) {                                   \
            PKG_LOGE("Invalid tlv type: %d length %u ", tlvType, ((tlv)->length));               \
            return PKG_INVALID_FILE;                                                             \
        }                                                                                        \
    } while (0)

using namespace std;

namespace Hpackage {
constexpr int32_t UPGRADE_FILE_HEADER_LEN = 3 * sizeof(PkgTlv) + sizeof(UpgradePkgHeader) + sizeof(UpgradePkgTime);
constexpr int32_t UPGRADE_RESERVE_LEN = 16;
constexpr int16_t TLV_TYPE_FOR_SHA256 = 0x0001;
constexpr int16_t TLV_TYPE_FOR_SHA384 = 0x0011;
constexpr size_t BUFFER_SIZE = 4 * 1024 * 1024;

int32_t UpgradeFileEntry::Init(const PkgManager::FileInfoPtr fileInfo, PkgStreamPtr inStream)
{
    int32_t ret = PkgEntry::Init(&fileInfo_.fileInfo, fileInfo, inStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to check input param");
        return PKG_INVALID_PARAM;
    }
    ComponentInfo *info = (ComponentInfo *)fileInfo;
    if (info != nullptr) {
        fileInfo_.version = info->version;
        fileInfo_.id = info->id;
        fileInfo_.resType = info->resType;
        fileInfo_.type = info->type;
        fileInfo_.compFlags = info->compFlags;
        fileInfo_.originalSize = info->originalSize;
        if (memcpy_s(fileInfo_.digest, sizeof(fileInfo_.digest), info->digest, sizeof(info->digest)) != EOK) {
            PKG_LOGE("UpgradeFileEntry memcpy failed");
            return PKG_NONE_MEMORY;
        }
    }
    return PKG_SUCCESS;
}

size_t UpgradePkgFile::GetUpgradeSignatureLen() const
{
    return SIGN_SHA256_LEN + SIGN_SHA384_LEN;
}

size_t UpgradePkgFile::GetDigestLen() const
{
    return DigestAlgorithm::GetDigestLen(pkgInfo_.pkgInfo.digestMethod);
}

int32_t UpgradePkgFile::GetEntryOffset(size_t &dataOffset, const PkgManager::FileInfoPtr file)
{
    if (!CheckState({PKG_FILE_STATE_IDLE, PKG_FILE_STATE_WORKING}, PKG_FILE_STATE_WORKING)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    if (pkgEntryMapId_.size() >= pkgInfo_.pkgInfo.entryCount) {
        PKG_LOGE("More entry for and for %s %zu", file->identity.c_str(), pkgEntryMapId_.size());
        return PKG_INVALID_PARAM;
    }
    PKG_LOGI("Add file %s to package", file->identity.c_str());
    size_t compDataLen = 0;
    for (auto &it : pkgEntryMapId_) {
        compDataLen += (*it.second).GetFileInfo()->packedSize;
    }
    dataOffset = UPGRADE_FILE_HEADER_LEN + pkgInfo_.pkgInfo.entryCount * sizeof(UpgradeCompInfo);
    dataOffset += UPGRADE_RESERVE_LEN + GetUpgradeSignatureLen();
    dataOffset += compDataLen;

    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::AddEntry(const PkgManager::FileInfoPtr file, const PkgStreamPtr inStream)
{
    if (file == nullptr || inStream == nullptr) {
        PKG_LOGE("Fail to check input param");
        return PKG_INVALID_PARAM;
    }
    size_t dataOffset = 0;

    int32_t ret = GetEntryOffset(dataOffset, file);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to GetEntryOffset");
        return ret;
    }

    UpgradeFileEntry *entry = static_cast<UpgradeFileEntry *>(AddPkgEntry(file->identity));
    if (entry == nullptr) {
        PKG_LOGE("Fail create pkg node for %s", file->identity.c_str());
        return PKG_NONE_MEMORY;
    }
    ret = entry->Init(file, inStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail init entry for %s", file->identity.c_str());
        return ret;
    }

    size_t encodeLen = 0;
    ret = entry->Pack(inStream, dataOffset, encodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Pack for %s", file->identity.c_str());
        return ret;
    }
    packedFileSize_ += encodeLen;

    size_t offset = UPGRADE_FILE_HEADER_LEN + (pkgEntryMapId_.size() - 1) * sizeof(UpgradeCompInfo);
    ret = entry->EncodeHeader(inStream, offset, encodeLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail encode header for %s", file->identity.c_str());
        return ret;
    }

    PKG_LOGI("Header offset:%zu data offset:%zu packedFileSize: %zu", offset, dataOffset, packedFileSize_);
    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::CheckPackageHeader(std::vector<uint8_t> &buffer, size_t &offset)
{
    if (!CheckState({PKG_FILE_STATE_WORKING}, PKG_FILE_STATE_CLOSE)) {
        PKG_LOGE("error state curr %d ", state_);
        return PKG_INVALID_STATE;
    }
    WriteLE16(buffer.data(), GetPackageTlvType()); // Type is 1 for package header in TLV format
    WriteLE16(buffer.data() + sizeof(uint16_t), sizeof(UpgradePkgHeader));
    offset += sizeof(PkgTlv);
    UpgradePkgHeader *header = reinterpret_cast<UpgradePkgHeader *>(buffer.data() + offset);
    if (header == nullptr) {
        PKG_LOGE("Fail to create header");
        return PKG_NONE_MEMORY;
    }
    header->pkgInfoLength = sizeof(PkgTlv) + sizeof(PkgTlv) + sizeof(PkgTlv) + sizeof(UpgradePkgHeader) +
        sizeof(UpgradePkgTime) + pkgInfo_.pkgInfo.entryCount * sizeof(UpgradeCompInfo) + UPGRADE_RESERVE_LEN;
    WriteLE32(reinterpret_cast<uint8_t *>(&header->updateFileVersion), pkgInfo_.updateFileVersion);
    int32_t ret = memcpy_s(header->softwareVersion, sizeof(header->softwareVersion), pkgInfo_.softwareVersion.data(),
        pkgInfo_.softwareVersion.size());
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s %s ret: %d", pkgStream_->GetFileName().c_str(), ret);
        return ret;
    }
    ret = memcpy_s(header->productUpdateId, sizeof(header->productUpdateId), pkgInfo_.productUpdateId.data(),
        pkgInfo_.productUpdateId.size());
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s %s ret: %d", pkgStream_->GetFileName().c_str(), ret);
        return ret;
    }
    offset += sizeof(UpgradePkgHeader);
    // 时间tlv
    WriteLE16(buffer.data() + offset, 0x02); // Type is 2 for time in TLV format
    WriteLE16(buffer.data() + offset + sizeof(uint16_t), sizeof(UpgradePkgTime));
    offset += sizeof(PkgTlv);
    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::WriteBuffer(std::vector<uint8_t> &buffer, size_t &offset, size_t &signOffset)
{
    offset += pkgInfo_.pkgInfo.entryCount * sizeof(UpgradeCompInfo);
    signOffset = offset + UPGRADE_RESERVE_LEN;

    buffer.assign(buffer.capacity(), 0);
    size_t nameLen = 0;
    int32_t ret = PkgFile::ConvertStringToBuffer(
        pkgInfo_.descriptPackageId, {buffer.data(), UPGRADE_RESERVE_LEN}, nameLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write descriptPackageId");
        return ret;
    }
    ret = pkgStream_->Write(buffer, GetUpgradeSignatureLen() + UPGRADE_RESERVE_LEN, offset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write sign for %s", pkgStream_->GetFileName().c_str());
        return ret;
    }
    PKG_LOGI("SavePackage success file length: %zu signOffset %zu", pkgStream_->GetFileLength(), signOffset);
    pkgStream_->Flush(offset);
    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::SavePackage(size_t &signOffset)
{
    PKG_LOGI("SavePackage %s", pkgStream_->GetFileName().c_str());

    // Allocate buffer size with max possible size
    size_t buffSize = GetUpgradeSignatureLen() + UPGRADE_RESERVE_LEN;
    buffSize = ((UPGRADE_FILE_HEADER_LEN > buffSize) ? UPGRADE_FILE_HEADER_LEN : buffSize);
    std::vector<uint8_t> buffer(buffSize);

    size_t offset = 0;
    // Package header information
    size_t ret = CheckPackageHeader(buffer, offset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to CheckPackageHeader");
        return PKG_NONE_MEMORY;
    }

    UpgradePkgTime *time = reinterpret_cast<UpgradePkgTime *>(buffer.data() + offset);
    ret = memcpy_s(time->date, sizeof(time->date), pkgInfo_.date.data(), pkgInfo_.date.size());
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s %s ret: %d", pkgStream_->GetFileName().c_str(), ret);
        return PKG_NONE_MEMORY;
    }
    ret = memcpy_s(time->time, sizeof(time->time), pkgInfo_.time.data(), pkgInfo_.time.size());
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s %s ret: %d", pkgStream_->GetFileName().c_str(), ret);
        return PKG_NONE_MEMORY;
    }
    offset += sizeof(UpgradePkgTime);
    // 组件的tlv
    WriteLE16(buffer.data() + offset, 0x05); // Type is 5 for component in TLV format
    WriteLE16(buffer.data() + offset + sizeof(uint16_t), pkgInfo_.pkgInfo.entryCount * sizeof(UpgradeCompInfo));
    offset += sizeof(PkgTlv);
    ret = pkgStream_->Write(buffer, UPGRADE_FILE_HEADER_LEN, 0);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write upgrade file header for %s ret: %d", pkgStream_->GetFileName().c_str(), ret);
        return ret;
    }
    // Clear buffer and save signature information
    ret = WriteBuffer(buffer, offset, signOffset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail WriteBuffer");
        return ret;
    }
    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::ReadPackageInfo(PkgBuffer &buffer, std::vector<uint8_t> &signData, size_t &parsedLen)
{
    size_t readBytes = 0;
    size_t ret = pkgStream_->Read(buffer, parsedLen, GetUpgradeSignatureLen() + UPGRADE_RESERVE_LEN, readBytes);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("read sign data fail");
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    PkgFile::ConvertBufferToString(pkgInfo_.descriptPackageId, {buffer.buffer, UPGRADE_RESERVE_LEN});
    if (pkgInfo_.pkgInfo.digestMethod == PKG_DIGEST_TYPE_SHA384) {
        signData.resize(SIGN_SHA384_LEN);
        ret = memcpy_s(signData.data(), signData.size(),
            buffer.buffer + UPGRADE_RESERVE_LEN + SIGN_SHA256_LEN, SIGN_SHA384_LEN);
    } else {
        signData.resize(SIGN_SHA256_LEN);
        ret = memcpy_s(signData.data(), signData.size(), buffer.buffer + UPGRADE_RESERVE_LEN, SIGN_SHA256_LEN);
    }
    if (ret != EOK) {
        PKG_LOGE("memcpy sign data fail");
        UPDATER_LAST_WORD(PKG_NONE_MEMORY);
        return PKG_NONE_MEMORY;
    }

    ret = memset_s(buffer.buffer + UPGRADE_RESERVE_LEN, buffer.length, 0, GetUpgradeSignatureLen());
    if (ret != EOK) {
        PKG_LOGE("memset buff fail");
        UPDATER_LAST_WORD(PKG_NONE_MEMORY);
        return PKG_NONE_MEMORY;
    }
    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::LoadPackage(std::vector<std::string> &fileNames, VerifyFunction verifier)
{
    if (verifier == nullptr) {
        PKG_LOGE("Check verifier nullptr");
        UPDATER_LAST_WORD(PKG_INVALID_SIGNATURE);
        return PKG_INVALID_SIGNATURE;
    }
    if (!CheckState({PKG_FILE_STATE_IDLE}, PKG_FILE_STATE_WORKING)) {
        PKG_LOGE("error state curr %d ", state_);
        UPDATER_LAST_WORD(PKG_INVALID_STATE);
        return PKG_INVALID_STATE;
    }
    PKG_LOGI("LoadPackage %s ", pkgStream_->GetFileName().c_str());
    size_t fileLen = pkgStream_->GetFileLength();
    // Allocate buffer with smallest package size
    size_t buffSize = UPGRADE_FILE_HEADER_LEN + sizeof(UpgradeCompInfo) +
        GetUpgradeSignatureLen() + UPGRADE_RESERVE_LEN;
    if (fileLen < buffSize) {
        PKG_LOGE("Invalid file %s fileLen:%zu ", pkgStream_->GetFileName().c_str(), fileLen);
        UPDATER_LAST_WORD(PKG_INVALID_STATE);
        return PKG_INVALID_FILE;
    }

    std::vector<uint8_t> signData;
    DigestAlgorithm::DigestAlgorithmPtr algorithm = nullptr;
    // Parse header
    PkgBuffer buffer(buffSize);
    size_t parsedLen = 0;
    int32_t ret = ReadUpgradePkgHeader(buffer, parsedLen, algorithm);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Decode header fail %d", ret);
        UPDATER_LAST_WORD(PKG_INVALID_STATE);
        return ret;
    }

    ret = ReadComponents(buffer, parsedLen, algorithm, fileNames);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Decode components fail %d", ret);
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    if (parsedLen + UPGRADE_RESERVE_LEN + GetUpgradeSignatureLen() >= fileLen) {
        PKG_LOGE("Decode components fail %d", ret);
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    // Read signature information
    ret = ReadPackageInfo(buffer, signData, parsedLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ReadPackageInfo fail %d", ret);
        return ret;
    }
    algorithm->Update(buffer, UPGRADE_RESERVE_LEN + GetUpgradeSignatureLen());
    parsedLen += UPGRADE_RESERVE_LEN + GetUpgradeSignatureLen();
    pkgInfo_.pkgInfo.updateFileHeadLen = parsedLen;

    // Calculate digest and verify
    return Verify(parsedLen, algorithm, verifier, signData);
}

int32_t UpgradePkgFile::Verify(size_t start, DigestAlgorithm::DigestAlgorithmPtr algorithm,
    VerifyFunction verifier, const std::vector<uint8_t> &signData)
{
    Updater::UPDATER_INIT_RECORD;
    int ret = 0;
    size_t buffSize = BUFFER_SIZE;
    size_t offset = start;
    size_t readBytes = 0;
    PkgBuffer buffer(buffSize);

    while (offset + readBytes < pkgStream_->GetFileLength()) {
        offset += readBytes;
        readBytes = 0;
        size_t remainBytes = pkgStream_->GetFileLength() - offset;
        remainBytes = ((remainBytes > buffSize) ? buffSize : remainBytes);
        ret = pkgStream_->Read(buffer, offset, remainBytes, readBytes);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail to read data ");
            UPDATER_LAST_WORD(ret);
            return ret;
        }
        algorithm->Update(buffer, readBytes);
        pkgManager_->PostDecodeProgress(POST_TYPE_VERIFY_PKG, remainBytes, nullptr);
    }

    PkgBuffer digest(GetDigestLen());
    algorithm->Final(digest);
    ret = verifier(&pkgInfo_.pkgInfo, digest.data, signData);
    if (ret != 0) {
        PKG_LOGE("Fail to verifier signature");
        UPDATER_LAST_WORD(PKG_INVALID_SIGNATURE);
        return PKG_INVALID_SIGNATURE;
    }
    return 0;
}

int32_t UpgradePkgFile::SaveEntry(const PkgBuffer &buffer, size_t &parsedLen, UpgradeParam &info,
    DigestAlgorithm::DigestAlgorithmPtr algorithm, std::vector<std::string> &fileNames)
{
    UpgradeFileEntry *entry = new (std::nothrow) UpgradeFileEntry(this, nodeId_++);
    if (entry == nullptr) {
        PKG_LOGE("Fail create upgrade node for %s", pkgStream_->GetFileName().c_str());
        return PKG_NONE_MEMORY;
    }

    // Extract header information from file
    size_t decodeLen = 0;
    PkgBuffer headerBuff(buffer.buffer + info.currLen, info.readLen - info.currLen);
    size_t ret = entry->DecodeHeader(headerBuff, parsedLen + info.srcOffset, info.dataOffset, decodeLen);
    if (ret != PKG_SUCCESS) {
        delete entry;
        PKG_LOGE("Fail to decode header");
        return ret;
    }
    // Save entry
    pkgEntryMapId_.insert(pair<uint32_t, PkgEntryPtr>(entry->GetNodeId(), entry));
    pkgEntryMapFileName_.insert(std::pair<std::string, PkgEntryPtr>(entry->GetFileName(), entry));
    fileNames.push_back(entry->GetFileName());

    PkgBuffer signBuffer(buffer.buffer + info.currLen, decodeLen);
    algorithm->Update(signBuffer, decodeLen); // Generate digest for components

    info.currLen += decodeLen;
    info.srcOffset += decodeLen;

    if (entry->GetFileInfo() == nullptr) {
        delete entry;
        PKG_LOGE("Failed to get file info");
        return PKG_INVALID_FILE;
    }
    
    info.dataOffset += entry->GetFileInfo()->packedSize;
    pkgInfo_.pkgInfo.entryCount++;
    PKG_LOGI("Component packedSize %zu unpackedSize %zu %s", entry->GetFileInfo()->packedSize,
        entry->GetFileInfo()->unpackedSize, entry->GetFileInfo()->identity.c_str());
    return PKG_SUCCESS;
}

int32_t UpgradePkgFile::ReadComponents(const PkgBuffer &buffer, size_t &parsedLen,
    DigestAlgorithm::DigestAlgorithmPtr algorithm, std::vector<std::string> &fileNames)
{
    Updater::UPDATER_INIT_RECORD;
    UpgradeParam info;
    size_t fileLen = pkgStream_->GetFileLength();
    info.readLen = 0;
    int32_t ret = pkgStream_->Read(buffer, parsedLen, buffer.length, info.readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Read component fail");
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    PkgTlv tlv;
    tlv.type = ReadLE16(buffer.buffer);
    tlv.length = ReadLE16(buffer.buffer + sizeof(uint16_t));
    TLV_CHECK_AND_RETURN(&tlv, 5, sizeof(UpgradeCompInfo), fileLen); // component type is 5
    algorithm->Update(buffer, sizeof(PkgTlv)); // tlv generate digest

    parsedLen += sizeof(PkgTlv);

    info.dataOffset = parsedLen + tlv.length + GetUpgradeSignatureLen() + UPGRADE_RESERVE_LEN;
    info.srcOffset = 0;
    info.currLen = sizeof(PkgTlv);
    while (info.srcOffset < tlv.length) {
        if (info.currLen + sizeof(UpgradeCompInfo) > info.readLen) {
            info.readLen = 0;
            ret = pkgStream_->Read(buffer, parsedLen + info.srcOffset, buffer.length, info.readLen);
            if (ret != PKG_SUCCESS) {
                PKG_LOGE("Fail to read data");
                UPDATER_LAST_WORD(ret);
                return ret;
            }
            info.currLen = 0;
        }
        size_t ret = SaveEntry(buffer, parsedLen, info, algorithm, fileNames);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("SaveEntry");
            UPDATER_LAST_WORD(ret);
            return ret;
        }
    }
    parsedLen += info.srcOffset;
    return PKG_SUCCESS;
}

void UpgradePkgFile::ParsePkgHeaderToTlv(const PkgBuffer &buffer, size_t &currLen, PkgTlv &tlv)
{
    pkgInfo_.pkgInfo.pkgType = PkgFile::PKG_TYPE_UPGRADE;
    pkgInfo_.pkgInfo.signMethod = PKG_SIGN_METHOD_RSA;
    pkgInfo_.pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA256;

    tlv.type = ReadLE16(buffer.buffer);
    tlv.length = ReadLE16(buffer.buffer + sizeof(uint16_t));
    if (tlv.type == TLV_TYPE_FOR_SHA384) {
        pkgInfo_.pkgInfo.digestMethod = PKG_DIGEST_TYPE_SHA384;
    }

    // Header information
    currLen = sizeof(PkgTlv);
    UpgradePkgHeader *header = reinterpret_cast<UpgradePkgHeader *>(buffer.buffer + currLen);
    pkgInfo_.updateFileVersion = ReadLE32(buffer.buffer + currLen + offsetof(UpgradePkgHeader, updateFileVersion));
    PkgFile::ConvertBufferToString(pkgInfo_.softwareVersion, {header->softwareVersion,
        sizeof(header->softwareVersion)});
    PkgFile::ConvertBufferToString(pkgInfo_.productUpdateId, {header->productUpdateId,
        sizeof(header->productUpdateId)});
}

int32_t UpgradePkgFile::ReadUpgradePkgHeader(const PkgBuffer &buffer, size_t &realLen,
    DigestAlgorithm::DigestAlgorithmPtr &algorithm)
{
    Updater::UPDATER_INIT_RECORD;
    size_t fileLen = pkgStream_->GetFileLength();
    size_t readLen = 0;
    size_t currLen = 0;
    PkgTlv tlv;
    int32_t ret = pkgStream_->Read(buffer, 0, buffer.length, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to read header");
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    ParsePkgHeaderToTlv(buffer, currLen, tlv);
    algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(pkgInfo_.pkgInfo.digestMethod);
    if (algorithm == nullptr) {
        PKG_LOGE("Invalid file %s", pkgStream_->GetFileName().c_str());
        UPDATER_LAST_WORD(PKG_NOT_EXIST_ALGORITHM);
        return PKG_NOT_EXIST_ALGORITHM;
    }
    algorithm->Init();

    if (currLen + tlv.length >= readLen) { // Extra TLV information, read it.
        realLen = currLen + tlv.length;
        algorithm->Update(buffer, realLen);
        ret = pkgStream_->Read(buffer, realLen, buffer.length, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail to read header");
            UPDATER_LAST_WORD(ret);
            return ret;
        }
        currLen = 0;
    } else {
        currLen += tlv.length;
    }

    // Time information
    tlv.type = ReadLE16(buffer.buffer + currLen);
    tlv.length = ReadLE16(buffer.buffer + currLen + sizeof(uint16_t));
    TLV_CHECK_AND_RETURN(&tlv, sizeof(uint16_t), sizeof(UpgradePkgTime), fileLen);
    currLen += sizeof(PkgTlv);
    UpgradePkgTime *time = reinterpret_cast<UpgradePkgTime *>(buffer.buffer + currLen);
    PkgFile::ConvertBufferToString(pkgInfo_.date, {time->date, sizeof(time->date)});
    PkgFile::ConvertBufferToString(pkgInfo_.time, {time->time, sizeof(time->time)});
    currLen += tlv.length;
    realLen += currLen;

    // Parser header to get compressional algorithm
    algorithm->Update(buffer, currLen); // Generate digest
    return PKG_SUCCESS;
}

int32_t UpgradeFileEntry::GetUpGradeCompInfo(UpgradeCompInfo &comp)
{
    if (memset_s(&comp, sizeof(comp), 0, sizeof(comp)) != EOK) {
        PKG_LOGE("UpgradeFileEntry memset_s failed");
        return PKG_NONE_MEMORY;
    }
    size_t len = 0;
    int32_t ret = PkgFile::ConvertStringToBuffer(
        fileInfo_.fileInfo.identity, {comp.address, sizeof(comp.address)}, len);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("outStream or inStream null for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }

    ret = PkgFile::ConvertStringToBuffer(fileInfo_.version, {comp.version, sizeof(comp.version)}, len);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("outStream or inStream null for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }
    ret = memcpy_s(comp.digest, sizeof(comp.digest), fileInfo_.digest, sizeof(fileInfo_.digest));
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s ret: %d", ret);
        return ret;
    }
    return PKG_SUCCESS;
}

int32_t UpgradeFileEntry::EncodeHeader(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    PkgStreamPtr outStream = pkgFile_->GetPkgStream();
    if (outStream == nullptr || inStream == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }

    UpgradeCompInfo comp;
    int ret = GetUpGradeCompInfo(comp);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("GetUpGradeCompInfo failed");
        return ret;
    }

    WriteLE32(reinterpret_cast<uint8_t *>(&comp.size), fileInfo_.fileInfo.unpackedSize);
    WriteLE16(reinterpret_cast<uint8_t *>(&comp.id), fileInfo_.id);
    WriteLE32(reinterpret_cast<uint8_t *>(&comp.originalSize), fileInfo_.originalSize);
    comp.resType = fileInfo_.resType;
    comp.flags = fileInfo_.compFlags;
    comp.type = fileInfo_.type;

    headerOffset_ = startOffset;
    PkgBuffer buffer(reinterpret_cast<uint8_t *>(&comp), sizeof(comp));
    ret = outStream->Write(buffer, sizeof(comp), startOffset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write header for %s", fileName_.c_str());
        return ret;
    }
    encodeLen = sizeof(UpgradeCompInfo);

    PKG_LOGI("EncodeHeader startOffset: %zu %zu packedSize:%zu %zu ", headerOffset_, dataOffset_,
        fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize);
    return PKG_SUCCESS;
}

int32_t UpgradeFileEntry::Pack(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    PkgStreamPtr outStream = pkgFile_->GetPkgStream();
    if (algorithm == nullptr || outStream == nullptr || inStream == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }

    PkgAlgorithmContext context = {
        {0, startOffset},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        0, fileInfo_.fileInfo.digestMethod
    };
    if (memcpy_s(context.digest, sizeof(context.digest), fileInfo_.digest, sizeof(fileInfo_.digest)) != EOK) {
        PKG_LOGE("UpgradeFileEntry pack memcpy failed");
        return PKG_NONE_MEMORY;
    }
    int32_t ret = algorithm->Pack(inStream, outStream, context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Compress for %s", fileName_.c_str());
        return ret;
    }

    // Fill digest and compressed size of file
    if (memcpy_s(fileInfo_.digest, sizeof(fileInfo_.digest), context.digest, sizeof(context.digest)) != EOK) {
        PKG_LOGE("UpgradeFileEntry pack memcpy failed");
        return PKG_NONE_MEMORY;
    }
    fileInfo_.fileInfo.packedSize = context.packedSize;
    dataOffset_ = startOffset;
    encodeLen = fileInfo_.fileInfo.packedSize;
    PKG_LOGI("Pack start:%zu unpackSize:%zu packSize:%zu", startOffset, fileInfo_.fileInfo.unpackedSize,
        fileInfo_.fileInfo.packedSize);
    return PKG_SUCCESS;
}

int32_t UpgradeFileEntry::DecodeHeader(const PkgBuffer &buffer, size_t headerOffset, size_t dataOffset,
    size_t &decodeLen)
{
    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    if (inStream == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }
    if (buffer.length < sizeof(UpgradeCompInfo)) {
        PKG_LOGE("Fail to check buffer %zu", buffer.length);
        return PKG_INVALID_PKG_FORMAT;
    }

    UpgradeCompInfo *info = reinterpret_cast<UpgradeCompInfo *>(buffer.buffer);
    fileInfo_.fileInfo.packedSize = ReadLE32(buffer.buffer + offsetof(UpgradeCompInfo, size));
    fileInfo_.fileInfo.unpackedSize = fileInfo_.fileInfo.packedSize;
    fileInfo_.originalSize = ReadLE32(buffer.buffer + offsetof(UpgradeCompInfo, originalSize));
    fileInfo_.fileInfo.packMethod = PKG_COMPRESS_METHOD_NONE;
    fileInfo_.fileInfo.digestMethod = PKG_DIGEST_TYPE_NONE;
    int32_t ret = memcpy_s(fileInfo_.digest, sizeof(fileInfo_.digest), info->digest, sizeof(info->digest));
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s ret: %d", ret);
        return ret;
    }
    PkgFile::ConvertBufferToString(fileInfo_.fileInfo.identity, {info->address, sizeof(info->address)});
    PkgFile::ConvertBufferToString(fileInfo_.version, {info->version, sizeof(info->version)});
    fileName_ = fileInfo_.fileInfo.identity;
    fileInfo_.id = ReadLE16(buffer.buffer + offsetof(UpgradeCompInfo, id));
    fileInfo_.resType = info->resType;
    fileInfo_.compFlags = info->flags;
    fileInfo_.type = info->type;

    headerOffset_ = headerOffset;
    dataOffset_ = dataOffset;
    decodeLen = sizeof(UpgradeCompInfo);

    PKG_LOGI("Component offset: %zu %zu packedSize:%zu %zu %s", headerOffset, dataOffset,
        fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize, fileName_.c_str());
    return PKG_SUCCESS;
}

int32_t UpgradeFileEntry::Unpack(PkgStreamPtr outStream)
{
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(&fileInfo_.fileInfo);
    if (algorithm == nullptr) {
        PKG_LOGE("can not algorithm for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }

    PkgStreamPtr inStream = pkgFile_->GetPkgStream();
    if (outStream == nullptr || inStream == nullptr) {
        PKG_LOGE("outStream or inStream null for %s", fileName_.c_str());
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithmContext context = {
        {this->dataOffset_, 0},
        {fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize},
        0, fileInfo_.fileInfo.digestMethod
    };
    int32_t ret = memcpy_s(context.digest, sizeof(context.digest), fileInfo_.digest, sizeof(fileInfo_.digest));
    if (ret != EOK) {
        PKG_LOGE("Fail to memcpy_s ret: %d", ret);
        return ret;
    }
    ret = algorithm->Unpack(inStream, outStream, context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Decompress for %s", fileName_.c_str());
        return ret;
    }
    PKG_LOGI("Unpack %s data offset:%zu packedSize:%zu unpackedSize:%zu", fileName_.c_str(), dataOffset_,
        fileInfo_.fileInfo.packedSize, fileInfo_.fileInfo.unpackedSize);
    outStream->Flush(fileInfo_.fileInfo.unpackedSize);
    return PKG_SUCCESS;
}

int16_t UpgradePkgFile::GetPackageTlvType()
{
    static int16_t packageTlvType[PKG_DIGEST_TYPE_MAX] = {
        TLV_TYPE_FOR_SHA256, TLV_TYPE_FOR_SHA256, TLV_TYPE_FOR_SHA256, TLV_TYPE_FOR_SHA384
    };
    if (pkgInfo_.pkgInfo.digestMethod < PKG_DIGEST_TYPE_MAX) {
        return packageTlvType[pkgInfo_.pkgInfo.digestMethod];
    }
    return TLV_TYPE_FOR_SHA256;
}
} // namespace Hpackage
