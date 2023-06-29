/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef PKG_INFO_UTILS_H
#define PKG_INFO_UTILS_H

#include <cstdint>
#include <functional>
#include <string>
#include "package/package.h"

namespace Hpackage {
struct FileInfo;
struct PkgInfo;
using FileInfoPtr = FileInfo *;
using PkgInfoPtr = PkgInfo *;

/**
 * Error code definition
 */
enum {
    PKG_SUCCESS = 0,
    PKG_ERROR_BASE = 100,
    PKG_INVALID_NAME,
    PKG_INVALID_PARAM,
    PKG_INVALID_FILE,
    PKG_INVALID_SIGNATURE,
    PKG_INVALID_PKG_FORMAT,
    PKG_INVALID_ALGORITHM,
    PKG_INVALID_DIGEST,
    PKG_INVALID_STREAM,
    PKG_INVALID_VERSION,
    PKG_INVALID_STATE,
    PKG_INVALID_LZ4,
    PKG_NONE_PERMISSION,
    PKG_NONE_MEMORY,
    PKG_VERIFY_FAIL,
};

enum {
    POST_TYPE_UPLOAD_PKG = 0,
    POST_TYPE_DECODE_PKG,
    POST_TYPE_VERIFY_PKG,
    POST_TYPE_WRITE_PARTITION
};

/**
 * Package information
 */
struct PkgInfo {
    uint32_t entryCount = 0;
    uint32_t updateFileHeadLen = 0;
    uint8_t signMethod;
    uint8_t digestMethod;
    uint8_t pkgType;
    uint8_t pkgFlags;
};

/**
 * File information
 */
struct FileInfo {
    uint8_t flags = 0;
    uint8_t digestMethod = 0;
    uint16_t packMethod = 0;
    time_t modifiedTime = 0;
    size_t packedSize = 0;
    size_t unpackedSize = 0;
    size_t headerOffset = 0;
    size_t dataOffset = 0;
    std::string identity;
};

/**
 * Header information of the update package
 */
struct UpgradePkgInfo {
    PkgInfo pkgInfo;
    uint32_t updateFileVersion = 0;
    std::string productUpdateId;
    std::string softwareVersion;
    std::string date;
    std::string time;
    std::string descriptPackageId;
};

/**
 * Component information of the update package
 */
struct ComponentInfo {
    FileInfo fileInfo;
    std::string version;
    uint8_t digest[DIGEST_MAX_LEN];
    uint16_t id;
    uint8_t resType;
    uint8_t type;
    uint8_t compFlags;
    size_t originalSize;
};

/**
 * Lz4 file configuration information
 */
struct Lz4FileInfo {
    FileInfo fileInfo;
    int8_t compressionLevel;
    int8_t blockIndependence;
    int8_t contentChecksumFlag;
    int8_t blockSizeID;
    int8_t autoFlush = 1;
};

/**
 * Zip file configuration information
 */
struct ZipFileInfo {
    FileInfo fileInfo;
    int32_t method = -1; // The system automatically uses the default value if the value is -1.
    int32_t level;
    int32_t windowBits;
    int32_t memLevel;
    int32_t strategy;
};

/**
 * buff definition used for parsing
 */
struct PkgBuffer {
    uint8_t *buffer;
    size_t length = 0; // buffer size

    std::vector<uint8_t> data;

    PkgBuffer()
    {
        this->buffer = nullptr;
        this->length = 0;
    }

    PkgBuffer(uint8_t *buffer, size_t bufferSize)
    {
        this->buffer = buffer;
        this->length = bufferSize;
    }

    PkgBuffer(std::vector<uint8_t> &buffer)
    {
        this->buffer = buffer.data();
        this->length = buffer.capacity();
    }

    PkgBuffer(size_t bufferSize)
    {
        data.resize(bufferSize, 0);
        this->buffer = data.data();
        this->length = bufferSize;
    }
};
} // namespace Hpackage
#endif // PKG_MANAGER_H
