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

namespace Hpackage {
constexpr int32_t BUFFER_SIZE = 4096;
constexpr int32_t DIGEST_INFO_NO_SIGN = 0;
constexpr int32_t DIGEST_INFO_HAS_SIGN = 1;
constexpr int32_t DIGEST_INFO_SIGNATURE = 2;
constexpr int32_t DIGEST_FLAGS_NO_SIGN = 1;
constexpr int32_t DIGEST_FLAGS_HAS_SIGN = 2;
constexpr int32_t DIGEST_FLAGS_SIGNATURE = 4;
constexpr uint32_t VERIFY_FINSH_PERCENT = 100;

static thread_local PkgManagerImpl *g_pkgManagerInstance = nullptr;
PkgManager::PkgManagerPtr PkgManager::GetPackageInstance()
{
    if (g_pkgManagerInstance == nullptr) {
        g_pkgManagerInstance = new PkgManagerImpl();
    }
    return g_pkgManagerInstance;
}

PkgManager::PkgManagerPtr PkgManager::CreatePackageInstance()
{
    return new PkgManagerImpl();
}

void PkgManager::ReleasePackageInstance(PkgManager::PkgManagerPtr manager)
{
    if (manager == nullptr) {
        return;
    }
    if (g_pkgManagerInstance == manager) {
        delete g_pkgManagerInstance;
        g_pkgManagerInstance = nullptr;
    } else {
        delete manager;
    }
    manager = nullptr;
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

PkgFilePtr PkgManagerImpl::CreatePackage(PkgStreamPtr stream, PkgFile::PkgType type, PkgInfoPtr header)
{
    PkgFilePtr pkgFile = nullptr;
    switch (type) {
        case PkgFile::PKG_TYPE_UPGRADE:
            pkgFile = new UpgradePkgFile(this, stream, header);
            break;
        case PkgFile::PKG_TYPE_ZIP:
            pkgFile = new ZipPkgFile(this, stream);
            break;
        case PkgFile::PKG_TYPE_LZ4:
            pkgFile = new Lz4PkgFile(this, stream);
            break;
        case PkgFile::PKG_TYPE_GZIP:
            pkgFile = new GZipPkgFile(this, stream);
            break;
        default:
            return nullptr;
    }
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
        pkgFile->SetPkgStream();
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
    int32_t ret = SetSignVerifyKeyName(keyPath);
    if (ret != PKG_SUCCESS) {
        UPDATER_LAST_WORD(ret);
        PKG_LOGE("Invalid keyname");
        return ret;
    }
    // Check if package already loaded
    for (auto iter : pkgFiles_) {
        PkgFilePtr pkgFile = iter;
        if (pkgFile != nullptr && pkgFile->GetPkgStream()->GetFileName().compare(packagePath) == 0) {
            return PKG_SUCCESS;
        }
    }
    PkgFile::PkgType pkgType = GetPkgTypeByName(packagePath);
    ret = PKG_INVALID_FILE;
    unzipToFile_ = ((pkgType == PkgFile::PKG_TYPE_GZIP) ? true : unzipToFile_);
    if (pkgType == PkgFile::PKG_TYPE_UPGRADE) {
        ret = LoadPackage(packagePath, fileIds, pkgType);
        if (ret != PKG_SUCCESS) {
            ClearPkgFile();
            UPDATER_LAST_WORD(ret);
            PKG_LOGE("Parse %s fail ", packagePath.c_str());
            return ret;
        }
    } else if (pkgType != PkgFile::PKG_TYPE_NONE) {
        std::vector<std::string> innerFileNames;
        ret = LoadPackage(packagePath, innerFileNames, pkgType);
        if (ret != PKG_SUCCESS) {
            ClearPkgFile();
            PKG_LOGE("Unzip %s fail ", packagePath.c_str());
            return ret;
        }
        std::string path = GetFilePath(packagePath);
        for (auto name : innerFileNames) {
            pkgType = GetPkgTypeByName(name);
            if (pkgType == PkgFile::PKG_TYPE_NONE) {
                fileIds.push_back(name);
                continue;
            }
            ret = ExtraAndLoadPackage(path, name, pkgType, fileIds);
            PKG_CHECK(ret == PKG_SUCCESS, ClearPkgFile(); UPDATER_LAST_WORD(ret);
                return ret, "unpack %s fail in package %s ", name.c_str(), packagePath.c_str());
        }
    }
    return PKG_SUCCESS;
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
    const std::string tempPath = path.find(Updater::SDCARD_CARD_PATH) != string::npos ?
        path : (string(Updater::UPDATER_PATH) + "/");
    if (stat(tempPath.c_str(), &st) != 0) {
#ifndef __WIN32
        (void)mkdir(tempPath.c_str(), 0775); // 0775 : rwxrwxr-x
        (void)chown(tempPath.c_str(), Updater::Utils::USER_UPDATE_AUTHORITY, Updater::Utils::GROUP_UPDATE_AUTHORITY);
#endif
    }

    // Extract package to file or memory
    if (unzipToFile_) {
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
    int32_t ret = CreatePkgStream(stream, packagePath, 0, PkgStream::PkgStreamType_Read);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Create input stream fail %s", packagePath.c_str());
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    return LoadPackageWithStream(packagePath, fileIds, type, stream);
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
    // Find out pkgEntry by path.
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

int32_t PkgManagerImpl::CreatePkgStream(StreamPtr &stream, const std::string &fileName, size_t size, int32_t type)
{
    PkgStreamPtr pkgStream;
    int32_t ret = CreatePkgStream(pkgStream, fileName, size, type);
    if (pkgStream == nullptr) {
        PKG_LOGE("Failed to create stream");
        return -1;
    }
    stream = pkgStream;
    return ret;
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
    PkgStream::ExtractFileProcessor processor, const void *context)
{
    PkgStreamPtr pkgStream;
    int32_t ret = CreatePkgStream(pkgStream, fileName, processor, context);
    stream = pkgStream;
    return ret;
}

void PkgManagerImpl::ClosePkgStream(StreamPtr &stream)
{
    PkgStreamPtr pkgStream = static_cast<PkgStreamPtr>(stream);
    ClosePkgStream(pkgStream);
    stream = nullptr;
}

int32_t PkgManagerImpl::CreatePkgStream(PkgStreamPtr &stream, const std::string &fileName, size_t size, int32_t type)
{
    stream = nullptr;
    if (type == PkgStream::PkgStreamType_Write || type == PkgStream::PkgStreamType_Read) {
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
        if (CheckFile(fileName) != PKG_SUCCESS) {
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
        PKG_CHECK(file != nullptr, UPDATER_LAST_WORD(PKG_INVALID_FILE);
            return PKG_INVALID_FILE, "Fail to open file %s ", fileName.c_str());
        stream = new FileStream(this, fileName, file, type);
    } else if (type == PkgStream::PkgStreamType_MemoryMap) {
        if ((size == 0) && (access(fileName.c_str(), 0) != 0)) {
            UPDATER_LAST_WORD(PKG_INVALID_FILE);
            return PKG_INVALID_FILE;
        }
        size_t fileSize = (size == 0) ? GetFileSize(fileName) : size;
        PKG_CHECK(fileSize > 0, UPDATER_LAST_WORD(PKG_INVALID_FILE);
            return PKG_INVALID_FILE, "Fail to check file size %s ", fileName.c_str());
        uint8_t *memoryMap = MapMemory(fileName, fileSize);
        PKG_CHECK(memoryMap != nullptr, UPDATER_LAST_WORD(PKG_INVALID_FILE);
            return PKG_INVALID_FILE, "Fail to map memory %s ", fileName.c_str());
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

PkgFile::PkgType PkgManagerImpl::GetPkgTypeByName(const std::string &path)
{
    std::size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return PkgFile::PKG_TYPE_NONE;
    }
    std::string postfix = path.substr(pos + 1, -1);
    std::transform(postfix.begin(), postfix.end(), postfix.begin(), ::tolower);

    if (path.substr(pos + 1, -1).compare("bin") == 0) {
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
        PkgManager::PkgManagerPtr pkgManager = PkgManager::GetPackageInstance();
        if (pkgManager == nullptr) {
            PKG_LOGE("Fail to GetPackageInstance");
            return PKG_INVALID_SIGNATURE;
        }
        std::vector<std::string> components;
        ret = pkgManager->LoadPackage(packagePath, keyPath, components);
    }
    cb(ret, VERIFY_FINSH_PERCENT);
    if (ret != PKG_SUCCESS) {
        PKG_LOGE("Verify file %s fail", packagePath.c_str());
        return ret;
    }
    PKG_LOGI("Verify file %s success", packagePath.c_str());
    return ret;
}

int32_t PkgManagerImpl::GenerateFileDigest(PkgStreamPtr stream,
    uint8_t digestMethod, uint8_t flags, std::vector<std::vector<uint8_t>> &digestInfos, size_t hashBufferLen)
{
    size_t fileLen = (hashBufferLen == 0) ? stream->GetFileLength() : hashBufferLen;
    size_t digestLen = DigestAlgorithm::GetDigestLen(digestMethod);
    size_t signatureLen = DigestAlgorithm::GetSignatureLen(digestMethod);
    // Check entire package
    DigestAlgorithm::DigestAlgorithmPtr algorithm = PkgAlgorithmFactory::GetDigestAlgorithm(digestMethod);
    PKG_CHECK(algorithm != nullptr, return PKG_NOT_EXIST_ALGORITHM, "Invalid file %s", stream->GetFileName().c_str());
    algorithm->Init();

    // Get verify algorithm
    DigestAlgorithm::DigestAlgorithmPtr algorithmInner = PkgAlgorithmFactory::GetDigestAlgorithm(digestMethod);
    PKG_CHECK(algorithm != nullptr, return PKG_NOT_EXIST_ALGORITHM, "Invalid file %s", stream->GetFileName().c_str());
    algorithmInner->Init();

    size_t offset = 0;
    size_t readLen = 0;
    size_t needReadLen = fileLen;
    size_t buffSize = BUFFER_SIZE;
    PkgBuffer buff(buffSize);
    if (flags & DIGEST_FLAGS_SIGNATURE) {
        PKG_ONLY_CHECK(SIGN_TOTAL_LEN < fileLen, return PKG_INVALID_SIGNATURE);
        needReadLen = fileLen - SIGN_TOTAL_LEN;
    }
    while (offset < needReadLen) {
        PKG_ONLY_CHECK((needReadLen - offset) >= buffSize, buffSize = needReadLen - offset);
        int32_t ret = stream->Read(buff, offset, buffSize, readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "read buffer fail %s", stream->GetFileName().c_str());
        PKG_IS_TRUE_DONE(flags & DIGEST_FLAGS_HAS_SIGN, algorithm->Update(buff, readLen));
        PKG_IS_TRUE_DONE(flags & DIGEST_FLAGS_NO_SIGN, algorithmInner->Update(buff, readLen));
        offset += readLen;
        PostDecodeProgress(POST_TYPE_VERIFY_PKG, readLen, nullptr);
        readLen = 0;
    }

    // Read last signatureLen
    if (flags & DIGEST_FLAGS_SIGNATURE) {
        readLen = 0;
        int32_t ret = stream->Read(buff, offset, SIGN_TOTAL_LEN, readLen);
        PKG_CHECK(ret == PKG_SUCCESS, return ret, "read buffer failed %s", stream->GetFileName().c_str());
        PKG_IS_TRUE_DONE(flags & DIGEST_FLAGS_HAS_SIGN, algorithm->Update(buff, readLen));
        PkgBuffer data(SIGN_TOTAL_LEN);
        PKG_IS_TRUE_DONE(flags & DIGEST_FLAGS_NO_SIGN, algorithmInner->Update(data, SIGN_TOTAL_LEN));
    }
    PKG_IS_TRUE_DONE(flags & DIGEST_FLAGS_HAS_SIGN,
        PkgBuffer result(digestInfos[DIGEST_INFO_HAS_SIGN].data(), digestLen); algorithm->Final(result));
    PKG_IS_TRUE_DONE(flags & DIGEST_FLAGS_NO_SIGN,
        PkgBuffer result(digestInfos[DIGEST_INFO_NO_SIGN].data(), digestLen); algorithmInner->Final(result));
    if (flags & DIGEST_FLAGS_SIGNATURE) {
        if (digestMethod == PKG_DIGEST_TYPE_SHA256) {
            PKG_CHECK(!memcpy_s(digestInfos[DIGEST_INFO_SIGNATURE].data(), signatureLen, buff.buffer, signatureLen),
                return PKG_NONE_MEMORY, "GenerateFileDigest memcpy failed");
        } else {
            PKG_CHECK(!memcpy_s(digestInfos[DIGEST_INFO_SIGNATURE].data(), signatureLen, buff.buffer + SIGN_SHA256_LEN,
                signatureLen), return PKG_NONE_MEMORY, "GenerateFileDigest memcpy failed");
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
    if (access(keyName.c_str(), 0) != 0) {
        UPDATER_LAST_WORD(PKG_INVALID_FILE);
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
