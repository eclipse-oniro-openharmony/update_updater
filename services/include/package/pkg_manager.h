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

#ifndef PKG_MANAGER_H
#define PKG_MANAGER_H

#include <cstdlib>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include "ring_buffer/ring_buffer.h"
#include "package/package.h"
#include "pkg_info_utils.h"

namespace Hpackage {
class PkgFile;
class PkgStream;
class PkgEntry;
using PkgFilePtr = PkgFile *;
using PkgStreamPtr = PkgStream *;
using PkgEntryPtr = PkgEntry *;

/**
 * Input and output stream definition
 */
class PkgStream {
public:
    enum {
        PkgStreamType_Read = 0,     // common file reading
        PkgStreamType_Write,        // common file writing (A new file is created and the original content is deleted.)
        PkgStreamType_MemoryMap,    // memory mapping
        PkgStreamType_Process,      // processing while parsing
        PkgStreamType_Buffer,       // buffer
        PKgStreamType_FileMap,      // file map to memory
        PkgStreamType_FlowData,     // flow data
    };

    virtual ~PkgStream() = default;

    /**
     * Read files.
     *
     * @param buff                  buffer to hold the output file content
     * @param start                 start position of reading
     * @param needRead              size of the data to read
     * @param readLen               length of the read data
     * @return                      file reading result
     */
    virtual int32_t Read(PkgBuffer &data, size_t start, size_t needRead, size_t &readLen) = 0;

    /**
     * write stream
     *
     * @param data                  buffer to hold the output file content
     * @param size                  size of the data to read
     * @param start                 start position of reading
     * @return                      file reading result
     */
    virtual int32_t Write(const PkgBuffer &data, size_t size, size_t start) = 0;

    virtual int32_t Flush(size_t size) = 0;

    virtual int32_t GetBuffer(PkgBuffer &buffer) const = 0;

    virtual size_t GetFileLength() = 0;
    virtual const std::string GetFileName() const = 0;
    virtual int32_t GetStreamType() const = 0;

    virtual void AddRef() = 0;
    virtual void DelRef() = 0;
    virtual bool IsRef() const = 0;

    using ExtractFileProcessor = std::function<int(const PkgBuffer &data, size_t size, size_t start,
        bool isFinish, const void *context)>;

    int32_t GetBuffer(uint8_t *&buffer, size_t &size)
    {
        PkgBuffer data = {};
        int ret = GetBuffer(data);
        buffer = data.buffer;
        size = data.length;
        return ret;
    }

    virtual size_t GetReadOffset() const
    {
        return 0;
    }

    void Stop()
    {
        return;
    }
};

class PkgFile {
public:
    enum PkgType {
        PKG_TYPE_NONE = PKG_PACK_TYPE_NONE,
        PKG_TYPE_UPGRADE = PKG_PACK_TYPE_UPGRADE, // 升级包
        PKG_TYPE_ZIP = PKG_PACK_TYPE_ZIP,     // zip压缩包
        PKG_TYPE_LZ4 = PKG_PACK_TYPE_LZ4,     // lz4压缩包
        PKG_TYPE_GZIP = PKG_PACK_TYPE_GZIP,     // gzip压缩包
        PKG_TYPE_MAX
    };

    using VerifyFunction = std::function<int(const PkgInfoPtr info,
        const std::vector<uint8_t> &digest, const std::vector<uint8_t> &signature)>;

public:
    virtual ~PkgFile() = default;

    virtual int32_t AddEntry(const FileInfoPtr file, const PkgStreamPtr input) = 0;

    virtual int32_t SavePackage(size_t &signOffset) = 0;

    virtual int32_t ExtractFile(const PkgEntryPtr node, const PkgStreamPtr output) = 0;

    virtual int32_t LoadPackage(std::vector<std::string> &fileNames, VerifyFunction verifier = nullptr) = 0;

    virtual int32_t ParseComponents(std::vector<std::string> &fileNames) = 0;

    virtual PkgEntryPtr FindPkgEntry(const std::string &fileName) = 0;

    virtual PkgStreamPtr GetPkgStream() const = 0;

    virtual const PkgInfo *GetPkgInfo() const = 0;

    virtual PkgType GetPkgType() const = 0;

    virtual void ClearPkgStream() = 0;
};

class PkgEntry {
public:
    PkgEntry(PkgFilePtr pkgFile, uint32_t nodeId) : nodeId_(nodeId), pkgFile_(pkgFile) {}

    virtual ~PkgEntry() {}

    virtual int32_t Init(const FileInfoPtr fileInfo, PkgStreamPtr inStream) = 0;

    virtual int32_t EncodeHeader(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen) = 0;

    virtual int32_t Pack(PkgStreamPtr inStream, size_t startOffset, size_t &encodeLen) = 0;

    virtual int32_t DecodeHeader(PkgBuffer &buffer, size_t headerOffset, size_t dataOffset,
        size_t &decodeLen) = 0;

    virtual int32_t Unpack(PkgStreamPtr outStream) = 0;

    virtual const std::string GetFileName() const
    {
        return fileName_;
    };

    virtual const FileInfo *GetFileInfo() const = 0;

    PkgFilePtr GetPkgFile() const
    {
        return pkgFile_;
    }

    uint32_t GetNodeId() const
    {
        return nodeId_;
    }

    void AddDataOffset(size_t offset)
    {
        dataOffset_ += offset;
    }

protected:
    int32_t Init(FileInfoPtr localFileInfo, const FileInfoPtr fileInfo,
        PkgStreamPtr inStream);

protected:
    uint32_t nodeId_ {0};
    PkgFilePtr pkgFile_ {nullptr};
    size_t headerOffset_ {0};
    size_t dataOffset_ {0};
    std::string fileName_ {};
};

using PkgDecodeProgress = std::function<void(int type, size_t writeDataLen, const void *context)>;

/**
 * Get a singleton PkgManager instance.
 */
class PkgManager {
public:
    using PkgManagerPtr = PkgManager *;
    using FileInfoPtr = FileInfo *;
    using PkgInfoPtr = PkgInfo *;
    using StreamPtr = PkgStream *;
    using VerifyCallback = std::function<void(int32_t result, uint32_t percent)>;
    using PkgFileConstructor = std::function<PkgFilePtr(
        PkgManagerPtr manager, PkgStreamPtr stream, PkgManager::PkgInfoPtr header)>;

    virtual ~PkgManager() = default;

    virtual void RegisterPkgFileCreator(const std::string &fileType, PkgFileConstructor constructor) = 0;

    static PkgManagerPtr CreatePackageInstance();
    static PkgManagerPtr GetPackageInstance();
    static void ReleasePackageInstance(PkgManagerPtr manager);

    /**
     * Create an update package based on specified parameters.
     *
     * @param path              path of the update package
     * @param header            header, which mainly consists of algorithm information
     * @param files             packed file list
     * @return                  packaging result, with the package saved as the file specified in path
     */
    virtual int32_t CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
        std::vector<std::pair<std::string, ZipFileInfo>> &files) = 0;
    virtual int32_t CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
        std::vector<std::pair<std::string, ComponentInfo>> &files) = 0;
    virtual int32_t CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
        std::vector<std::pair<std::string, Lz4FileInfo>> &files) = 0;

    /**
     * Verify the signature of the upgrade package.
     *
     * @param packagePath       file name of the update package
     * @param keyPath           file name of the key used for verification
     * @param version           version number of the update package to download
     * @param digest            digest value
     * @param size              digest value size
     * @return                  verification result
     */
    virtual int32_t VerifyPackage(const std::string &packagePath, const std::string &keyPath,
        const std::string &version, const PkgBuffer &digest, VerifyCallback cb) = 0;

    /**
     * Load and parse the update package.
     *
     * @param packagePath       file name of the update package
     * @param fileIds           returned file ID list
     * @param middleTofile      file saving mode during intermediate parsing.
     * @return                  loading and parsing result
     */
    virtual int32_t LoadPackage(const std::string &packagePath, const std::string &keyPath,
        std::vector<std::string> &fileIds) = 0;

    virtual int32_t VerifyOtaPackage(const std::string &packagePath) = 0;

    virtual int32_t VerifyBinFile(const std::string &packagePath, const std::string &keyPath,
        const std::string &version, const PkgBuffer &digest) = 0;

    /**
     * Get the information about the update package.
     *
     * @param packagePath   file name of the update package
     * @return              information about the update package
     */
    virtual const PkgInfo *GetPackageInfo(const std::string &packagePath) = 0;

    /**
     * Extract files from the update package, parse the files, and verify the hash value.
     *
     * @param fileId        File ID, which is obtained from the fileIds returned by the LoadPackage function
     * @param output        output of the extracted files
     * @return              read operation result
     */
    virtual int32_t ExtractFile(const std::string &fileId, StreamPtr output) = 0;

    /**
     * Obtain information about the files in the update package.
     *
     * @param fileId        file ID
     * @return              file information
     */
    virtual const FileInfo *GetFileInfo(const std::string &fileId) = 0;

    /**
     * Create a a package stream to output.
     *
     * @param stream        stream for io management
     * @param fileName      file name corresponding to the stream
     * @param size          file size
     * @param type          stream type
     * @return              creation result; false if no access permission
     */
    virtual int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, size_t size,
        int32_t type) = 0;

    /**
     * Create a package stream that can be processed while parsing.
     *
     * @param stream        stream used for io management
     * @param fileName      file name corresponding to the stream
     * @param processor     content processor
     * @param context       context for the processor
     * @return              creation result
     */
    virtual int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName,
        PkgStream::ExtractFileProcessor processor, const void *context) = 0;

    /**
     * Create a package stream that can be processed while parsing.
     *
     * @param stream        stream used for io management
     * @param fileName      file name corresponding to the stream
     * @param buffer        buffer
     * @return              creation result
     */
    virtual int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, const PkgBuffer &buffer) = 0;

    /**
     * Create a package stream that can be processed while parsing.
     *
     * @param stream        stream used for io management
     * @param fileName      file name corresponding to the stream
     * @param buffer        ringbuffer
     * @return              creation result
     */
    virtual int32_t CreatePkgStream(StreamPtr &stream, const std::string &fileName, Updater::RingBuffer *buffer) = 0;

    /**
     * Close the stream
     *
     * @param stream  stream对象
     */
    virtual void ClosePkgStream(StreamPtr &stream) = 0;

    virtual int32_t DecompressBuffer(FileInfoPtr info, const PkgBuffer &buffer, StreamPtr output) const = 0;
    virtual int32_t CompressBuffer(FileInfoPtr info, const PkgBuffer &buffer, StreamPtr output) const = 0;

    virtual int32_t LoadPackageWithoutUnPack(const std::string &packagePath,
        std::vector<std::string> &fileIds) = 0;

    virtual int32_t LoadPackageWithStream(const std::string &packagePath, const std::string &keyPath,
        std::vector<std::string> &fileIds, uint8_t type, StreamPtr stream) = 0;

    virtual int32_t ParsePackage(StreamPtr stream, std::vector<std::string> &fileIds, int32_t type) = 0;

    virtual void SetPkgDecodeProgress(PkgDecodeProgress decodeProgress) = 0;

    virtual void PostDecodeProgress(int type, size_t writeDataLen, const void *context) = 0;

    virtual StreamPtr GetPkgFileStream(const std::string &fileName) = 0;

    virtual int32_t ParseComponents(const std::string &packagePath, std::vector<std::string> &fileName) = 0;
};

template <typename FileClassName>
PkgFilePtr NewPkgFile(PkgManager::PkgManagerPtr manager, PkgStreamPtr stream, PkgInfoPtr header)
{
    return new FileClassName (manager, stream, header);
}
} // namespace Hpackage
#endif // PKG_MANAGER_H
