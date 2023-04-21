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
#include "pkg_algo_deflate.h"
#include "pkg_stream.h"
#include "pkg_utils.h"
#include "securec.h"
#include "zlib.h"

namespace Hpackage {
constexpr uint32_t DEFLATE_IN_BUFFER_SIZE = 1024 * 64;
constexpr uint32_t DEFLATE_OUT_BUFFER_SIZE = 1024 * 32;
constexpr uint32_t INFLATE_IN_BUFFER_SIZE = 1024 * 1024 * 1024;
constexpr uint32_t INFLATE_OUT_BUFFER_SIZE = 1024 * 1024;

int32_t PkgAlgoDeflate::DeflateData(const PkgStreamPtr outStream, z_stream &zstream, int32_t flush,
    PkgBuffer &outBuffer, size_t &destOffset) const
{
    int32_t ret = Z_OK;
    do {
        size_t deflateLen = 0;
        ret = deflate(&zstream, flush);
        if (ret < Z_OK) {
            PKG_LOGE("deflate finish error ret1 %d", ret);
            return PKG_NOT_EXIST_ALGORITHM;
        }
        deflateLen += outBuffer.length - zstream.avail_out;
        if (deflateLen > 0) {
            int32_t ret1 = outStream->Write(outBuffer, deflateLen, destOffset);
            if (ret1 != PKG_SUCCESS) {
                PKG_LOGE("error write data deflateLen: %zu", deflateLen);
                break;
            }
            destOffset += deflateLen;
            zstream.next_out = outBuffer.buffer;
            zstream.avail_out = outBuffer.length;
        }
        if (flush == Z_NO_FLUSH) {
            break;
        }
    } while (ret == Z_OK && flush == Z_FINISH);
    return PKG_SUCCESS;
}

int32_t PkgAlgoDeflate::PackCalculate(PkgAlgorithmContext &context, const PkgStreamPtr inStream,
    const PkgStreamPtr outStream, const DigestAlgorithm::DigestAlgorithmPtr algorithm)
{
    PkgBuffer inBuffer = {};
    PkgBuffer outBuffer = {};
    z_stream zstream;
    int32_t ret = InitStream(zstream, true, inBuffer, outBuffer);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("fail InitStream");
        return ret;
    }
    size_t remainSize = context.unpackedSize;
    uint32_t crc = 0;
    size_t srcOffset = context.srcOffset;
    size_t destOffset = context.destOffset;
    PkgBuffer crcResult((uint8_t *)&crc, sizeof(crc));

    while ((remainSize > 0) || (zstream.avail_in > 0)) {
        size_t readLen = 0;
        if (zstream.avail_in == 0) {
            ret = ReadData(inStream, srcOffset, inBuffer, remainSize, readLen);
            if (ret != PKG_SUCCESS) {
                PKG_LOGE("Read data fail!");
                break;
            }
            zstream.next_in = reinterpret_cast<unsigned char *>(inBuffer.buffer);
            zstream.avail_in = readLen;
            srcOffset += readLen;
            // Calculate CRC of original file
            algorithm->Calculate(crcResult, inBuffer, readLen);
        }
        ret = DeflateData(outStream, zstream, ((remainSize == 0) ? Z_FINISH : Z_NO_FLUSH), outBuffer, destOffset);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("error write data deflateLen: %zu", destOffset);
            break;
        }
    }
    ReleaseStream(zstream, true);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("error write data");
        return ret;
    }
    if (srcOffset != context.unpackedSize) {
        PKG_LOGE("original size error %zu %zu", srcOffset, context.unpackedSize);
        return PKG_INVALID_PKG_FORMAT;
    }
    context.crc = crc;
    context.packedSize = destOffset - context.destOffset;
    return ret;
}

int32_t PkgAlgoDeflate::Pack(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context)
{
    if (inStream == nullptr || outStream == nullptr) {
        PKG_LOGE("Param context null!");
        return PKG_INVALID_PARAM;
    }
    DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(context.digestMethod);
    if (algorithm == nullptr) {
        PKG_LOGE("Can not get digest algor");
        return PKG_NOT_EXIST_ALGORITHM;
    }
    return PackCalculate(context, inStream, outStream, algorithm);
}

int32_t PkgAlgoDeflate::ReadUnpackData(const PkgStreamPtr inStream, PkgBuffer &inBuffer,
    z_stream &zstream, PkgAlgorithmContext &context, size_t &readLen)
{
    if (zstream.avail_in == 0) {
        int32_t ret = ReadData(inStream, context.srcOffset, inBuffer, context.packedSize, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Read data fail!");
            return ret;
        }
        zstream.next_in = reinterpret_cast<uint8_t *>(inBuffer.buffer);
        zstream.avail_in = readLen;
        context.srcOffset += readLen;
    }
    return PKG_SUCCESS;
}

int32_t PkgAlgoDeflate::CalculateUnpackData(z_stream &zstream, uint32_t &crc, int32_t &ret,
    PkgAlgorithmContext &context, PkgAlgorithmContext &unpackContext)
{
    ReleaseStream(zstream, false);
    context.packedSize = context.packedSize - zstream.avail_in - unpackContext.packedSize;
    context.unpackedSize = unpackContext.destOffset - context.destOffset;
    if (context.crc != 0 && context.crc != crc) {
        PKG_LOGE("crc fail %u %u!", crc, context.crc);
        return PKG_VERIFY_FAIL;
    }
    context.crc = crc;
    return (ret == Z_STREAM_END) ? PKG_SUCCESS : ret;
}

int32_t PkgAlgoDeflate::UnpackCalculate(PkgAlgorithmContext &context, const PkgStreamPtr inStream,
    const PkgStreamPtr outStream, DigestAlgorithm::DigestAlgorithmPtr algorithm)
{
    z_stream zstream;
    PkgBuffer inBuffer = {};
    PkgBuffer outBuffer = {};
    int32_t ret = InitStream(zstream, false, inBuffer, outBuffer);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("fail InitStream ");
        return ret;
    }
    size_t inflateLen = 0;
    uint32_t crc = 0;

    PkgBuffer crcResult((uint8_t *)&crc, sizeof(crc));
    PkgAlgorithmContext unpackContext = context;
    size_t readLen = 0;
    int32_t unpack_ret = Z_OK;
    while ((unpackContext.packedSize > 0) || (unpack_ret != Z_STREAM_END)) {
        inBuffer.length = INFLATE_IN_BUFFER_SIZE;
        if (ReadUnpackData(inStream, inBuffer, zstream, unpackContext, readLen) != PKG_SUCCESS) {
            break;
        }
        unpack_ret = inflate(&zstream, Z_SYNC_FLUSH);
        if (unpack_ret < Z_OK) {
            PKG_LOGE("fail inflate ret:%d", unpack_ret);
            break;
        }

        if (zstream.avail_out == 0 || (unpack_ret == Z_STREAM_END && zstream.avail_out != INFLATE_OUT_BUFFER_SIZE)) {
            inflateLen = outBuffer.length - zstream.avail_out;
            ret = outStream->Write(outBuffer, inflateLen, unpackContext.destOffset);
            if (ret != PKG_SUCCESS) {
                PKG_LOGE("write data is fail!");
                break;
            }
            unpackContext.destOffset += inflateLen;
            zstream.next_out = outBuffer.buffer;
            zstream.avail_out = outBuffer.length;

            algorithm->Calculate(crcResult, outBuffer, inflateLen);
        }
    }
    return CalculateUnpackData(zstream, crc, unpack_ret, context, unpackContext);
}

int32_t PkgAlgoDeflate::Unpack(const PkgStreamPtr inStream, const PkgStreamPtr outStream,
    PkgAlgorithmContext &context)
{
    DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(context.digestMethod);
    if (algorithm == nullptr) {
        PKG_LOGE("Can not get digest algor");
        return PKG_NOT_EXIST_ALGORITHM;
    }
    if (inStream == nullptr || outStream == nullptr) {
        PKG_LOGE("Param context null!");
        return PKG_INVALID_PARAM;
    }

    return UnpackCalculate(context, inStream, outStream, algorithm);
}

int32_t PkgAlgoDeflate::InitStream(z_stream &zstream, bool zip, PkgBuffer &inBuffer, PkgBuffer &outBuffer)
{
    int32_t ret = PKG_SUCCESS;
    // init zlib stream
    if (memset_s(&zstream, sizeof(z_stream), 0, sizeof(z_stream)) != EOK) {
        PKG_LOGE("memset fail");
        return PKG_NONE_MEMORY;
    }
    if (zip) {
        PKG_LOGI("InitStream level_:%d method_:%d windowBits_:%d memLevel_:%d strategy_:%d",
            level_, method_, windowBits_, memLevel_, strategy_);
        ret = deflateInit2(&zstream, level_, method_, windowBits_, memLevel_, strategy_);
        if (ret != Z_OK) {
            PKG_LOGE("fail deflateInit2 ret %d", ret);
            return PKG_NOT_EXIST_ALGORITHM;
        }
        inBuffer.length = DEFLATE_IN_BUFFER_SIZE;
        outBuffer.length = DEFLATE_OUT_BUFFER_SIZE;
    } else {
        ret = inflateInit2(&zstream, windowBits_);
        if (ret != Z_OK) {
            PKG_LOGE("fail deflateInit2");
            return PKG_NOT_EXIST_ALGORITHM;
        }
        inBuffer.length = INFLATE_IN_BUFFER_SIZE;
        outBuffer.length = INFLATE_OUT_BUFFER_SIZE;
    }

    outBuffer.data.resize(outBuffer.length);
    outBuffer.buffer = reinterpret_cast<uint8_t *>(outBuffer.data.data());
    zstream.next_out = outBuffer.buffer;
    zstream.avail_out = outBuffer.length;
    return PKG_SUCCESS;
}

void PkgAlgoDeflate::ReleaseStream(z_stream &zstream, bool zip) const
{
    int32_t ret = Z_OK;
    if (zip) {
        ret = deflateEnd(&zstream);
        if (ret != Z_OK) {
            PKG_LOGE("fail deflateEnd %d", ret);
            return;
        }
    } else {
        ret = inflateEnd(&zstream);
        if (ret != Z_OK) {
            PKG_LOGE("fail inflateEnd %d", ret);
            return;
        }
    }
}
} // namespace Hpackage
