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

#include "ptable_manager.h"

#include "log/log.h"
#include "securec.h"

namespace Updater {
// class PtableManager
PtableManager::PtableManager() : pPtable_(nullptr)
{
    InitPtablePtr();
}

PtableManager::StorageType PtableManager::GetDeviceStorageType() const
{
    return storage_;
}

void PtableManager::SetDeviceStorageType()
{
    if (storage_ != StorageType::STORAGE_UNKNOWN) {
        return;
    }
    if (IsUfsDevice()) {
        storage_ = StorageType::STORAGE_UFS;
        LOG(INFO) << "is UFS DEVICE";
    } else {
        storage_ = StorageType::STORAGE_EMMC;
        LOG(INFO) << "is EMMC DEVICE";
    }
}

bool PtableManager::IsUfsDevice()
{
    return true;
}

void PtableManager::ReloadDevicePartition(Hpackage::PkgManager *pkgManager)
{
    return LoadPartitionInfo(pkgManager);
}

void PtableManager::InitPtablePtr()
{
    SetDeviceStorageType();
    if (pPtable_ == nullptr) {
        if (GetDeviceStorageType() == StorageType::STORAGE_UFS) {
            pPtable_ = std::make_unique<UfsPtable>();
        } else {
            pPtable_ = nullptr;
        }
    }
}

bool PtableManager::InitPtableManager()
{
    if (pPtable_ == nullptr) {
        LOG(ERROR) << "pPtable_ is nullptr";
        return false;
    }
    if (!pPtable_->InitPtable()) {
        LOG(ERROR) << "init ptable error";
        return false;
    }
    return true;
}

int32_t PtableManager::GetPartitionInfoIndexByName(const std::vector<Ptable::PtnInfo> &ptnInfo,
    const std::string &name)
{
    if (ptnInfo.empty() || name.size() == 0) {
        LOG(ERROR) << "invalid input: ptnInfo is empty or name is null";
        return -1;
    }

    for (size_t i = 0; i < ptnInfo.size(); i++) {
        if (ptnInfo[i].dispName == name) {
            return i;
        }
    }
    return -1;
}

bool PtableManager::IsPartitionChanged(const std::vector<Ptable::PtnInfo> &devicePtnInfo,
    const std::vector<Ptable::PtnInfo> &pkgPtnInfo, const std::string &partitionName)
{
    if (pkgPtnInfo.empty()) {
        LOG(INFO) << "No ptable in package. Ptable no changed!";
        return false;
    }
    if (devicePtnInfo.empty()) {
        LOG(WARNING) << "ptable sizes in device and package are different, partition is changed";
        return true;
    }
    int32_t deviceIndex = GetPartitionInfoIndexByName(devicePtnInfo, partitionName);
    if (deviceIndex < 0) {
        LOG(ERROR) << "can't find the " << partitionName << " partition in device ptable!";
        return true;
    }
    int32_t updateIndex = GetPartitionInfoIndexByName(pkgPtnInfo, partitionName);
    if (updateIndex < 0) {
        LOG(ERROR) << "can't find the " << partitionName << " partition in package ptable!";
        return true;
    }
    bool ret = false;
    if (devicePtnInfo[deviceIndex].startAddr != pkgPtnInfo[updateIndex].startAddr) {
        LOG(INFO) << partitionName << " start address is changed:";
        LOG(INFO) << "[" << partitionName << "]: device ptable[" << deviceIndex << "] startAddr = 0x" <<
            devicePtnInfo[deviceIndex].startAddr << ", in package ptable[" << updateIndex << "] startAddr is 0x" <<
            pkgPtnInfo[updateIndex].startAddr;
        ret = true;
    }
    if (devicePtnInfo[deviceIndex].partitionSize != pkgPtnInfo[updateIndex].partitionSize) {
        LOG(INFO) << partitionName << " partition size is changed:";
        LOG(INFO) << "[" << partitionName << "]: device ptable[" << deviceIndex << "] partitionSize = 0x" <<
            devicePtnInfo[deviceIndex].partitionSize << ", in package ptable[" << updateIndex <<
            "] partitionSize is 0x" << pkgPtnInfo[updateIndex].partitionSize;
        ret = true;
    }
    return ret;
}

bool PtableManager::IsPtableChanged(const std::vector<Ptable::PtnInfo> &devicePtnInfo,
    const std::vector<Ptable::PtnInfo> &pkgPtnInfo)
{
    if (pkgPtnInfo.empty()) {
        LOG(INFO) << "No ptable in package. Ptable no changed!";
        return false;
    }
    if (devicePtnInfo.empty() || pkgPtnInfo.size() != devicePtnInfo.size()) {
        LOG(WARNING) << "ptable sizes in device and package are different, ptable is changed";
        return true;
    }
    for (size_t i = 0; i < pkgPtnInfo.size(); i++) {
        if (devicePtnInfo[i].dispName != pkgPtnInfo[i].dispName) {
            LOG(WARNING) << "module_name in ptable is different:";
            LOG(WARNING) << "ptable NAME in device is " << devicePtnInfo[i].dispName <<
                ", in package is " << pkgPtnInfo[i].dispName;
            return true;
        }
        if (devicePtnInfo[i].startAddr != pkgPtnInfo[i].startAddr) {
            LOG(WARNING) << pkgPtnInfo[i].dispName << " start address is different:";
            LOG(WARNING) << "Device ptable [" << devicePtnInfo[i].dispName << "] startAddr is 0x" <<
                devicePtnInfo[i].startAddr;
            LOG(WARNING) << "Package ptable [" << pkgPtnInfo[i].dispName << "] startAddr is 0x" <<
                pkgPtnInfo[i].startAddr;
            return true;
        }
        if (devicePtnInfo[i].partitionSize != pkgPtnInfo[i].partitionSize) {
            LOG(WARNING) << pkgPtnInfo[i].dispName << " partition size is different:";
            LOG(WARNING) << "Device ptable [" << devicePtnInfo[i].dispName << "] partitionSize is 0x" <<
                devicePtnInfo[i].partitionSize;
            LOG(WARNING) << "Package ptable [" << pkgPtnInfo[i].dispName << "] partitionSize is 0x" <<
                pkgPtnInfo[i].partitionSize;
            return true;
        }
    }
    return false;
}

bool PtableManager::WritePtableToDevice()
{
    if (pPtable_ == nullptr) {
        LOG(ERROR) << "Write ptable to device failed! pPtable_ is nullptr";
        return false;
    }
    if (!pPtable_->WritePartitionTable()) {
        LOG(ERROR) << "Write ptable to device failed! Please load ptable first!";
        return false;
    }
    LOG(INFO) << "Write ptable to device success!";
    return true;
}

void PtableManager::PrintPtableInfo()
{
    if (pPtable_ != nullptr) {
        LOG(ERROR) << "print partition info:";
        pPtable_->PrintPtableInfo();
        return;
    }

    LOG(INFO) << "print partition info failed!";
    return;
}

bool PtableManager::GetPartionInfoByName(const std::string &partitionName, Ptable::PtnInfo &ptnInfo, int32_t &index)
{
    if (pPtable_ == nullptr) {
        LOG(ERROR) << "GetPartionInfoByName failed! pPtable_ is nullptr";
        return false;
    }
    std::string standardPtnName = partitionName;
    standardPtnName.erase(std::remove(standardPtnName.begin(), standardPtnName.end(), '/'), standardPtnName.end());
    if (pPtable_->GetPartionInfoByName(standardPtnName, ptnInfo, index)) {
        LOG(INFO) << "GetPartionInfoByName success!";
        return true;
    }
    LOG(ERROR) << "GetPartionInfoByName failed! Not found " << standardPtnName;
    return false;
}

bool PtableManager::GetPartionInfoByName(const std::string &partitionName, Ptable::PtnInfo &ptnInfo)
{
    int32_t index = -1;
    return GetPartionInfoByName(partitionName, ptnInfo, index);
}

// class PackagePtable
PackagePtable::PackagePtable() : PtableManager() {}

void PackagePtable::LoadPartitionInfo([[maybe_unused]] Hpackage::PkgManager *pkgManager)
{
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return;
    }
    if (!InitPtableManager()) {
        LOG(ERROR) << "init ptable manager error";
        return;
    }

    uint32_t imgBufSize = pPtable_->GetDefaultImageSize();
    if (imgBufSize <= 0) {
        LOG(ERROR) << "Invalid imgBufSize";
        return;
    }
    uint8_t *imageBuf = new(std::nothrow) uint8_t[imgBufSize]();

    if (imageBuf == nullptr) {
        LOG(ERROR) << "new ptable_buffer error";
        return;
    }
    if (!GetPtableBufferFromPkg(pkgManager, imageBuf, imgBufSize)) {
        LOG(ERROR) << "get ptable buffer failed";
        delete [] imageBuf;
        return;
    }

    if (!pPtable_->ParsePartitionFromBuffer(imageBuf, imgBufSize)) {
        LOG(ERROR) << "get ptable from ptable image buffer failed";
        delete [] imageBuf;
        return;
    }
    delete [] imageBuf;
    LOG(INFO) << "print package partition info:";
    pPtable_->PrintPtableInfo();
    return;
}

bool PackagePtable::GetPtableBufferFromPkg(Hpackage::PkgManager *pkgManager, uint8_t *&imageBuf, uint32_t size)
{
    if (pkgManager == nullptr) {
        LOG(ERROR) << "pkgManager is nullptr";
        return false;
    }

    std::string ptableName = "/ptable";
    const Hpackage::FileInfo *info = pkgManager->GetFileInfo(ptableName);
    if (info == nullptr) {
        LOG(ERROR) << "Can not get file info " << ptableName;
        return false;
    }
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    (void)pkgManager->CreatePkgStream(outStream, ptableName, info->unpackedSize,
        Hpackage::PkgStream::PkgStreamType_MemoryMap);
    if (outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        return false;
    }

    if (pkgManager->ExtractFile(ptableName, outStream) != Hpackage::PKG_SUCCESS) {
        LOG(ERROR) << "Error to extract ptable";
        pkgManager->ClosePkgStream(outStream);
        return false;
    }

    size_t bufSize = 0;
    uint8_t* buffer = nullptr;
    outStream->GetBuffer(buffer, bufSize);
    if (memcpy_s(imageBuf, size, buffer, std::min(static_cast<size_t>(size), bufSize))) {
        LOG(ERROR) << "memcpy to imageBuf fail";
        pkgManager->ClosePkgStream(outStream);
        return false;
    }
    pkgManager->ClosePkgStream(outStream);
    return true;
}

// class DevicePtable
DevicePtable::DevicePtable() : PtableManager() {}

void DevicePtable::LoadPartitionInfo([[maybe_unused]] Hpackage::PkgManager *pkgManager)
{
    (void)pkgManager;
    if (!InitPtableManager()) {
        LOG(ERROR) << "init ptable manager error";
        return;
    }
    if (!pPtable_->LoadPtableFromDevice()) {
        LOG(ERROR) << "load device parititon to ram fail";
        return;
    }

    LOG(INFO) << "print device partition info:";
    pPtable_->PrintPtableInfo();
    return;
}

bool DevicePtable::ComparePartition(PtableManager &newPtbManager, const std::string partitionName)
{
    if (pPtable_ == nullptr || newPtbManager.pPtable_ == nullptr) {
        LOG(ERROR) << "input pPtable point is nullptr, compare failed!";
        return false;
    }
    if (IsPartitionChanged(pPtable_->GetPtablePartitionInfo(),
        newPtbManager.pPtable_->GetPtablePartitionInfo(), partitionName)) {
        LOG(INFO) << partitionName << " are different";
        return true;
    }
    LOG(INFO) << partitionName << " are the same";
    return false;
}

bool DevicePtable::ComparePtable(PtableManager &newPtbManager)
{
    if (pPtable_ == nullptr || newPtbManager.pPtable_ == nullptr) {
        LOG(ERROR) << "input pPtable point is nullptr, compare failed!";
        return false;
    }
    if (IsPtableChanged(pPtable_->GetPtablePartitionInfo(),
        newPtbManager.pPtable_->GetPtablePartitionInfo())) {
        LOG(INFO) << "two ptables are different";
        return true;
    }
    LOG(INFO) << "two ptables are the same";
    return false;
}
} // namespace Updater
