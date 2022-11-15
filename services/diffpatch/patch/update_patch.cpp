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

#include "patch/update_patch.h"
#include <memory>
#include <vector>
#include "blocks_patch.h"
#include "diffpatch.h"
#include "image_patch.h"
#include "openssl/sha.h"
#include "securec.h"

using namespace Hpackage;
namespace UpdatePatch {
int32_t UpdateApplyPatch::ApplyImagePatch(const PatchParam &param, const std::vector<uint8_t> &bonusData,
    ImageProcessor writer, const std::string& expected)
{
    if (writer == nullptr) {
        PATCH_LOGE("ApplyImagePatch : processor is null");
        return -1;
    }
    std::unique_ptr<ImagePatchWriter> patchWriter = std::make_unique<ImagePatchWriter>(writer, expected, "");
    if (patchWriter == nullptr) {
        PATCH_LOGE("ApplyImagePatch : Failed to create patch writer");
        return -1;
    }
    int32_t ret = patchWriter->Init();
    if (ret != 0) {
        PATCH_LOGE("ApplyImagePatch : Failed to init patch writer");
        return -1;
    }
    ret = ApplyImagePatch(param, patchWriter.get(), bonusData);
    if (ret != 0) {
        PATCH_LOGE("ApplyImagePatch : Failed to apply image patch");
        return -1;
    }
    return patchWriter->Finish();
}

int32_t UpdateApplyPatch::ApplyImagePatch(const PatchParam &param,
    UpdatePatchWriterPtr writer, const std::vector<uint8_t> &bonusData)
{
    PATCH_CHECK(writer != nullptr, return -1, "check param fail ");
    PATCH_CHECK(param.patchSize >= (std::char_traits<char>::length(PKGDIFF_MAGIC) + sizeof(int32_t)),
        return -1, "patch too short to contain header ");
    PATCH_CHECK(memcmp(param.patch, PKGDIFF_MAGIC, std::char_traits<char>::length(PKGDIFF_MAGIC)) == 0,
        return -1, "corrupt patch file header (magic number) ");
    size_t offset = std::char_traits<char>::length(PKGDIFF_MAGIC);
    int32_t numChunks = ImagePatch::ReadLE<int32_t>(param.patch + offset);
    offset += sizeof(int32_t);

    std::vector<uint8_t> empty;
    for (int i = 0; i < numChunks; ++i) {
        // each chunk's header record starts with 4 bytes.
        PATCH_CHECK((offset + sizeof(int32_t)) <= param.patchSize, return -1, "Failed to read chunk record ");
        int32_t type = ImagePatch::ReadLE<int32_t>(param.patch + offset);
        PATCH_LOGI("ApplyImagePatch numChunks[%d] type %d offset %d", i, type, offset);
        offset += sizeof(int32_t);
        std::unique_ptr<ImagePatch> imagePatch = nullptr;
        switch (type) {
            case BLOCK_NORMAL:
                imagePatch = std::make_unique<NormalImagePatch>(writer);
                break;
            case BLOCK_RAW:
                imagePatch = std::make_unique<RowImagePatch>(writer);
                break;
            case BLOCK_DEFLATE:
                imagePatch = std::make_unique<ZipImagePatch>(writer, ((i == 1) ? bonusData : empty));
                break;
            case BLOCK_LZ4:
                imagePatch = std::make_unique<Lz4ImagePatch>(writer, ((i == 1) ? bonusData : empty));
                break;
            default:
                break;
        }
        PATCH_CHECK(imagePatch != nullptr, return -1, "Failed to  creareimg patch ");
        int32_t ret = imagePatch->ApplyImagePatch(param, offset);
        PATCH_CHECK(ret == 0, return -1, "Apply image patch fail ");
    }
    return 0;
}

int32_t UpdateApplyPatch::ApplyBlockPatch(const PatchBuffer &patchInfo,
    const BlockBuffer &oldInfo, std::vector<uint8_t> &newData)
{
    std::unique_ptr<BlocksBufferPatch> patch = std::make_unique<BlocksBufferPatch>(patchInfo, oldInfo, newData);
    if (patch == nullptr) {
        PATCH_LOGE("Failed to  creare patch ");
        return -1;
    }
    return patch->ApplyPatch();
}

int32_t UpdateApplyPatch::ApplyBlockPatch(const PatchBuffer &patchInfo,
    const BlockBuffer &oldInfo, UpdatePatchWriterPtr writer)
{
    PkgManager* pkgManager = Hpackage::PkgManager::GetPackageInstance();
    if (pkgManager == nullptr) {
        PATCH_LOGE("Failed to get pkg manager");
        return -1;
    }

    Hpackage::PkgManager::StreamPtr stream = nullptr;
    int32_t ret = pkgManager->CreatePkgStream(stream, "", {oldInfo.buffer, oldInfo.length});
    if (stream == nullptr || ret != PKG_SUCCESS) {
        PATCH_LOGE("Failed to create stream");
        pkgManager->ClosePkgStream(stream);
        return -1;
    }

    std::unique_ptr<BlocksStreamPatch> patch = std::make_unique<BlocksStreamPatch>(patchInfo, stream, writer);
    if (patch == nullptr) {
        PATCH_LOGE("Failed to  creare patch ");
        pkgManager->ClosePkgStream(stream);
        return -1;
    }
    ret = patch->ApplyPatch();
    pkgManager->ClosePkgStream(stream);
    return ret;
}

int32_t UpdateApplyPatch::ApplyBlockPatch(const PatchBuffer &patchInfo,
    const BlockBuffer &oldInfo, ImageProcessor writer, const std::string& expected)
{
    if (writer == nullptr) {
        PATCH_LOGE("ApplyBlockPatch : processor is null");
        return -1;
    }
    std::unique_ptr<ImagePatchWriter> patchWriter = std::make_unique<ImagePatchWriter>(writer, expected, "");
    if (patchWriter == nullptr) {
        PATCH_LOGE("ApplyBlockPatch : Failed to create patch writer");
        return -1;
    }
    int32_t ret = patchWriter->Init();
    if (ret != 0) {
        PATCH_LOGE("ApplyBlockPatch : Failed to init patch writer");
        return -1;
    }

    PkgManager* pkgManager = Hpackage::PkgManager::GetPackageInstance();
    if (pkgManager == nullptr) {
        PATCH_LOGE("ApplyBlockPatch ::Failed to get pkg manager");
        return -1;
    }

    Hpackage::PkgManager::StreamPtr stream = nullptr;
    ret = pkgManager->CreatePkgStream(stream, "", {oldInfo.buffer, oldInfo.length});
    if (stream == nullptr) {
        PATCH_LOGE("Failed to create stream");
        pkgManager->ClosePkgStream(stream);
        return -1;
    }

    std::unique_ptr<BlocksStreamPatch> patch = std::make_unique<BlocksStreamPatch>(patchInfo,
        stream, patchWriter.get());
    if (patch == nullptr) {
        PATCH_LOGE("Failed to  creare patch ");
        pkgManager->ClosePkgStream(stream);
        return -1;
    }
    ret = patch->ApplyPatch();
    pkgManager->ClosePkgStream(stream);
    if (ret != 0) {
        PATCH_LOGE("Failed to applay patch ");
        return -1;
    }
    return patchWriter->Finish();
}

int32_t UpdateApplyPatch::ApplyBlockPatch(const PatchBuffer &patchInfo,
    Hpackage::PkgManager::StreamPtr stream, UpdatePatchWriterPtr writer)
{
    std::unique_ptr<BlocksStreamPatch> patch = std::make_unique<BlocksStreamPatch>(patchInfo, stream, writer);
    if (patch == nullptr) {
        PATCH_LOGE("Failed to  creare patch ");
        return -1;
    }
    return patch->ApplyPatch();
}

int32_t UpdateApplyPatch::ApplyPatch(const std::string &patchName,
    const std::string &oldName, const std::string &newName)
{
    PATCH_DEBUG("UpdatePatch::ApplyPatch : %s ", patchName.c_str());
    std::vector<uint8_t> empty;
    MemMapInfo patchData {};
    MemMapInfo oldData {};
    int32_t ret = PatchMapFile(patchName, patchData);
    if (ret != 0) {
        PATCH_LOGE("ApplyPatch : Failed to read patch file");
        return -1;
    }
    ret = PatchMapFile(oldName, oldData);
    if (ret != 0) {
        PATCH_LOGE("ApplyPatch : Failed to read old file");
        return -1;
    }
    PATCH_LOGI("UpdatePatch::ApplyPatch patchData %zu oldData %zu ", patchData.length, oldData.length);
    std::unique_ptr<FilePatchWriter> writer = std::make_unique<FilePatchWriter>(newName);
    if (writer == nullptr) {
        PATCH_LOGE("Failed to create writer");
        return -1;
    }
    writer->Init();

    // check if image patch
    if (memcmp(patchData.memory, PKGDIFF_MAGIC, std::char_traits<char>::length(PKGDIFF_MAGIC)) == 0) {
        PatchParam param {};
        param.patch = patchData.memory;
        param.patchSize = patchData.length;
        param.oldBuff = oldData.memory;
        param.oldSize = oldData.length;
        ret = UpdatePatch::UpdateApplyPatch::ApplyImagePatch(param, writer.get(), empty);
        if (ret != 0) {
            PATCH_LOGE("Failed to apply image patch file");
            return -1;
        }
    } else if (memcmp(patchData.memory, BSDIFF_MAGIC, std::char_traits<char>::length(BSDIFF_MAGIC)) == 0) { // bsdiff
        PatchBuffer patchInfo = {patchData.memory, 0, patchData.length};
        BlockBuffer oldInfo = {oldData.memory, oldData.length};
        ret = ApplyBlockPatch(patchInfo, oldInfo, writer.get());
        if (ret != 0) {
            PATCH_LOGE("Failed to apply block patch");
            return -1;
        }
    } else {
        PATCH_LOGE("Invalid patch file");
        return -1;
    }
    writer->Finish();
    return 0;
}

int32_t ImagePatchWriter::Init()
{
    if (init_) {
        PATCH_LOGE("Has beed init");
        return -1;
    }
    if (writer_ == nullptr) {
        PATCH_LOGE("Writer is null");
        return -1;
    }
    SHA256_Init(&sha256Ctx_);
    init_ = true;
    return 0;
}

int32_t ImagePatchWriter::Write(size_t start, const BlockBuffer &buffer, size_t len)
{
    if (!init_) {
        PATCH_LOGE("Failed to check init");
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    SHA256_Update(&sha256Ctx_, buffer.buffer, len);
    return writer_(start, buffer, len);
}

int32_t ImagePatchWriter::Finish()
{
    if (!init_) {
        PATCH_LOGE("Failed to check init");
        return -1;
    }
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    SHA256_Final(digest.data(), &sha256Ctx_);
    BlockBuffer data = {  digest.data(), digest.size() };
    std::string hexDigest = ConvertSha256Hex(data);
    PATCH_LOGI("VerifySha256 SHA256 : %s expected SHA256 : %s", hexDigest.c_str(), expected_.c_str());
    init_ = false;
    return hexDigest.compare(expected_);
}

int32_t FilePatchWriter::Init()
{
    if (init_) {
        PATCH_LOGE("Has beed init");
        return -1;
    }
    if (!stream_.is_open()) {
        stream_.open(newFileName_, std::ios::out | std::ios::binary);
        if (stream_.fail()) {
            PATCH_LOGE("Failed to open %s", newFileName_.c_str());
            return -1;
        }
    }
    init_ = true;
    return 0;
}

int32_t FilePatchWriter::Write(size_t start, const BlockBuffer &buffer, size_t len)
{
    if (!init_) {
        PATCH_LOGE("Failed to check init");
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (!stream_.is_open()) {
        stream_.open(newFileName_, std::ios::out | std::ios::binary);
        if (stream_.fail()) {
            PATCH_LOGE("Failed to open %s", newFileName_.c_str());
            return -1;
        }
    }
    stream_.write(reinterpret_cast<const char*>(buffer.buffer), len);
    return 0;
}

int32_t FilePatchWriter::Finish()
{
    if (!init_) {
        PATCH_LOGE("Failed to check init");
        return -1;
    }
    PATCH_LOGI("FilePatchWriter %zu", static_cast<size_t>(stream_.tellp()));
    stream_.close();
    init_ = false;
    return 0;
}
} // namespace UpdatePatch