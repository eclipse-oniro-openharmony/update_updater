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
#ifndef UPDATER_FS_MANAGER_MOUNT_H
#define UPDATER_FS_MANAGER_MOUNT_H

#include <string>
#include "fs_manager/fs_manager.h"
#include "updater/updater.h"

namespace Updater {
void LoadFstab(); /* Load fstab */
void LoadSpecificFstab(const std::string &fstabName);
int FormatPartition(const std::string &path, bool isZeroErase = false);
int UmountForPath(const std::string &path);
int MountForPath(const std::string &path);
int SetupPartitions(PackageUpdateMode mode = HOTA_UPDATE);
const std::string GetBlockDeviceByMountPoint(const std::string &mountPoint);
MountStatus GetMountStatusForPath(const std::string &path);
} // Updater
#endif // UPDATER_FS_MANAGER_MOUNT_H