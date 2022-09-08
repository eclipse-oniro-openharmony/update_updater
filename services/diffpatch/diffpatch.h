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

#ifndef DIFF_PATCH_H
#define DIFF_PATCH_H

#ifdef __WIN32
#include "pkg_utils.h"
#else
#include <sys/mman.h>
#endif
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include "log/log.h"
#include "patch/update_patch.h"

namespace UpdatePatch {
#ifdef __WIN32
#undef ERROR
#endif

#define PATCH_LOGE(format, ...) Logger(Updater::ERROR, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define PATCH_DEBUG(format, ...) Logger(Updater::DEBUG, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define PATCH_LOGI(format, ...) Logger(Updater::INFO, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define PATCH_LOGW(format, ...) Logger(Updater::WARNING, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)

#define PATCH_CHECK(retCode, exper, ...) \
    if (!(retCode)) { \
        PATCH_LOGE(__VA_ARGS__); \
        exper; \
    }

#define PATCH_ONLY_CHECK(retCode, exper) \
    if (!(retCode)) {                  \
        exper;                         \
    }

#define PATCH_IS_TRUE_DONE(retCode, exper) \
    if ((retCode)) {                  \
        exper;                         \
    }

enum {
    PATCH_SUCCESS = 0,
	PATCH_INVALID_PARAM,
    PATCH_NEW_FILE,
    PATCH_EXCEED_LIMIT,
    PATCH_INVALID_PATCH,
};

/*
 * The pkgdiff patch header looks like this:
 *
 *    "pkgdiff0"                  (8)   [magic number and version]
 *    block count                 (4)
 *    for each block:
 *        block type              (4)   [BLOCK_{NORMAL, DEFLATE, RAW, Lz4}]
 *        if block type == BLOCK_NORMAL:
 *           source start         (8)
 *           source len           (8)
 *           bsdiff patch offset  (8)   [from start of patch file]
 *        if block type == BLOCK_DEFLATE:
 *           source start         (8)
 *           source len           (8)
 *           bsdiff patch offset  (8)   [from start of patch file]
 *           source expanded len  (8)   [size of uncompressed source]
 *           target expected len  (8)   [size of uncompressed target]
 *           zip level            (4)
 *                method          (4)
 *                windowBits      (4)
 *                memLevel        (4)
 *                strategy        (4)
 *        if block type == BLOCK_LZ4:
 *           source start         (8)
 *           source len           (8)
 *           bsdiff patch offset  (8)   [from start of patch file]
 *           source expanded len  (8)   [size of uncompressed source]
 *           target expected len  (8)   [size of uncompressed target]
 *           lz4 level            (4)
 *                method          (4)
 *                blockIndependence     (4)
 *                contentChecksumFlag   (4)
 *                blockSizeID     (4)
 *                autoFlush       (4)
 *        if block type == RAW:
 *           target len           (4)
 *           data                 (target len)
 *
 */

/* Header is
    0	8	 "BSDIFF40"
    8	8	length of bzip2ed ctrl block
    16	8	length of bzip2ed diff block
    24	8	length of new file
*/
/* File is
    0	32	Header
    32	40	Bzip2ed ctrl block
    40	48	Bzip2ed diff block
    48	56	Bzip2ed extra block
*/

// patch block types
#define BLOCK_NORMAL 0
#define BLOCK_GZIP 1
#define BLOCK_DEFLATE 2
#define BLOCK_RAW 3
#define BLOCK_LZ4 4

static constexpr size_t GZIP_HEADER_LEN = 10;
static constexpr size_t VERSION = 2;
static constexpr unsigned short HEADER_CRC = 0x02; /* bit 1 set: CRC16 for the gzip header */
static constexpr unsigned short EXTRA_FIELD = 0x04; /* bit 2 set: extra field present */
static constexpr unsigned short ORIG_NAME = 0x08; /* bit 3 set: original file name present */
static constexpr unsigned short COMMENT = 0x10; /* bit 4 set: file comment present */
static constexpr unsigned short ENCRYPTED = 0x20; /* bit 5 set: file is encrypted */
static constexpr uint8_t SHIFT_RIGHT_FOUR_BITS = 4;

// The gzip footer size really is fixed.
static constexpr size_t GZIP_FOOTER_LEN = 8;
static constexpr size_t LZ4_HEADER_LEN = 4;
static constexpr size_t IGMDIFF_LIMIT_UNIT = 10240;

static constexpr int LZ4S_MAGIC = 0x184D2204;
static constexpr int LZ4B_MAGIC = 0x184C2102;
static constexpr int GZIP_MAGIC = 0x00088b1f;

static constexpr int PATCH_NORMAL_MIN_HEADER_LEN = 24;
static constexpr int PATCH_DEFLATE_MIN_HEADER_LEN = 60;
static constexpr int PATCH_LZ4_MIN_HEADER_LEN = 64;

constexpr const char *BSDIFF_MAGIC = "BSDIFF40";
constexpr const char *PKGDIFF_MAGIC = "PKGDIFF0";

struct PatchHeader {
    size_t srcStart = 0;
    size_t srcLength = 0;
    size_t patchOffset = 0;
    size_t expandedLen = 0;
    size_t targetSize = 0;
};

struct ControlData {
    int64_t diffLength;
    int64_t extraLength;
    int64_t offsetIncrement;
    uint8_t *diffNewStart;
    uint8_t *diffOldStart;
    uint8_t *extraNewStart;
};

struct MemMapInfo {
    uint8_t *memory {};
    size_t length {};
    int fd {-1};
    ~MemMapInfo()
    {
        if (memory != nullptr) {
            munmap(memory, length);
        }
        memory = nullptr;
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
    }
};

int32_t WriteDataToFile(const std::string &fileName, const std::vector<uint8_t> &data, size_t dataSize);
int32_t PatchMapFile(const std::string &fileName, MemMapInfo &info);
std::string GeneraterBufferHash(const BlockBuffer &buffer);
std::string ConvertSha256Hex(const BlockBuffer &buffer);
} // namespace UpdatePatch
#endif // DIFF_PATCH_H