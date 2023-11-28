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
#include "pkg_manager_impl.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include "dump.h"
#include "pkg_gzipfile.h"
#include "pkg_lz4file.h"
#include "pkg_manager.h"
#include "pkg_upgradefile.h"
#include "pkg_verify_util.h"
#include "pkg_zipfile.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "zip_pkg_parse.h"

using namespace std;
using namespace Updater;

namespace Hpackage {
constexpr int32_t BUFFER_SIZE = 4096;
constexpr int32_t DIGEST_INFO_NO_SIGN = 0;
constexpr int32_t DIGEST_INFO_HAS_SIGN = 1;
constexpr int32_t DIGEST_INFO_SIGNATURE = 2;
constexpr int32_t DIGEST_FLAGS_NO_SIGN = 1;
constexpr int32_t DIGEST_FLAGS_HAS_SIGN = 2;
constexpr int32_t DIGEST_FLAGS_SIGNATURE = 4;
constexpr uint32_t VERIFY_FINSH_PERCENT = 100;

PkgManager::PkgManagerPtr PkgManager::CreatePackageInstance()
{
    return new(std::nothrow) PkgManagerImpl();
}

void PkgManager::ReleasePackageInstance(PkgManager::PkgManagerPtr manager)
{
    if (manager == nullptr) {
        return;
    }
    delete manager;
    manager = nullptr;
}

PkgManagerImpl::PkgManagerImpl()
{
    RegisterPkgFileCreator("bin", NewPkgFile<UpgradePkgFile>);
    RegisterPkgFileCreator("zip", NewPkgFile<ZipPkgFile>);
    RegisterPkgFileCreator("lz4", NewPkgFile<Lz4PkgFile>);
    RegisterPkgFileCreator("gz", NewPkgFile<GZipPkgFile>);
}

PkgManagerImpl::~PkgManagerImpl()
{
    ClearPkgFile();
}

void PkgManagerImpl::ClearPkgFile()
{
    auto iter = pkgFiles_.begin();
    while (iter != pkgFiles_.end()) {
        PkgFilePtr file = (*iter);
        delete file;
        file = nullptr;
        iter = pkgFiles_.erase(iter);
    }
    std::lock_guard<std::mutex> lock(mapLock_);
    auto iter1 = pkgStreams_.begin();
    while (iter1 != pkgStreams_.end()) {
        PkgStreamPtr stream = (*iter1).second;
        delete stream;
        stream = nullptr;
        iter1 = pkgStreams_.erase(iter1);
    }
}

int32_t PkgManagerImpl::CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
    std::vector<std::pair<std::string, ZipFileInfo>> &files)
{
    int32_t ret = SetSignVerifyKeyName(keyName);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ZipFileInfo Invalid keyname");
        return ret;
    }
    if (files.size() <= 0 || header == nullptr) {
        PKG_LOGE("ZipFileInfo Invalid param");
        return PKG_INVALID_PARAM;
    }
    size_t offset = 0;
    PkgFilePtr pkgFile = CreatePackage<ZipFileInfo>(path, header, files, offset);
    if (pkgFile == nullptr) {
        return PKG_INVALID_FILE;
    }
    delete pkgFile;
    pkgFile = nullptr;
    return ret;
}

int32_t PkgManagerImpl::CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
    std::vector<std::pair<std::string, ComponentInfo>> &files)
{
    int32_t ret = SetSignVerifyKeyName(keyName);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("ComponentInfo Invalid keyname");
        return ret;
    }
    if (files.size() <= 0 || header == nullptr) {
        PKG_LOGE("ComponentInfo sssInvalid param");
        return PKG_INVALID_PARAM;
    }
    size_t offset = 0;
    PkgFilePtr pkgFile = CreatePackage<ComponentInfo>(path, header, files, offset);
    if (pkgFile == nullptr) {
        return PKG_INVALID_FILE;
    }
    ret = Sign(pkgFile->GetPkgStream(), offset, header);
    delete pkgFile;
    pkgFile = nullptr;
    return ret;
}

int32_t PkgManagerImpl::CreatePackage(const std::string &path, const std::string &keyName, PkgInfoPtr header,
    std::vector<std::pair<std::string, Lz4FileInfo>> &files)
{
    int32_t ret = SetSignVerifyKeyName(keyName);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Invalid keyname");
        return ret;
    }
    if (files.size() != 1 || header == nullptr) {
        PKG_LOGE("Invalid param");
        return PKG_INVALID_PARAM;
    }
    size_t offset = 0;
    PkgFilePtr pkgFile = CreatePackage<Lz4FileInfo>(path, header, files, offset);
    if (pkgFile == nullptr) {
        return PKG_INVALID_FILE;
    }
    ret = Sign(pkgFile->GetPkgStream(), offset, header);
    delete pkgFile;
    pkgFile = nullptr;
    return ret;
}

template<class T>
PkgFilePtr PkgManagerImpl::CreatePackage(const std::string &path, PkgInfoPtr header,
    std::vector<std::pair<std::string, T>> &files, size_t &offset)
{
    PkgStreamPtr stream = nullptr;
    int32_t ret = CreatePkgStream(stream, path, 0, PkgStream::PkgStreamType_Write);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("CreatePackage fail %s", path.c_str());
        return nullptr;
    }

    PkgFilePtr pkgFile = CreatePackage(PkgStreamImpl::ConvertPkgStream(stream),
        static_cast<PkgFile::PkgType>(header->pkgType), header);
    if (pkgFile == nullptr) {
        PKG_LOGE("CreatePackage fail %s", path.c_str());
        ClosePkgStream(stream);
        return nullptr;
    }

    PkgStreamPtr inputStream = nullptr;
    for (size_t i = 0; i < files.size(); i++) {
        ret = CreatePkgStream(inputStream, files[i].first, 0, PkgStream::PkgStreamType_Read);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Create stream fail %s", files[i].first.c_str());
            break;
        }
        ret = pkgFile->AddEntry(reinterpret_cast<const FileInfoPtr>(&(files[i].second)), inputStream);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("Add entry fail %s", files[i].first.c_str());
            break;
        }
        ClosePkgStream(inputStream);
        inputStream = nullptr;
    }
    if (ret != PKG_SUCCESS) {
        ClosePkgStream(inputStream);
        delete pkgFile;
        pkgFile = nullptr;
        return nullptr;
    }
    ret = pkgFile->SavePackage(offset);
    if (ret != PKG_SUCCESS) {
        delete pkgFile;
        pkgFile = nullptr;
        return nullptr;
    }
    return pkgFile;
}

void PkgManagerImpl::RegisterPkgFileCreator(const std::string &fileType, PkgFileConstructor constructor)
{
    if (!pkgFileCreator_.emplace(fileType, constructor).second) {
        LOG(ERROR) << "emplace: " << fileType << " fail";
    }
}

PkgFilePtr PkgManagerImpl::CreatePackage(PkgStreamPtr stream, PkgFile::PkgType type, PkgInfoPtr header)
{
    UNUSED(type);
    PkgFilePtr pkgFile = nullptr;
    std::string pkgName = stream->GetFileName();
    std::string pkgType = GetPkgName(pkgName);
    auto iter = pkgFileCreator_.find(pkgType);
    if (iter == pkgFileCreator_.end()) {
        LOG(ERROR) << "fileType is not registered: " << pkgType;
        return pkgFile;
    }
    pkgFile = iter->second(this, stream, header);
    return pkgFile;
}

int32_t PkgManagerImpl::LoadPackageWithoutUnPack(const std::string &packagePath,
    std::vector<std::string> &fileIds)
{
    PkgFile::PkgType pkgType = GetPkgTypeByName(packagePath);
    int32_t ret = LoadPackage(packagePath, fileIds, pkgType);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Parse %s fail ", packagePath.c_str());
        ClearPkgFile();
        return ret;
    }
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::ParsePackage(StreamPtr stream, std::vector<std::string> &fileIds, int32_t type)
{
    if (stream == nullptr) {
        PKG_LOGE("Invalid stream");
        return PKG_INVALID_PARAM;
    }
    PkgFilePtr pkgFile = CreatePackage(static_cast<PkgStreamPtr>(stream), static_cast<PkgFile::PkgType>(type), nullptr);
    if (pkgFile == nullptr) {
        PKG_LOGE("Create package fail %s", stream->GetFileName().c_str());
        return PKG_INVALID_PARAM;
    }

    int32_t ret = pkgFile->LoadPackage(fileIds,
        [](const PkgInfoPtr info, const std::vector<uint8_t> &digest, const std::vector<uint8_t> &signature)->int {
            return PKG_SUCCESS;
        });
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Load package fail %s", stream->GetFileName().c_str());
        pkgFile->ClearPkgStream();
        delete pkgFile;
        return ret;
    }
    pkgFiles_.push_back(pkgFile);
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::LoadPackage(const std::string &packagePath, const std::string &keyPath,
    std::vector<std::string> &fileIds)
{
    if (access(packagePath.c_str(), 0) != 0) {
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return PKG_INVALID_FILE;
    }
    if (SetSignVerifyKeyName(keyPath) != PKG_SUCCESS) {
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return PKG_INVALID_FILE;
    }
    // Check if package already loaded
    for (auto iter : pkgFiles_) {
        if (iter != nullptr && iter->GetPkgStream()->GetFileName().compare(packagePath) == 0) {
            return PKG_SUCCESS;
        }
    }
    PkgFile::PkgType pkgType = GetPkgTypeByName(packagePath);
    unzipToFile_ = ((pkgType == PkgFile::PKG_TYPE_GZIP) ? true : unzipToFile_);
    if (pkgType == PkgFile::PKG_TYPE_UPGRADE) {
        int32_t ret = LoadPackage(packagePath, fileIds, pkgType);
        if (ret != PKG_SUCCESS) {
            ClearPkgFile();
            UPDATER_LAST_WORD(ret);
            PKG_LOGE("Parse %s fail ", packagePath.c_str());
            return ret;
        }
    } else if (pkgType != PkgFile::PKG_TYPE_NONE) {
        std::vector<std::string> innerFileNames;
        int32_t ret = LoadPackage(packagePath, innerFileNames, pkgType);
        if (ret != PKG_SUCCESS) {
            ClearPkgFile();
            PKG_LOGE("Unzip %s fail ", packagePath.c_str());
            return ret;
        }
        for (auto name : innerFileNames) {
            pkgType = GetPkgTypeByName(name);
            if (pkgType == PkgFile::PKG_TYPE_NONE || (pkgType == PkgFile::PKG_TYPE_UPGRADE
                && std::find(innerFileNames.begin(), innerFileNames.end(), "board_list") != innerFileNames.end())) {
                fileIds.push_back(name);
                continue;
            }
            ret = ExtraAndLoadPackage(GetFilePath(packagePath), name, pkgType, fileIds);
            if (ret != PKG_SUCCESS) {
                ClearPkgFile();
                UPDATER_LAST_WORD(ret);
                PKG_LOGE("unpack %s fail in package %s ", name.c_str(), packagePath.c_str());
                return ret;
            }
        }
    }
    return PKG_SUCCESS;
}

const std::string PkgManagerImpl::GetExtraPath(const std::string &path)
{
    if (path.find(Updater::SDCARD_CARD_PATH) != string::npos) {
        return path;
    } else if (path == UPDATRE_SCRIPT_ZIP) {
        return "/tmp/";
    }

    return string(Updater::UPDATER_PATH) + "/";
}

int32_t PkgManagerImpl::ExtraAndLoadPackage(const std::string &path, const std::string &name,
    PkgFile::PkgType type, std::vector<std::string> &fileIds)
{
    int32_t ret = PKG_SUCCESS;
    const FileInfo *info = GetFileInfo(name);
    if (info == nullptr) {
        PKG_LOGE("Create middle stream fail %s", name.c_str());
        return PKG_INVALID_FILE;
    }

    PkgStreamPtr stream = nullptr;
    struct stat st {};
    const std::string tempPath = GetExtraPath(path);
    if (stat(tempPath.c_str(), &st) != 0) {
#ifndef __WIN32
        (void)mkdir(tempPath.c_str(), 0775); // 0775 : rwxrwxr-x
#endif
    }

    // Extract package to file or memory
    if (unzipToFile_ || type == PkgFile::PKG_TYPE_UPGRADE) {
        ret = CreatePkgStream(stream, tempPath + name + ".tmp", info->unpackedSize, PkgStream::PkgStreamType_Write);
    } else {
        ret = CreatePkgStream(stream, tempPath + name + ".tmp", info->unpackedSize, PkgStream::PkgStreamType_MemoryMap);
    }
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Create middle stream fail %s", name.c_str());
        return ret;
    }

    ret = ExtractFile(name, stream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Extract file fail %s", name.c_str());
        ClosePkgStream(stream);
        return ret;
    }
    return LoadPackageWithStream(path, fileIds, type, stream);
}

int32_t PkgManagerImpl::LoadPackage(const std::string &packagePath, std::vector<std::string> &fileIds,
    PkgFile::PkgType type)
{
    PkgStreamPtr stream = nullptr;
    int32_t ret = CreatePkgStream(stream, packagePath, 0, PkgStream::PKgStreamType_FileMap);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Create input stream fail %s", packagePath.c_str());
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    return LoadPackageWithStream(packagePath, fileIds, type, stream);
}

int32_t PkgManagerImpl::LoadPackageWithStream(const std::string &packagePath, const std::string &keyPath,
    std::vector<std::string> &fileIds, uint8_t type, StreamPtr stream)
{
    int32_t ret = SetSignVerifyKeyName(keyPath);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Invalid keyname");
        return ret;
    }

    return LoadPackageWithStream(packagePath, fileIds, static_cast<PkgFile::PkgType>(type),
        static_cast<PkgStreamPtr>(stream));
}

int32_t PkgManagerImpl::LoadPackageWithStream(const std::string &packagePath,
    std::vector<std::string> &fileIds, PkgFile::PkgType type, PkgStreamPtr stream)
{
    int32_t ret = PKG_SUCCESS;
    PkgFilePtr pkgFile = CreatePackage(stream, type, nullptr);
    if (pkgFile == nullptr) {
        PKG_LOGE("Create package fail %s", packagePath.c_str());
        ClosePkgStream(stream);
        UPDATER_LAST_WORD(ret);
        return PKG_INVALID_PARAM;
    }

    ret = pkgFile->LoadPackage(fileIds,
        [this](const PkgInfoPtr info, const std::vector<uint8_t> &digest, const std::vector<uint8_t> &signature)->int {
            return Verify(info->digestMethod, digest, signature);
        });
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Load package fail %s", packagePath.c_str());
        delete pkgFile;
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    pkgFiles_.push_back(pkgFile);
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::ExtractFile(const std::string &path, PkgManager::StreamPtr output)
{
    if (output == nullptr) {
        PKG_LOGE("Invalid stream");
        UPDATER_LAST_WORD(PKG_INVALID_STREAM);
        return PKG_INVALID_STREAM;
    }
    int32_t ret = PKG_INVALID_FILE;
    PkgEntryPtr pkgEntry = GetPkgEntry(path);
    if (pkgEntry != nullptr && pkgEntry->GetPkgFile() != nullptr) {
        ret = pkgEntry->GetPkgFile()->ExtractFile(pkgEntry, PkgStreamImpl::ConvertPkgStream(output));
    } else {
        PKG_LOGE("Can not find file %s", path.c_str());
    }
    return ret;
}

int32_t PkgManagerImpl::ParseComponents(const std::string &packagePath, std::vector<std::string> &fileName)
{
    int32_t ret = PKG_INVALID_FILE;
    for (auto iter : pkgFiles_) {
        PkgFilePtr pkgFile = iter;
        if (pkgFile != nullptr && pkgFile->GetPkgType() == PkgFile::PKG_TYPE_UPGRADE) {
            return pkgFile->ParseComponents(fileName);
        }
    }
    return ret;
}

const PkgInfo *PkgManagerImpl::GetPackageInfo(const std::string &packagePath)
{
    for (auto iter : pkgFiles_) {
        PkgFilePtr pkgFile = iter;
        if (pkgFile != nullptr && pkgFile->GetPkgType() == PkgFile::PKG_TYPE_UPGRADE) {
            return pkgFile->GetPkgInfo();
        }
    }
    return nullptr;
}

const FileInfo *PkgManagerImpl::GetFileInfo(const std::string &path)
{
    PkgEntryPtr pkgEntry = GetPkgEntry(path);
    if (pkgEntry != nullptr) {
        return pkgEntry->GetFileInfo();
    }
    return nullptr;
}

PkgEntryPtr PkgManagerImpl::GetPkgEntry(const std::string &path)
{
    // Find out pkgEntry by fileId.
    for (auto iter : pkgFiles_) {
        PkgFilePtr pkgFile = iter;
        PkgEntryPtr pkgEntry = pkgFile->FindPkgEntry(path);
        if (pkgEntry == nullptr) {
            continue;
        }
        return pkgEntry;
    }
    return nullptr;
}

PkgManager::StreamPtr PkgManagerImpl::GetPkgFileStream(const std::string &fileName)
{
    auto iter = pkgStreams_.find(fileName);
    if (iter != pkgStreams_.end()) {
        return (*iter).second;
    }

    return nullptr;
}

int32_t PkgManagerImpl::CreatePkgStream(StreamPtr &stream, const std::string &fileName, const PkgBuffer &buffer)
{
    PkgStreamPtr pkgStream = new MemoryMapStream(this, fileName, buffer, PkgStream::PkgStreamType_Buffer);
    if (pkgStream == nullptr) {
        PKG_LOGE("Failed to create stream");
        return -1;
    }
    stream = pkgStream;
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::CreatePkgStream(StreamPtr &stream, const std::string &fileName,
    uint64_t fileLen, RingBuffer *buffer)
{
    PkgStreamPtr pkgStream = new(std::nothrow) FlowDataStream(this, fileName, fileLen,
        buffer, PkgStream::PkgStreamType_FlowData);
    if (pkgStream == nullptr) {
        PKG_LOGE("Failed to create stream");
        return -1;
    }
    stream = pkgStream;
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::DoCreatePkgStream(PkgStreamPtr &stream, const std::string &fileName, int32_t type)
{
    static char const *modeFlags[] = { "rb", "wb+" };
    char realPath[PATH_MAX + 1] = {};
#ifdef _WIN32
    if (type == PkgStream::PkgStreamType_Read && _fullpath(realPath, fileName.c_str(), PATH_MAX) == nullptr) {
#else
    if (type == PkgStream::PkgStreamType_Read && realpath(fileName.c_str(), realPath) == nullptr) {
#endif
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return PKG_INVALID_FILE;
    }
    if (CheckFile(fileName, type) != PKG_SUCCESS) {
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        PKG_LOGE("Fail to check file %s ", fileName.c_str());
        return PKG_INVALID_FILE;
    }
    std::lock_guard<std::mutex> lock(mapLock_);
    if (pkgStreams_.find(fileName) != pkgStreams_.end()) {
        PkgStreamPtr mapStream = pkgStreams_[fileName];
        mapStream->AddRef();
        stream = mapStream;
        return PKG_SUCCESS;
    }
    FILE *file = nullptr;
    if (type == PkgStream::PkgStreamType_Read) {
        file = fopen(realPath, modeFlags[type]);
    } else {
        file = fopen(fileName.c_str(), modeFlags[type]);
    }
    if (file == nullptr) {
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        PKG_LOGE("Fail to open file %s ", fileName.c_str());
        return PKG_INVALID_FILE;
    }
    stream = new FileStream(this, fileName, file, type);
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::CreatePkgStream(PkgStreamPtr &stream, const std::string &fileName, size_t size, int32_t type)
{
    Updater::UPDATER_INIT_RECORD;
    stream = nullptr;
    if (type == PkgStream::PkgStreamType_Write || type == PkgStream::PkgStreamType_Read) {
        int32_t ret = DoCreatePkgStream(stream, fileName, type);
        if (ret != PKG_SUCCESS) {
            UPDATER_LAST_WORD(ret);
            return ret;
        }
    } else if (type == PkgStream::PkgStreamType_MemoryMap || type == PkgStream::PKgStreamType_FileMap) {
        if ((size == 0) && (access(fileName.c_str(), 0) != 0)) {
            UPDATER_LAST_WORD(PKG_INVALID_FILE);
            return PKG_INVALID_FILE;
        }
        size_t fileSize = (size == 0) ? GetFileSize(fileName) : size;
        if (fileSize <= 0) {
            UPDATER_LAST_WORD(PKG_INVALID_FILE);
            PKG_LOGE("Fail to check file size %s ", fileName.c_str());
            return PKG_INVALID_FILE;
        }
        uint8_t *memoryMap = nullptr;
        if (type == PkgStream::PkgStreamType_MemoryMap) {
            memoryMap = AnonymousMap(fileName, fileSize);
        } else {
            memoryMap = FileMap(fileName);
        }
        if (memoryMap == nullptr) {
            UPDATER_LAST_WORD(PKG_INVALID_FILE);
            PKG_LOGE("Fail to map memory %s ", fileName.c_str());
            return PKG_INVALID_FILE;
        }
        PkgBuffer buffer(memoryMap, fileSize);
        stream = new MemoryMapStream(this, fileName, buffer);
    } else {
        UPDATER_LAST_WORD(-1);
        return -1;
    }
    std::lock_guard<std::mutex> lock(mapLock_);
    pkgStreams_[fileName] = stream;
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::CreatePkgStream(PkgStreamPtr &stream, const std::string &fileName,
    PkgStream::ExtractFileProcessor processor, const void *context)
{
    stream = new ProcessorStream(this, fileName, processor, context);
    if (stream == nullptr) {
        PKG_LOGE("Failed to create stream");
        return -1;
    }
    return PKG_SUCCESS;
}

void PkgManagerImpl::ClosePkgStream(PkgStreamPtr &stream)
{
    PkgStreamPtr mapStream = stream;
    if (mapStream == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mapLock_);
    auto iter = pkgStreams_.find(mapStream->GetFileName());
    if (iter != pkgStreams_.end()) {
        mapStream->DelRef();
        if (mapStream->IsRef()) {
            return;
        }
        pkgStreams_.erase(iter);
    }
    delete mapStream;
    stream = nullptr;
}

std::string PkgManagerImpl::GetPkgName(const std::string &path)
{
    std::size_t pos = path.find_last_of('.');
    if (pos == std::string::npos || pos < 1) {
        return "";
    }
    std::string pkgName = path.substr(pos + 1, -1);
    std::transform(pkgName.begin(), pkgName.end(), pkgName.begin(), ::tolower);
    if (pkgName.compare("tmp") != 0) {
        return pkgName;
    }
    std::size_t secPos = path.find_last_of('.', pos - 1);
    if (secPos == std::string::npos) {
        return "";
    }
    std::string secPkgName = path.substr(secPos + 1, pos - secPos - 1);
    std::transform(secPkgName.begin(), secPkgName.end(), secPkgName.begin(), ::tolower);
    return secPkgName;
}

PkgFile::PkgType PkgManagerImpl::GetPkgTypeByName(const std::string &path)
{
    std::size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return PkgFile::PKG_TYPE_NONE;
    }
    std::string postfix = path.substr(pos + 1, -1);
    std::transform(postfix.begin(), postfix.end(), postfix.begin(), ::tolower);

    if (path.compare("update.bin") == 0) {
        return PkgFile::PKG_TYPE_UPGRADE;
    } else if (path.substr(pos + 1, -1).compare("zip") == 0) {
        return PkgFile::PKG_TYPE_ZIP;
    } else if (path.substr(pos + 1, -1).compare("lz4") == 0) {
        return PkgFile::PKG_TYPE_LZ4;
    } else if (path.substr(pos + 1, -1).compare("gz") == 0) {
        return PkgFile::PKG_TYPE_GZIP;
    }
    return PkgFile::PKG_TYPE_NONE;
}

int32_t PkgManagerImpl::VerifyPackage(const std::string &packagePath, const std::string &keyPath,
    const std::string &version, const PkgBuffer &digest, VerifyCallback cb)
{
    int32_t ret = SetSignVerifyKeyName(keyPath);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Invalid keyname");
        return ret;
    }

    PkgFile::PkgType type = GetPkgTypeByName(packagePath);
    if (type != PkgFile::PKG_TYPE_UPGRADE) {
        ret = VerifyOtaPackage(packagePath);
    } else if (digest.buffer != nullptr) {
        ret = VerifyBinFile(packagePath, keyPath, version, digest);
    } else {
        PkgManager::PkgManagerPtr pkgManager = PkgManager::CreatePackageInstance();
        if (pkgManager == nullptr) {
            PKG_LOGE("pkgManager is nullptr");
            return PKG_INVALID_SIGNATURE;
        }
        std::vector<std::string> components;
        ret = pkgManager->LoadPackage(packagePath, keyPath, components);
        PkgManager::ReleasePackageInstance(pkgManager);
    }
    cb(ret, VERIFY_FINSH_PERCENT);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Verify file %s fail", packagePath.c_str());
        return ret;
    }
    PKG_LOGI("Verify file %s success", packagePath.c_str());
    return ret;
}

int32_t PkgManagerImpl::DoGenerateFileDigest(PkgStreamPtr stream, uint8_t flags, const size_t fileLen,
    PkgBuffer &buff, std::pair<DigestAlgorithm::DigestAlgorithmPtr, DigestAlgorithm::DigestAlgorithmPtr> &algorithm)
{
    size_t offset = 0;
    size_t readLen = 0;
    size_t needReadLen = fileLen;
    size_t buffSize = BUFFER_SIZE;
    if (flags & DIGEST_FLAGS_SIGNATURE) {
        if (SIGN_TOTAL_LEN >= fileLen) {
            return PKG_INVALID_SIGNATURE;
        }
        needReadLen = fileLen - SIGN_TOTAL_LEN;
    }
    while (offset < needReadLen) {
        if ((needReadLen - offset) < buffSize) {
            buffSize = needReadLen - offset;
        }
        int32_t ret = stream->Read(buff, offset, buffSize, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("read buffer fail %s", stream->GetFileName().c_str());
            return ret;
        }
        if (flags & DIGEST_FLAGS_HAS_SIGN) {
            algorithm.first->Update(buff, readLen);
        }
        if (flags & DIGEST_FLAGS_NO_SIGN) {
            algorithm.second->Update(buff, readLen);
        }
        offset += readLen;
        PostDecodeProgress(POST_TYPE_VERIFY_PKG, readLen, nullptr);
        readLen = 0;
    }

    // Read last signatureLen
    if (flags & DIGEST_FLAGS_SIGNATURE) {
        readLen = 0;
        int32_t ret = stream->Read(buff, offset, SIGN_TOTAL_LEN, readLen);
        if (ret != PKG_SUCCESS) {
            PKG_LOGE("read buffer failed %s", stream->GetFileName().c_str());
            return ret;
        }
        if (flags & DIGEST_FLAGS_HAS_SIGN) {
            algorithm.first->Update(buff, readLen);
        }
        PkgBuffer data(SIGN_TOTAL_LEN);
        if (flags & DIGEST_FLAGS_NO_SIGN) {
            algorithm.second->Update(data, SIGN_TOTAL_LEN);
        }
    }
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::GenerateFileDigest(PkgStreamPtr stream,
    uint8_t digestMethod, uint8_t flags, std::vector<std::vector<uint8_t>> &digestInfos, size_t hashBufferLen)
{
    size_t fileLen = (hashBufferLen == 0) ? stream->GetFileLength() : hashBufferLen;
    size_t digestLen = DigestAlgorithm::GetDigestLen(digestMethod);
    size_t signatureLen = DigestAlgorithm::GetSignatureLen(digestMethod);
    // Check entire package
    DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(digestMethod);
    if (algorithm == nullptr) {
        PKG_LOGE("Invalid file %s", stream->GetFileName().c_str());
        return PKG_NOT_EXIST_ALGORITHM;
    }
    algorithm->Init();
    // Get verify algorithm
    DigestAlgorithm::DigestAlgorithmPtr algorithmInner = PkgAlgorithmFactory::GetDigestAlgorithm(digestMethod);
    if (algorithmInner == nullptr) {
        PKG_LOGE("Invalid file %s", stream->GetFileName().c_str());
        return PKG_NOT_EXIST_ALGORITHM;
    }
    algorithmInner->Init();
    PkgBuffer buff(BUFFER_SIZE);
    std::pair<DigestAlgorithm::DigestAlgorithmPtr, DigestAlgorithm::DigestAlgorithmPtr> digestAlgorithm(
        algorithm, algorithmInner);
    int32_t ret = DoGenerateFileDigest(stream, flags, fileLen, buff, digestAlgorithm);
    if (ret != PKG_SUCCESS) {
        return ret;
    }
    if (flags & DIGEST_FLAGS_HAS_SIGN) {
        PkgBuffer result(digestInfos[DIGEST_INFO_HAS_SIGN].data(), digestLen);
        algorithm->Final(result);
    }
    if (flags & DIGEST_FLAGS_NO_SIGN) {
        PkgBuffer result(digestInfos[DIGEST_INFO_NO_SIGN].data(), digestLen);
        algorithmInner->Final(result);
    }

    if (flags & DIGEST_FLAGS_SIGNATURE) {
        uint8_t *buffer = buff.buffer;
        if (digestMethod != PKG_DIGEST_TYPE_SHA256) {
            buffer = buff.buffer + SIGN_SHA256_LEN;
        }
        if (memcpy_s(digestInfos[DIGEST_INFO_SIGNATURE].data(), signatureLen, buffer, signatureLen) != EOK) {
            PKG_LOGE("GenerateFileDigest memcpy failed");
            return PKG_NONE_MEMORY;
        }
    }
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::Verify(uint8_t digestMethod, const std::vector<uint8_t> &digest,
    const std::vector<uint8_t> &signature)
{
    SignAlgorithm::SignAlgorithmPtr signAlgorithm = PkgAlgorithmFactory::GetVerifyAlgorithm(
        signVerifyKeyName_, digestMethod);
    if (signAlgorithm == nullptr) {
        PKG_LOGE("Invalid sign algo");
        return PKG_INVALID_SIGNATURE;
    }
    return signAlgorithm->VerifyDigest(digest, signature);
}

int32_t PkgManagerImpl::Sign(PkgStreamPtr stream, size_t offset, const PkgInfoPtr &info)
{
    if (info == nullptr) {
        PKG_LOGE("Invalid param");
        return PKG_INVALID_PARAM;
    }
    if (info->signMethod == PKG_SIGN_METHOD_NONE) {
        return PKG_SUCCESS;
    }

    size_t digestLen = DigestAlgorithm::GetDigestLen(info->digestMethod);
    std::vector<std::vector<uint8_t>> digestInfos(DIGEST_INFO_SIGNATURE + 1);
    digestInfos[DIGEST_INFO_HAS_SIGN].resize(digestLen);
    int32_t ret = GenerateFileDigest(stream, info->digestMethod, DIGEST_FLAGS_HAS_SIGN, digestInfos);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to generate signature %s", stream->GetFileName().c_str());
        return ret;
    }
    SignAlgorithm::SignAlgorithmPtr signAlgorithm =
        PkgAlgorithmFactory::GetSignAlgorithm(signVerifyKeyName_, info->signMethod, info->digestMethod);
    if (signAlgorithm == nullptr) {
        PKG_LOGE("Invalid sign algo");
        return PKG_INVALID_SIGNATURE;
    }
    size_t signLen = DigestAlgorithm::GetSignatureLen(info->digestMethod);
    std::vector<uint8_t> signedData(signLen, 0);
    // Clear buffer
    PkgBuffer signBuffer(signedData);
    ret = stream->Write(signBuffer, signLen, offset);
    size_t signDataLen = 0;
    signedData.clear();
    PkgBuffer digest(digestInfos[DIGEST_INFO_HAS_SIGN].data(), digestLen);
    ret = signAlgorithm->SignBuffer(digest, signedData, signDataLen);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to SignBuffer %s", stream->GetFileName().c_str());
        return ret;
    }
    if (signDataLen > signLen) {
        PKG_LOGE("SignData len %zu more %zu", signDataLen, signLen);
        return PKG_INVALID_SIGNATURE;
    }
    PKG_LOGI("Signature %zu %zu %s", offset, signDataLen, stream->GetFileName().c_str());
    ret = stream->Write(signBuffer, signDataLen, offset);
    stream->Flush(offset + signedData.size());
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail to Write signature %s", stream->GetFileName().c_str());
        return ret;
    }
    PKG_LOGW("Sign file %s success", stream->GetFileName().c_str());
    return ret;
}

int32_t PkgManagerImpl::SetSignVerifyKeyName(const std::string &keyName)
{
    Updater::UPDATER_INIT_RECORD;
    if (access(keyName.c_str(), 0) != 0) {
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        PKG_LOGE("Invalid keyname");
        return PKG_INVALID_FILE;
    }
    signVerifyKeyName_ = keyName;
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::DecompressBuffer(FileInfoPtr info, const PkgBuffer &buffer, StreamPtr stream) const
{
    if (info == nullptr || buffer.buffer == nullptr || stream == nullptr) {
        PKG_LOGE("DecompressBuffer Param is null");
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(info);
    if (algorithm == nullptr) {
        PKG_LOGE("DecompressBuffer Can not get algorithm for %s", info->identity.c_str());
        return PKG_INVALID_PARAM;
    }

    std::shared_ptr<MemoryMapStream> inStream = std::make_shared<MemoryMapStream>(
        (PkgManager::PkgManagerPtr)this, info->identity, buffer, PkgStream::PkgStreamType_Buffer);
    if (inStream == nullptr) {
        PKG_LOGE("DecompressBuffer Can not create stream for %s", info->identity.c_str());
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithmContext context = {{0, 0}, {buffer.length, 0}, 0, info->digestMethod};
    int32_t ret = algorithm->Unpack(inStream.get(), PkgStreamImpl::ConvertPkgStream(stream), context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Decompress for %s", info->identity.c_str());
        return ret;
    }
    PKG_LOGI("packedSize: %zu unpackedSize: %zu ", buffer.length, context.unpackedSize);
    PkgStreamImpl::ConvertPkgStream(stream)->Flush(context.unpackedSize);
    info->packedSize = context.packedSize;
    info->unpackedSize = context.unpackedSize;
    algorithm->UpdateFileInfo(info);
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::CompressBuffer(FileInfoPtr info, const PkgBuffer &buffer, StreamPtr stream) const
{
    if (info == nullptr || buffer.buffer == nullptr || stream == nullptr) {
        PKG_LOGE("CompressBuffer Param is null");
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithm::PkgAlgorithmPtr algorithm = PkgAlgorithmFactory::GetAlgorithm(info);
    if (algorithm == nullptr) {
        PKG_LOGE("CompressBuffer Can not get algorithm for %s", info->identity.c_str());
        return PKG_INVALID_PARAM;
    }

    std::shared_ptr<MemoryMapStream> inStream = std::make_shared<MemoryMapStream>(
        (PkgManager::PkgManagerPtr)this, info->identity, buffer, PkgStream::PkgStreamType_Buffer);
    if (inStream == nullptr) {
        PKG_LOGE("CompressBuffer Can not create stream for %s", info->identity.c_str());
        return PKG_INVALID_PARAM;
    }
    PkgAlgorithmContext context = {{0, 0}, {0, buffer.length}, 0, info->digestMethod};
    int32_t ret = algorithm->Pack(inStream.get(), PkgStreamImpl::ConvertPkgStream(stream), context);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Fail Decompress for %s", info->identity.c_str());
        return ret;
    }
    PKG_LOGI("packedSize: %zu unpackedSize: %zu ", context.packedSize, context.unpackedSize);
    PkgStreamImpl::ConvertPkgStream(stream)->Flush(context.packedSize);
    info->packedSize = context.packedSize;
    info->unpackedSize = context.unpackedSize;
    return PKG_SUCCESS;
}

void PkgManagerImpl::PostDecodeProgress(int type, size_t writeDataLen, const void *context)
{
    if (decodeProgress_ != nullptr) {
        decodeProgress_(type, writeDataLen, context);
    }
}

int32_t PkgManagerImpl::VerifyOtaPackage(const std::string &packagePath)
{
    PkgStreamPtr pkgStream = nullptr;
    int32_t ret = CreatePkgStream(pkgStream, packagePath, 0, PkgStream::PkgStreamType_Read);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("CreatePackage fail %s", packagePath.c_str());
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
        return ret;
    }

    PkgVerifyUtil verifyUtil;
    ret = verifyUtil.VerifyPackageSign(pkgStream);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Verify zpkcs7 signature failed.");
        UPDATER_LAST_WORD(ret);
        ClosePkgStream(pkgStream);
        return ret;
    }

    ClosePkgStream(pkgStream);
    return PKG_SUCCESS;
}

int32_t PkgManagerImpl::VerifyBinFile(const std::string &packagePath, const std::string &keyPath,
    const std::string &version, const PkgBuffer &digest)
{
    PkgStreamPtr stream = nullptr;
    int32_t ret = CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Create input stream fail %s", packagePath.c_str());
        return ret;
    }
    size_t fileLen = stream->GetFileLength();
    if (fileLen <= 0) {
        PKG_LOGE("invalid file to load");
        ClosePkgStream(stream);
        return PKG_INVALID_FILE;
    }

    int8_t digestMethod = static_cast<int8_t>(DigestAlgorithm::GetDigestMethod(version));
    size_t digestLen = DigestAlgorithm::GetDigestLen(digestMethod);
    size_t signatureLen = DigestAlgorithm::GetSignatureLen(digestMethod);
    if (digestLen != digest.length) {
        PKG_LOGE("Invalid digestLen");
        ClosePkgStream(stream);
        return PKG_INVALID_PARAM;
    }
    std::vector<std::vector<uint8_t>> digestInfos(DIGEST_INFO_SIGNATURE + 1);
    digestInfos[DIGEST_INFO_HAS_SIGN].resize(digestLen);
    digestInfos[DIGEST_INFO_NO_SIGN].resize(digestLen);
    digestInfos[DIGEST_INFO_SIGNATURE].resize(signatureLen);

    ret = GenerateFileDigest(stream, digestMethod, DIGEST_FLAGS_HAS_SIGN, digestInfos);
    if (memcmp(digestInfos[DIGEST_INFO_HAS_SIGN].data(), digest.buffer, digest.length) != 0) {
        PKG_LOGE("Fail to verify package %s", packagePath.c_str());
        ret = PKG_INVALID_SIGNATURE;
    }

    ClosePkgStream(stream);
    return ret;
}
} // namespace Hpackage
