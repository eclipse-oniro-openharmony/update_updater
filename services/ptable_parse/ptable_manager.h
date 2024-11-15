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

#ifndef UPDATER_PTABLE_MANAGER_H
#define UPDATER_PTABLE_MANAGER_H

#include "package/pkg_manager.h"
#include "ufs_ptable.h"
#include "emmc_ptable.h"

namespace Updater {
class PtableManager {
public:
    DISALLOW_COPY_MOVE(PtableManager);
    virtual ~PtableManager() {}

    using PtableConstructor = std::unique_ptr<Ptable> (*)();
    enum class StorageType {
        STORAGE_UNKNOWN,
        STORAGE_EMMC,
        STORAGE_UFS,
    };

    virtual void LoadPartitionInfo([[maybe_unused]] Hpackage::PkgManager *pkgManager = nullptr) = 0;
    void ReloadDevicePartition(Hpackage::PkgManager *pkgManager);
    bool WritePtableToDevice();
    void PrintPtableInfo();
    bool GetPartionInfoByName(const std::string &partitionName, Ptable::PtnInfo &ptnInfo, int32_t &index);
    bool GetPartionInfoByName(const std::string &partitionName, Ptable::PtnInfo &ptnInfo);
    static void RegisterPtable(uint32_t bitIndex, PtableConstructor constructor);
#ifdef UPDATER_UT
    void SetPtable(std::unique_ptr<Ptable> &&ptable)
    {
        pPtable_ = std::move(ptable);
    }
#endif

    std::unique_ptr<Ptable> pPtable_;
    StorageType storage_ = StorageType::STORAGE_UNKNOWN;
    static std::string ptbImgTag_;

protected:
    PtableManager();
    void InitPtablePtr();
    bool InitPtableManager();
    void SetDeviceStorageType();
    bool IsUfsDevice();
    bool IsPartitionChanged(const std::vector<Ptable::PtnInfo> &devicePtnInfo,
        const std::vector<Ptable::PtnInfo> &pkgPtnInfo, const std::string &partitionName);
    bool IsPtableChanged(const std::vector<Ptable::PtnInfo> &devicePtnInfo,
        const std::vector<Ptable::PtnInfo> &pkgPtnInfo);
    int32_t GetPartitionInfoIndexByName(const std::vector<Ptable::PtnInfo> &ptnInfo, const std::string &name);

    StorageType GetDeviceStorageType();

private:
    bool IsCompositePtable();
    uint32_t GetBootdevType();
    void InitCompositePtable();

    static inline std::unordered_map<uint32_t, PtableConstructor> ptableMap_;
};


class PackagePtable : public PtableManager {
public:
    DISALLOW_COPY_MOVE(PackagePtable);
    ~PackagePtable() override  {}
    static PackagePtable& GetInstance()
    {
        static PackagePtable instance;
        return instance;
    }

    void LoadPartitionInfo([[maybe_unused]] Hpackage::PkgManager *pkgManager = nullptr) override;

protected:
    PackagePtable();
    bool GetPtableBufferFromPkg(Hpackage::PkgManager *pkgManager, uint8_t *&imageBuf, uint32_t size);
};


class DevicePtable : public PtableManager {
public:
    DISALLOW_COPY_MOVE(DevicePtable);
    ~DevicePtable() override {}
    static DevicePtable& GetInstance()
    {
        static DevicePtable instance;
        return instance;
    }

    void LoadPartitionInfo([[maybe_unused]] Hpackage::PkgManager *pkgManager = nullptr) override;
    bool ComparePtable(PtableManager &newPtbManager);
    bool ComparePartition(PtableManager &newPtbManager, const std::string partitionName);
protected:
    DevicePtable();
};
} // namespace Updater
#endif // UPDATER_PTABLE_MANAGER_H