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
#include "dump.h"
#include "pkg_utils.h"

namespace Hpackage {
struct Footer {
    uint16_t signDataStart;
    uint16_t signDataFlag;
    uint16_t signDataSize;
};

namespace {
constexpr uint32_t ZIP_EOCD_LEN_EXCLUDE_COMMENT = 20;
constexpr uint32_t ZIP_EOCD_FIXED_PART_LEN = 22;
constexpr uint32_t PKG_FOOTER_SIZE = 6;
constexpr uint32_t PKG_ZIP_EOCD_MIN_LEN = ZIP_EOCD_FIXED_PART_LEN + PKG_FOOTER_SIZE;
constexpr uint32_t ZIP_EOCD_SIGNATURE = 0x06054b50;
constexpr uint16_t PKG_ZIP_EOCD_FOOTER_FLAG = 0xFFFF;
const uint8_t ZIP_EOCD_SIGNATURE_BIG_ENDIAN[4] = {0x50, 0x4b, 0x05, 0x06};
}

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
        UPDATER_LAST_WORD(PKG_INVALID_PARAM);
        return PKG_INVALID_PARAM;
    }
    size_t fileLen = pkgStream->GetFileLength();
    size_t footerSize = PKG_FOOTER_SIZE;
    PKG_CHECK(fileLen > footerSize, UPDATER_LAST_WORD(PKG_INVALID_FILE, fileLen); return PKG_INVALID_FILE,
        "file len[%zu] < footerSize.", pkgStream->GetFileLength());
    size_t footerStart = fileLen - footerSize;
    size_t readLen = 0;
    PkgBuffer footer(footerSize);
    int32_t ret = pkgStream->Read(footer, footerStart, footerSize, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, UPDATER_LAST_WORD(ret);
        return ret, "read FOOTER struct failed %s", pkgStream->GetFileName().c_str());

    uint16_t signCommentAppendLen = 0;
    uint16_t signCommentTotalLen = 0;
    ret = ParsePkgFooter(footer.buffer, PKG_FOOTER_SIZE, signCommentAppendLen, signCommentTotalLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ParsePkgFooter() error, ret[%d]", ret);
        UPDATER_LAST_WORD(ret);
        return ret;
    }

    size_t eocdTotalLen = ZIP_EOCD_FIXED_PART_LEN + signCommentTotalLen;
    PKG_CHECK(fileLen > eocdTotalLen, UPDATER_LAST_WORD(PKG_INVALID_PKG_FORMAT);
        return PKG_INVALID_PKG_FORMAT, "Invalid eocd len[%zu]", eocdTotalLen);

    size_t zipEocdStart = fileLen - eocdTotalLen;
    PkgBuffer zipEocd(eocdTotalLen);
    ret = pkgStream->Read(zipEocd, zipEocdStart, eocdTotalLen, readLen);
    PKG_CHECK(ret == PKG_SUCCESS, UPDATER_LAST_WORD(PKG_INVALID_PKG_FORMAT);
        return ret, "read zip eocd failed %s", pkgStream->GetFileName().c_str());

    ret = CheckZipEocd(zipEocd.buffer, eocdTotalLen, signCommentTotalLen);
    PKG_CHECK(ret == PKG_SUCCESS, UPDATER_LAST_WORD(PKG_INVALID_PKG_FORMAT);
        return ret, "CheckZipEocd() error, ret[%d]", ret);

    PKG_CHECK(fileLen > signCommentTotalLen, UPDATER_LAST_WORD(PKG_INVALID_PKG_FORMAT, fileLen, signCommentTotalLen);
        return PKG_INVALID_FILE, "file len[%zu] < signCommentTotalLen[%zu]", fileLen, signCommentTotalLen);
    signatureStart = fileLen - signCommentTotalLen;
    signatureSize = signCommentTotalLen;

    return PKG_SUCCESS;
}

int32_t ZipPkgParse::ParsePkgFooter(const uint8_t *footer, size_t length,
    uint16_t &signCommentAppendLen, uint16_t &signCommentTotalLen) const
{
    if (length < PKG_FOOTER_SIZE) {
        PKG_LOGE("length[%d] < Footer Size[%d]", length, PKG_FOOTER_SIZE);
        UPDATER_LAST_WORD(PKG_INVALID_PARAM, length);
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
        UPDATER_LAST_WORD(PKG_INVALID_PKG_FORMAT, signFooter.signDataFlag);
        return PKG_INVALID_PKG_FORMAT;
    }

    signCommentAppendLen = signFooter.signDataStart;
    signCommentTotalLen = signFooter.signDataSize;
    if ((signCommentAppendLen < PKG_FOOTER_SIZE) || (signCommentTotalLen < PKG_FOOTER_SIZE) ||
        (signCommentAppendLen > signCommentTotalLen)) {
        PKG_LOGE("bad footer length: append[0x%04X], total[0x%04X]",
            signCommentAppendLen, signCommentTotalLen);
        UPDATER_LAST_WORD(PKG_INVALID_PKG_FORMAT, signCommentAppendLen, signCommentTotalLen);
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
        /* every 4 byte check if eocd, if eocd[i] = 0x50, eocd[i + 1] = 0x4b, eocd[i + 2] = 0x05, eocd[i + 3] = 0x06 */
        /* the zip contain another ecod, we can consider it's invalid zip */
        if (eocd[i] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[0] &&
            eocd[i + 1] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[1] &&
            eocd[i + 2] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[2] && /* eocd[i + 2] = 0x05 */
            eocd[i + 3] == ZIP_EOCD_SIGNATURE_BIG_ENDIAN[3]) { /* eocd[i + 3] = 0x06 */
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
} // namespace Hpackage
