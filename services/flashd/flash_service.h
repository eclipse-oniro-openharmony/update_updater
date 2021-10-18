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

#ifndef FLASHING_H
#define FLASHING_H
#include <cstdlib>
#include <memory>
#include <fcntl.h>
#include <vector>
#include "blockdevice.h"
#include "partition.h"
#include "updater/updater.h"

// Just update-mode use
namespace flashd {
class FlashService {
public:
    FlashService(std::string &errorMsg, ProgressFunction progressor)
        : errorMsg_(errorMsg), progressor_(progressor) {}
    explicit FlashService(std::string &errorMsg) : errorMsg_(errorMsg), progressor_(nullptr) {}
    ~FlashService();

    int DoUpdate(const std::string &packageName);
    int DoFlashPartition(const std::string &fileName, const std::string &partition);
    int DoErasePartition(const std::string &partition);
    int DoFormatPartition(const std::string &partition, const std::string &fsType);
    int GetPartitionPath(const std::string &partition, std::string &paratitionPath);
    int DoResizeParatiton(const std::string &partition, uint32_t blocks);

    int LoadSysDevice();
    PartitionPtr GetPartition(const std::string &partition) const;

    void PostProgress(uint32_t type, size_t dataLen, const void *context) const;
    void RecordMsg(uint8_t level, const char *msg, ...);
    uint8_t GetErrorLevel() const
    {
        return errorLevel_;
    }

    static std::string ReadSysInfo(const std::string &path, const std::string &type, std::vector<std::string> &table);
    static std::string GetParamFromTable(const std::vector<std::string> &table, const std::string &param);
    static int ExecCommand(const std::vector<std::string> &cmds);

    static const std::string GetBaseName(const std::string &path);
    static const std::string GetPathRoot(const std::string &path);
    static const std::string GetRealPath(const std::string &path);
    static const std::string GetPartNameByAlias(const std::string &alias);
    static bool CheckFreeSpace(const std::string &root, uint32_t blocks);
private:
    int LoadBlockDevice(const std::string &fileDir);
    int LoadPartition(std::vector<std::string> &partitionsNames);

    int AddNewBlockDevice(DeviceType type, const std::string &devPath);
    int AddNewPartition(const std::string &path, BlockDevicePtr device);

    int CheckOperationPermission(int type, const std::string &partition) const;
    std::string &errorMsg_;
    ProgressFunction progressor_;
    uint8_t errorLevel_ = 0;
    bool loadSysDevice_ = false;
    std::vector<BlockDevicePtr> blockDevices_;
    std::vector<PartitionPtr> partitions_;
};
}
#endif // FLASHING_H