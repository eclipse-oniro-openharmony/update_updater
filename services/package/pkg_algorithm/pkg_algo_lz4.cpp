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
#include "pkg_algo_lz4.h"
#include "lz4.h"
#include "lz4frame.h"
#include "lz4hc.h"
#include "pkg_stream.h"
#include "pkg_utils.h"
#include "securec.h"

namespace Hpackage {
PkgAlgorithmLz4::PkgAlgorithmLz4(const Lz4FileInfo &config) : PkgAlgorithm(),
    compressionLevel_(config.compressionLevel),
    blockIndependence_(config.blockIndependence),
    contentChecksumFlag_(config.contentChecksumFlag),
    blockSizeID_(config.blockSizeID),
    autoFlush_(config.autoFlush)
{
    // blockIndependence_ 0 LZ4F_blockLinked
    // contentChecksumFlag_ 0 disable
    // blockSizeID_ LZ4F_default=0
    if (compressionLevel_ < 1) {
        compressionLevel_ = 2; // 2 : set compressionLevel_ 2
    }
    if (compressionLevel_ >= LZ4HC_CLEVEL_MAX) {
        compressionLevel_ = LZ4HC_CLEVEL_MAX;
    }
}

int32_t PkgAlgorithmLz4::AdpLz4Compress(const uint8_t *src, uint8_t *dest,
    uint32_t srcSize, uint32_t dstCapacity) const
{
    if (compressionLevel_ < LZ4HC_CLEVEL_MIN) { // hc 最小是3
        return LZ4_compress_default(reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dest),
            static_cast<int32_t>(srcSize), static_cast<int32_t>(dstCapacity));
    }
    return LZ4_compress_HC(reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dest), srcSize, dstCapacity,
        compressionLevel_);
}

int32_t PkgAlgorithmLz4::AdpLz4Decompress(const uint8_t *src, uint8_t *dest, uint32_t srcSize,
    uint32_t dstCapacity) const
{
    return LZ4_decompress_safe(reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dest), srcSize,
        dstCapacity);
}

int32_t PkgAlgorithmBlockLz4::PackCalculate(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context, PkgBuffer &inBuffer, PkgBuffer &outBuffer)
{
    size_t srcOffset = context.srcOffset;
    size_t destOffset = context.destOffset;
    size_t remainSize = context.unpackedSize;
    size_t readLen = 0;
    /* 写包头 */
    WriteLE32(outBuffer.buffer, LZ4B_MAGIC_NUMBER);
    int32_t ret = outStream->Write(outBuffer, sizeof(LZ4B_MAGIC_NUMBER), destOffset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write data ");
        return ret;
    }
    destOffset += sizeof(LZ4B_MAGIC_NUMBER);

    while (remainSize > 0) {
        ret = ReadData(inStream, srcOffset, inBuffer, remainSize, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail read data ");
            break;
        }

        // Compress Block, reserve 4 bytes to store block size
        int32_t outSize = AdpLz4Compress(inBuffer.buffer,
            outBuffer.buffer + LZ4B_REVERSED_LEN, readLen, outBuffer.length - LZ4B_REVERSED_LEN);
        if (outSize <= 0) {
            PKG_LOGE("Fail to compress data outSize %d ", outSize);
            break;
        }

        // Write block to buffer.
        // Buffer format: <block size> + <block contents>
        WriteLE32(outBuffer.buffer, outSize);
        ret = outStream->Write(outBuffer, outSize + LZ4B_REVERSED_LEN, destOffset);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail write data ");
            break;
        }

        srcOffset += readLen;
        destOffset += static_cast<size_t>(outSize) + LZ4B_REVERSED_LEN;
    }
    if (srcOffset - context.srcOffset != context.unpackedSize) {
        PKG_LOGE("original size error %zu %zu", srcOffset, context.unpackedSize);
        return ret;
    }
    context.packedSize = destOffset - context.destOffset;
    return PKG_SUCCESS;
}

int32_t PkgAlgorithmBlockLz4::Pack(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context)
{
    if (inStream == nullptr || outStream == nullptr) {
        PKG_LOGE("Param context null!");
        return PKG_INVALID_PARAM;
    }
    size_t blockSize = static_cast<size_t>(GetBlockSizeFromBlockId(blockSizeID_));
    blockSize = (blockSize > LZ4B_BLOCK_SIZE) ? LZ4B_BLOCK_SIZE : blockSize;
    PkgBuffer inBuffer = {blockSize};
    PkgBuffer outBuffer = {LZ4_compressBound(blockSize)};
    if (inBuffer.buffer == nullptr || outBuffer.buffer == nullptr) {
        PKG_LOGE("Fail to alloc buffer ");
        return PKG_NONE_MEMORY;
    }

    PKG_LOGI("frameInfo blockSizeID %d compressionLevel_: %d blockIndependence_:%d contentChecksumFlag_:%d %zu",
        static_cast<int32_t>(blockSizeID_), static_cast<int32_t>(compressionLevel_),
        static_cast<int32_t>(blockIndependence_), static_cast<int32_t>(contentChecksumFlag_), blockSize);

    return PackCalculate(inStream, outStream, context, inBuffer, outBuffer);
}

int32_t PkgAlgorithmBlockLz4::UnpackCalculate(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context, int &inBuffSize)
{
    PkgBuffer inBuffer(static_cast<size_t>(inBuffSize));
    PkgBuffer outBuffer(LZ4B_BLOCK_SIZE);
    if (inBuffer.buffer == nullptr || outBuffer.buffer == nullptr) {
        PKG_LOGE("Fail to alloc buffer ");
        return PKG_NONE_MEMORY;
    }
    PkgAlgorithmContext unpackText = context;
    size_t readLen = 0;

    /* Main Loop */
    while (1) {
        /* Block Size */
        inBuffer.length = sizeof(uint32_t);
        int32_t ret = ReadData(inStream, unpackText.srcOffset, inBuffer, unpackText.packedSize, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail read data ");
            break;
        }
        if (readLen == 0) {
            break;
        }
        uint32_t blockSize = ReadLE32(inBuffer.buffer);
        if (blockSize > static_cast<uint32_t>(inBuffSize)) {
            PKG_LOGE("Fail to get block size %u  %d", blockSize, inBuffSize);
            break;
        }
        unpackText.srcOffset += sizeof(uint32_t);

        /* Read Block */
        inBuffer.length = blockSize;
        ret = ReadData(inStream, unpackText.srcOffset, inBuffer, unpackText.packedSize, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail Read Block ");
            break;
        }

        /* Decode Block */
        int32_t decodeSize = AdpLz4Decompress(inBuffer.buffer,
            outBuffer.buffer, readLen, LZ4B_BLOCK_SIZE);
        if (decodeSize <= 0) {
            PKG_LOGE("Fail to decompress");
            break;
        }

        /* Write Block */
        ret = outStream->Write(outBuffer, decodeSize, unpackText.destOffset);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail Write Block ");
            break;
        }
        unpackText.destOffset += static_cast<size_t>(decodeSize);
        unpackText.srcOffset += readLen;
    }
    context.packedSize = unpackText.srcOffset - context.srcOffset;
    context.unpackedSize = unpackText.destOffset - context.destOffset;
    return PKG_SUCCESS;
}

int32_t PkgAlgorithmBlockLz4::Unpack(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context)
{
    if (inStream == nullptr || outStream == nullptr) {
        PKG_LOGE("Param context null!");
        return PKG_INVALID_PARAM;
    }
    int inBuffSize = LZ4_compressBound(LZ4B_BLOCK_SIZE);
    if (inBuffSize <= 0) {
        PKG_LOGE("BufferSize must > 0");
        return PKG_NONE_MEMORY;
    }
    return UnpackCalculate(inStream, outStream, context, inBuffSize);
}

int32_t PkgAlgorithmLz4::GetPackParam(LZ4F_compressionContext_t &ctx, LZ4F_preferences_t &preferences,
    size_t &inBuffSize, size_t &outBuffSize) const
{
    LZ4F_errorCode_t errorCode = 0;
    errorCode = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) {
        PKG_LOGE("Fail to create compress context %s", LZ4F_getErrorName(errorCode));
        return PKG_NONE_MEMORY;
    }
    size_t blockSize = static_cast<size_t>(GetBlockSizeFromBlockId(blockSizeID_));
    if (memset_s(&preferences, sizeof(preferences), 0, sizeof(preferences)) != EOK) {
        PKG_LOGE("Memset failed");
        return PKG_NONE_MEMORY;
    }
    preferences.autoFlush = autoFlush_;
    preferences.compressionLevel = compressionLevel_;
    preferences.frameInfo.blockMode = ((blockIndependence_ == 0) ? LZ4F_blockLinked : LZ4F_blockIndependent);
    preferences.frameInfo.blockSizeID = (LZ4F_blockSizeID_t)blockSizeID_;
    preferences.frameInfo.contentChecksumFlag =
        ((contentChecksumFlag_ == 0) ? LZ4F_noContentChecksum : LZ4F_contentChecksumEnabled);

    outBuffSize = LZ4F_compressBound(blockSize, &preferences);
    if (outBuffSize <= 0) {
        PKG_LOGE("BufferSize must > 0");
        return PKG_NONE_MEMORY;
    }
    inBuffSize = blockSize;

    PKG_LOGI("frameInfo blockSizeID %d compressionLevel_: %d blockIndependence_:%d contentChecksumFlag_:%d",
        static_cast<int32_t>(blockSizeID_), static_cast<int32_t>(compressionLevel_),
        static_cast<int32_t>(blockIndependence_), static_cast<int32_t>(contentChecksumFlag_));
    PKG_LOGI("blockSize %zu %zu %zu", blockSize, GetBlockSizeFromBlockId(blockSizeID_), outBuffSize);
    return PKG_SUCCESS;
}

int32_t PkgAlgorithmLz4::PackCalculate(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgBufferMessage &msg, size_t &dataLen, LZ4F_compressionContext_t &ctx)
{
    /* 写包头 */
    int32_t ret = outStream->Write(msg.outBuffer, dataLen, msg.context.destOffset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail write data ");
        return ret;
    }

    msg.context.destOffset += dataLen;
    while (msg.context.unpackedSize > 0) {
        int32_t ret = ReadData(inStream, msg.context.srcOffset, msg.inBuffer, msg.context.unpackedSize, dataLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail read data ");
            break;
        }
        size_t outSize = LZ4F_compressUpdate(ctx,
            msg.outBuffer.buffer, msg.outBuffer.length, msg.inBuffer.buffer, dataLen, nullptr);
        if (LZ4F_isError(outSize)) {
            ret = PKG_NONE_MEMORY;
            PKG_LOGE("Fail to compress update %s", LZ4F_getErrorName(outSize));
            break;
        }
        ret = outStream->Write(msg.outBuffer, outSize, msg.context.destOffset);
        if (ret != PKG_SUCCESS) {
            ret = PKG_NONE_MEMORY;
            PKG_LOGE("Fail write data ");
            break;
        }

        msg.context.srcOffset += dataLen;
        msg.context.destOffset += outSize;
    }

    if (ret == PKG_SUCCESS) {
        size_t headerSize = LZ4F_compressEnd(ctx, msg.outBuffer.buffer, msg.outBuffer.length, nullptr);
        if (LZ4F_isError(headerSize)) {
            PKG_LOGE("Fail to compress update end %s", LZ4F_getErrorName(headerSize));
            return PKG_NONE_MEMORY;
        }
        ret = outStream->Write(msg.outBuffer, headerSize, msg.context.destOffset);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Fail write data ");
            return ret;
        }
        msg.context.destOffset += headerSize;
    }
    return ret;
}

/* 打包数据时，会自动生成magic字 */
int32_t PkgAlgorithmLz4::Pack(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context)
{
    if (inStream == nullptr || outStream == nullptr) {
        PKG_LOGE("Param context null!");
        return PKG_INVALID_PARAM;
    }
    LZ4F_compressionContext_t ctx;
    LZ4F_preferences_t preferences;
    size_t inLength = 0;
    size_t outLength = 0;
    if (GetPackParam(ctx, preferences, inLength, outLength) != PKG_SUCCESS) {
        PKG_LOGE("Fail to get param for pack");
        return PKG_NONE_MEMORY;
    }

    PkgBuffer inBuffer(inLength);
    PkgBuffer outBuffer(outLength);
    if (inBuffer.buffer == nullptr || outBuffer.buffer == nullptr) {
        (void)LZ4F_freeCompressionContext(ctx);
        PKG_LOGE("Fail to alloc buffer ");
        return PKG_NONE_MEMORY;
    }
    PkgAlgorithmContext packText = context;
    struct PkgBufferMessage msg { packText, inBuffer, outBuffer, inLength, outLength};
    size_t dataLen = LZ4F_compressBegin(ctx, msg.outBuffer.buffer, msg.outBuffer.length, &preferences);
    if (LZ4F_isError(dataLen)) {
        (void)LZ4F_freeCompressionContext(ctx);
        PKG_LOGE("Fail to generate header %s", LZ4F_getErrorName(dataLen));
        return PKG_NONE_MEMORY;
    }
    int32_t ret = PackCalculate(inStream, outStream, msg, dataLen, ctx);
    if (ret != PKG_SUCCESS) {
        (void)LZ4F_freeCompressionContext(ctx);
        return ret;
    }
    (void)LZ4F_freeCompressionContext(ctx);
    if (msg.context.srcOffset - context.srcOffset != context.unpackedSize) {
        PKG_LOGE("original size error %zu %zu", msg.context.srcOffset, context.unpackedSize);
        return PKG_INVALID_LZ4;
    }
    context.packedSize = msg.context.destOffset - context.destOffset;
    return PKG_SUCCESS;
}

int32_t PkgAlgorithmLz4::GetUnpackParam(LZ4F_decompressionContext_t &ctx, const PkgStreamPtr inStream,
    size_t &nextToRead, size_t &srcOffset)
{
    LZ4F_errorCode_t errorCode = 0;
    LZ4F_frameInfo_t frameInfo;

    PkgBuffer pkgHeader(LZ4S_HEADER_LEN);
    WriteLE32(pkgHeader.buffer, LZ4S_MAGIC_NUMBER);

    /* Decode stream descriptor */
    size_t readLen = 0;
    size_t outBuffSize = 0;
    size_t inBuffSize = 0;
    size_t sizeCheck = sizeof(LZ4B_MAGIC_NUMBER);
    nextToRead = LZ4F_decompress(ctx, nullptr, &outBuffSize, pkgHeader.buffer, &sizeCheck, nullptr);
    if (LZ4F_isError(nextToRead)) {
        PKG_LOGE("Fail to decode frame info %s", LZ4F_getErrorName(nextToRead));
        return PKG_INVALID_LZ4;
    }
    if (nextToRead > pkgHeader.length) {
        PKG_LOGE("Invalid pkgHeader.length %d", pkgHeader.length);
        return PKG_INVALID_LZ4;
    }

    size_t remainSize = LZ4S_HEADER_LEN;
    PkgBuffer inbuffer(nullptr, nextToRead);
    if (ReadData(inStream, srcOffset, inbuffer, remainSize, readLen) != PKG_SUCCESS) {
        PKG_LOGE("Fail read data ");
        return PKG_INVALID_LZ4;
    }
    if (readLen != nextToRead) {
        PKG_LOGE("Invalid len %zu %zu", readLen, nextToRead);
        return PKG_INVALID_LZ4;
    }
    srcOffset += readLen;
    sizeCheck = readLen;
    nextToRead = LZ4F_decompress(ctx, nullptr, &outBuffSize, inbuffer.buffer, &sizeCheck, nullptr);
    errorCode = LZ4F_getFrameInfo(ctx, &frameInfo, nullptr, &inBuffSize);
    if (LZ4F_isError(errorCode)) {
        PKG_LOGE("Fail to decode frame info %s", LZ4F_getErrorName(errorCode));
        return PKG_INVALID_LZ4;
    }
    if (frameInfo.blockSizeID < 3 || frameInfo.blockSizeID > 7) { // 3,7 : Check whether block size ID is valid
        PKG_LOGE("Invalid block size ID %d", frameInfo.blockSizeID);
        return PKG_INVALID_LZ4;
    }

    blockIndependence_ = frameInfo.blockMode;
    contentChecksumFlag_ = frameInfo.contentChecksumFlag;
    blockSizeID_ = frameInfo.blockSizeID;
    return PKG_SUCCESS;
}

int32_t PkgAlgorithmLz4::UnpackDecode(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgBufferMessage &msg, size_t &nextToRead, LZ4F_decompressionContext_t &ctx)
{
    /* Main Loop */
    while (nextToRead != 0) {
        size_t readLen = 0;
        size_t decodedBytes = msg.outBuffSize;
        if (nextToRead > msg.inBuffSize) {
            PKG_LOGE("Error next read %zu %zu ", nextToRead, msg.inBuffSize);
            break;
        }

        /* Read Block */
        msg.inBuffer.length = nextToRead;
        if (ReadData(inStream, msg.context.srcOffset, msg.inBuffer, msg.context.packedSize, readLen) != PKG_SUCCESS) {
            PKG_LOGE("Fail read data ");
            return PKG_INVALID_STREAM;
        }

        /* Decode Block */
        size_t sizeCheck = readLen;
        LZ4F_errorCode_t errorCode = LZ4F_decompress(ctx, msg.outBuffer.buffer,
            &decodedBytes, msg.inBuffer.buffer, &sizeCheck, nullptr);
        if (LZ4F_isError(errorCode)) {
            PKG_LOGE("Fail to decompress %s", LZ4F_getErrorName(errorCode));
            return PKG_INVALID_LZ4;
        }
        if (decodedBytes == 0) {
            msg.context.srcOffset += readLen;
            break;
        }
        if (sizeCheck != nextToRead) {
            PKG_LOGE("Error next read %zu %zu ", nextToRead, sizeCheck);
            break;
        }

        /* Write Block */
        if (outStream->Write(msg.outBuffer, decodedBytes, msg.context.destOffset) != PKG_SUCCESS) {
            PKG_LOGE("Fail write data ");
            return PKG_INVALID_STREAM;
        }
        msg.context.destOffset += decodedBytes;
        msg.context.srcOffset += readLen;
        nextToRead = errorCode;
    }
    return PKG_SUCCESS;
}

int32_t PkgAlgorithmLz4::Unpack(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context)
{
    if (inStream == nullptr || outStream == nullptr) {
        PKG_LOGE("Param context null!");
        return PKG_INVALID_PARAM;
    }
    LZ4F_decompressionContext_t ctx;
    LZ4F_errorCode_t errorCode = 0;
    size_t nextToRead = 0;
    PkgAlgorithmContext unpackText = context;
    errorCode = LZ4F_createDecompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) {
        PKG_LOGE("Fail to create compress context %s", LZ4F_getErrorName(errorCode));
        return PKG_INVALID_LZ4;
    }
    int32_t ret = GetUnpackParam(ctx, inStream, nextToRead, unpackText.srcOffset);
    if (ret != PKG_SUCCESS) {
        (void)LZ4F_freeDecompressionContext(ctx);
        PKG_LOGE("Fail to get param ");
        return PKG_INVALID_LZ4;
    }

    int32_t outBuffSize = GetBlockSizeFromBlockId(blockSizeID_);
    PKG_LOGI("Block size ID %d outBuffSize:%d", blockSizeID_, outBuffSize);
    int32_t inBuffSize = outBuffSize + static_cast<int32_t>(sizeof(uint32_t));
    if (inBuffSize <= 0 || outBuffSize <= 0) {
        (void)LZ4F_freeDecompressionContext(ctx);
        PKG_LOGE("Buffer size must > 0");
        return PKG_NONE_MEMORY;
    }
    size_t inLength = static_cast<size_t>(inBuffSize);
    size_t outLength = static_cast<size_t>(outBuffSize);
    PkgBuffer inBuffer(nullptr, inLength);
    PkgBuffer outBuffer(outLength);
    if (outBuffer.buffer == nullptr) {
        (void)LZ4F_freeDecompressionContext(ctx);
        PKG_LOGE("Fail to alloc buffer ");
        return PKG_NONE_MEMORY;
    }
    struct PkgBufferMessage msg { unpackText, inBuffer, outBuffer, inLength, outLength};
    ret = UnpackDecode(inStream, outStream, msg, nextToRead, ctx);

    errorCode = LZ4F_freeDecompressionContext(ctx);
    if (LZ4F_isError(errorCode)) {
        PKG_LOGE("Fail to free compress context %s", LZ4F_getErrorName(errorCode));
        return PKG_NONE_MEMORY;
    }
    context.packedSize = msg.context.srcOffset - context.srcOffset;
    context.unpackedSize = msg.context.destOffset - context.destOffset;
    return ret;
}

void PkgAlgorithmLz4::UpdateFileInfo(PkgManager::FileInfoPtr info) const
{
    Lz4FileInfo *lz4Info = (Lz4FileInfo *)info;
    lz4Info->fileInfo.packMethod = PKG_COMPRESS_METHOD_LZ4;
    lz4Info->fileInfo.digestMethod = PKG_DIGEST_TYPE_NONE;
    lz4Info->compressionLevel = compressionLevel_;
    lz4Info->blockIndependence = blockIndependence_;
    lz4Info->contentChecksumFlag = contentChecksumFlag_;
    lz4Info->blockSizeID = blockSizeID_;
    lz4Info->autoFlush = autoFlush_;
}
} // namespace Hpackage
