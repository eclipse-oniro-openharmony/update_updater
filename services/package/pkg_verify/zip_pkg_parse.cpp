/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "zip_pkg_parse.h"
#include <vector>
#include "pkg_utils.h"

namespace hpackage {
struct Footer {
    uint16_t signDataStart;
    uint16_t signDataFlag;
    uint16_t signDataSize;
};

static constexpr uint32_t SOURCE_DATA_WRITE_BLOCK_LEN = 4096;
static constexpr uint32_t ZIP_EOCD_LEN_EXCLUDE_COMMENT = 20;
static constexpr uint32_t ZIP_EOCD_FIXED_PART_LEN = 22;
static constexpr uint32_t PKG_FOOTER_SIZE = 6;
static constexpr uint32_t PKG_ZIP_EOCD_MIN_LEN = ZIP_EOCD_FIXED_PART_LEN + PKG_FOOTER_SIZE;
static constexpr uint32_t ZIP_EOCD_SIGNATURE = 0x06054b50;
static constexpr uint16_t PKG_ZIP_EOCD_FOOTER_FLAG = 0xFFFF;
static constexpr uint32_t SECOND_BYTE = 2;
static constexpr uint32_t THIRD_BYTE = 3;
static const uint8_t ZIP_EOCD_SIGNATURE_BIG_ENDIAN[4] = {0x50, 0x4b, 0x05, 0x06};

/*
 * ZIP:  File Entry(1..n) + CD(1..n) + EOCD(1)
 *
 * EOCD: FLAG(4 bytes) + FIX PART1(16 bytes) + comment length(2 bytes) + comment('comment length' bytes)
 *
 * EOCD comment: RESERVED(18 bytes) + SIGNATYRE(variable size) + FOOTER (6 bytes)
 *
 * FOOTER                           6 bytes (little endian)
 *     append signed result length  2 bytes (SIGNATYRE's length + FOOTER's length) = SIGNATYRE reversed offset
 *     0xFFFF                       2 bytes
 *     = .ZIP file comment length   2 bytes
 */
int32_t ZipPkgParse::ParseZipPkg(PkgStreamPtr pkgStream, size_t &signatureStart, size_t &signatureSize) const
{
    if (pkgStream == nullptr) {
        return PKG_INVALID_PARAM;
    }
    size_t fileLen = pkgStream->GetFileLength();
    size_t footerSize = PKG_FOOTER_SIZE;
    PKG_CHECK(fileLen > footerSize, return PKG_INVALID_FILE,
        "file len[%zu] < footerSize.", pkgStream->GetFileLength());
    size_t footerStart = fileLen - footerSize;
    size_t readLen = 0;
    PkgBuffer footer(footerSize);
    int32_t ret = pkgStream->Read(footer, footerStart, footerSize, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "read FOOTER struct failed %s", pkgStream->GetFileName().c_str());

    uint16_t signCommentAppendLen = 0;
    uint16_t signCommentTotalLen = 0;
    ret = ParsePkgFooter(footer.buffer, PKG_FOOTER_SIZE, signCommentAppendLen, signCommentTotalLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ParsePkgFooter() error, ret[%d]", ret);
        return ret;
    }

    size_t eocdTotalLen = ZIP_EOCD_FIXED_PART_LEN + signCommentTotalLen;
    PKG_CHECK(fileLen > eocdTotalLen, return PKG_INVALID_PKG_FORMAT, "Invalid eocd len[%zu]", eocdTotalLen);

    size_t zipEocdStart = fileLen - eocdTotalLen;
    PkgBuffer zipEocd(eocdTotalLen);
    ret = pkgStream->Read(zipEocd, zipEocdStart, eocdTotalLen, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "read zip eocd failed %s", pkgStream->GetFileName().c_str());

    ret = CheckZipEocd(zipEocd.buffer, eocdTotalLen, signCommentTotalLen);
    PKG_CHECK(ret == PKG_SUCCESS, return ret, "CheckZipEocd() error, ret[%d]", ret);

    PKG_CHECK(fileLen > signCommentTotalLen, return PKG_INVALID_FILE,
        "file len[%zu] < signCommentTotalLen[%zu]", fileLen, signCommentTotalLen);
    signatureStart = fileLen - signCommentTotalLen;
    signatureSize = signCommentTotalLen;

    return PKG_SUCCESS;
}

int32_t ZipPkgParse::ParsePkgFooter(const uint8_t *footer, size_t length,
    uint16_t &signCommentAppendLen, uint16_t &signCommentTotalLen) const
{
    if (length < PKG_FOOTER_SIZE) {
        PKG_LOGE("length[%d] < Footer Size[%d]", length, PKG_FOOTER_SIZE);
        return PKG_INVALID_PARAM;
    }

    Footer signFooter = {0};
    size_t offset = 0;
    signFooter.signDataStart = ReadLE16(footer);
    offset += sizeof(uint16_t);
    signFooter.signDataFlag = ReadLE16(footer + offset);
    offset += sizeof(uint16_t);
    signFooter.signDataSize = ReadLE16(footer + offset);
    if (signFooter.signDataFlag != PKG_ZIP_EOCD_FOOTER_FLAG) {
        PKG_LOGE("error FooterFlag[0x%04X]", signFooter.signDataFlag);
        return PKG_INVALID_PKG_FORMAT;
    }

    signCommentAppendLen = signFooter.signDataStart;
    signCommentTotalLen = signFooter.signDataSize;
    if ((signCommentAppendLen < PKG_FOOTER_SIZE) || (signCommentTotalLen < PKG_FOOTER_SIZE) ||
        (signCommentAppendLen > signCommentTotalLen)) {
        PKG_LOGE("bad footer length: append[0x%04X], total[0x%04X]",
            signCommentAppendLen, signCommentTotalLen);
        return PKG_INVALID_PKG_FORMAT;
    }

    return PKG_SUCCESS;
}

int32_t ZipPkgParse::CheckZipEocd(const uint8_t *eocd, size_t length,
    uint16_t signCommentTotalLen) const
{
    if (length < PKG_ZIP_EOCD_MIN_LEN) {
        PKG_LOGE("bad eocd length: append[0x%04X]", length);
        return PKG_INVALID_PKG_FORMAT;
    }

    uint32_t eocdSignature = ReadLE32(eocd);
    if (eocdSignature != ZIP_EOCD_SIGNATURE) {
        PKG_LOGE("bad zip eocd flag[%zu]", eocdSignature);
        return PKG_INVALID_PKG_FORMAT;
    }

    /* the beginning 4 chars are already checked before, so begin with i = 4; (length - 3) in case for overflow */
    for (size_t i = 4; i < length - 3; i++) {
        if (eocd[i] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[0] &&
            eocd[i + 1] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[1] &&
            eocd[i + SECOND_BYTE] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[SECOND_BYTE] &&
            eocd[i + THIRD_BYTE] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[THIRD_BYTE]) {
            PKG_LOGE("EOCD marker occurs after start of EOCD");
            return PKG_INVALID_PKG_FORMAT;
        }
    }

    const uint8_t *zipSignCommentAddr = eocd + ZIP_EOCD_LEN_EXCLUDE_COMMENT;
    uint16_t tempLen = ReadLE16(zipSignCommentAddr);
    if (signCommentTotalLen != tempLen) {
        PKG_LOGE("compare sign comment length: eocd[0x%04X], footer[0x%04X] error", tempLen, signCommentTotalLen);
        return PKG_INVALID_PKG_FORMAT;
    }

    return PKG_SUCCESS;
}

int32_t ZipPkgParse::CheckZipPkg(const PkgStreamPtr pkgStream) const
{
    size_t fileLen = pkgStream->GetFileLength();
    if (fileLen <= ZIP_EOCD_FIXED_PART_LEN) {
        PKG_LOGE("Invalid file len %zu", fileLen);
        return PKG_INVALID_FILE;
    }

    PkgBuffer zipEocd(ZIP_EOCD_FIXED_PART_LEN);
    size_t eocdStart = fileLen - ZIP_EOCD_FIXED_PART_LEN;
    size_t readLen = 0;
    int32_t ret = pkgStream->Read(zipEocd, eocdStart, ZIP_EOCD_FIXED_PART_LEN, readLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("read eocd failed %s", pkgStream->GetFileName().c_str());
        return PKG_INVALID_FILE;
    }

    uint32_t eocdSignature = ReadLE32(zipEocd.buffer);
    if (eocdSignature != ZIP_EOCD_SIGNATURE) {
        PKG_LOGE("Zip pkg has been signed.");
        return PKG_INVALID_FILE;
    }

    return PKG_SUCCESS;
}

int32_t ZipPkgParse::WriteZipSignedData(PkgStreamPtr outStream, const PkgBuffer &p7Data, PkgStreamPtr inStream) const
{
    size_t offset = 0;
    size_t fileSize = inStream->GetFileLength();
    size_t srcDataLen = fileSize - sizeof(uint16_t);
    int32_t ret = WriteSourcePackageData(outStream, inStream, srcDataLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to write src data");
        return ret;
    }
    offset += srcDataLen;

    uint16_t zipCommentLen = p7Data.length + PKG_FOOTER_SIZE;
    std::vector<uint8_t> buff(sizeof(uint16_t));
    WriteLE16(buff.data(), zipCommentLen);
    PkgBuffer buffer(buff);
    ret = outStream->Write(buffer, sizeof(uint16_t), offset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to write zip eocd comment len");
        return ret;
    }
    offset += sizeof(uint16_t);

    ret = outStream->Write(p7Data, p7Data.length, offset);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to write pkcs7 signed data");
        return ret;
    }
    offset += p7Data.length;

    return WriteFooter(outStream, zipCommentLen, offset);
}

int32_t ZipPkgParse::WriteSourcePackageData(PkgStreamPtr outStream, PkgStreamPtr inStream, size_t wirteLen) const
{
    size_t offset = 0;
    size_t remainLen = wirteLen;
    size_t blockLen = SOURCE_DATA_WRITE_BLOCK_LEN;
    PkgBuffer buffer(blockLen);
    size_t readLen = 0;
    int32_t ret = PKG_SUCCESS;
    while (remainLen >= blockLen) {
        ret = inStream->Read(buffer, offset, blockLen, readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail read data");
        ret = outStream->Write(buffer, blockLen, offset);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail write data");

        offset += blockLen;
        remainLen -= blockLen;
    }
    if (remainLen > 0) {
        ret = inStream->Read(buffer, offset, remainLen, readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail read data");
        ret = outStream->Write(buffer, remainLen, offset);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "Fail write data");
    }

    return ret;
}

int32_t ZipPkgParse::WriteFooter(PkgStreamPtr outStream, uint16_t zipCommentLen, size_t &offset) const
{
    Footer footer = {0};
    footer.signDataStart = zipCommentLen;
    footer.signDataFlag = 0xFFFF;
    footer.signDataSize = zipCommentLen;

    std::vector<uint8_t> buff(sizeof(Footer));
    WriteLE16(buff.data() + offsetof(Footer, signDataStart), footer.signDataStart);
    WriteLE16(buff.data() + offsetof(Footer, signDataFlag), footer.signDataFlag);
    WriteLE16(buff.data() + offsetof(Footer, signDataSize), footer.signDataSize);
    PkgBuffer buffer(buff);

    return outStream->Write(buffer, sizeof(Footer), offset);
}
} // namespace hpackage
