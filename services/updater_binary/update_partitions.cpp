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
#include "update_partitions.h"
#include <cerrno>
#include <cstdio>
#include <sstream>
#include <string>
#include "log/log.h"
#include "updater/updater_const.h"
#include "utils.h"

using namespace std;
using namespace Uscript;
using namespace Hpackage;
using namespace Updater;
constexpr int MIN_PARTITIONS_NUM = 2;
constexpr int MAX_PARTITIONS_NUM = 20;
namespace Updater {
bool UpdatePartitions::SetPartitionInfo(const cJSON *partitions, int idx, struct Partition *myPartition) const
{
    cJSON *thisPartition = cJSON_GetArrayItem(partitions, idx);
    if (thisPartition == nullptr) {
        LOG(ERROR) << "Error get thisPartion: " << idx;
        return false;
    }
    cJSON *item = cJSON_GetObjectItem(thisPartition, "start");
    if (item == nullptr) {
        LOG(ERROR) << "Error get start";
        return false;
    }
    myPartition->start = static_cast<size_t>(item->valueint);

    item = cJSON_GetObjectItem(thisPartition, "length");
    if (item == nullptr) {
        LOG(ERROR) << "Error get length";
        return false;
    }
    myPartition->length = static_cast<size_t>(item->valueint);
    myPartition->partNum = 0;
    myPartition->devName = "mmcblk0px";

    item = cJSON_GetObjectItem(thisPartition, "partName");
    if (item == nullptr) {
        LOG(ERROR) << "Error get partName";
        return false;
    }
    myPartition->partName = (item->valuestring);

    item = cJSON_GetObjectItem(thisPartition, "fsType");
    if (item == nullptr) {
        LOG(ERROR) << "Error get fsType";
        return false;
    }
    myPartition->fsType = (item->valuestring);

    LOG(INFO) << "<start> <length> <devname> <partname> <fstype>";
    LOG(INFO) << myPartition->start << " " << myPartition->length << " " << myPartition->devName << " " <<
        myPartition->partName << " " << myPartition->fsType;
    return true;
}

int UpdatePartitions::ParsePartitionInfo(const std::string &partitionInfo, PartitonList &newPartList) const
{
    cJSON* root = cJSON_Parse(partitionInfo.c_str());
    if (root == nullptr) {
        LOG(ERROR) << "Error get root";
        return -1;
    }
    cJSON* partitions = cJSON_GetObjectItem(root, "Partition");
    if (partitions == nullptr) {
        LOG(ERROR) << "Error get Partitions";
        cJSON_Delete(root);
        return -1;
    }
    int number = cJSON_GetArraySize(partitions);
    if (number <= MIN_PARTITIONS_NUM || number >= MAX_PARTITIONS_NUM) {
        LOG(ERROR) << "Error partitions number: " << number;
        cJSON_Delete(root);
        return -1;
    }
    LOG(INFO) << "Partitions numbers " << number;

    for (int i = 0; i < number; i++) {
        struct Partition* myPartition = static_cast<struct Partition*>(calloc(1, sizeof(struct Partition)));
        if (!myPartition) {
            LOG(ERROR) << "Allocate memory for partition failed: " << errno;
            cJSON_Delete(root);
            return 0;
        }
        if (!SetPartitionInfo(partitions, i, myPartition)) {
            free(myPartition);
            myPartition = nullptr;
            break;
        }
        newPartList.push_back(myPartition);
    }
    cJSON_Delete(root);
    return 1;
}

int UpdatePartitions::DoNewPartitions(PartitonList &newPartList)
{
    int ret = DoPartitions(newPartList);
    newPartList.clear();
    if (ret <= 0) {
        LOG(INFO) << "do_partitions FAIL ";
    } else if (ret == 1) {
        LOG(INFO) << "partitions not changed,Skip.";
    } else if (ret > 1) {
        LOG(INFO) << "do_partitions success reboot";
#ifndef UPDATER_UT
        Utils::DoReboot("updater");
#endif
    }
    return ret;
}

int UpdatePartitions::SetNewPartition(const std::string &filePath, const FileInfo *info, Uscript::UScriptEnv &env)
{
    UPDATER_INIT_RECORD;
    std::string tmpPath = "/data/updater" + filePath;
    char realPath[PATH_MAX + 1] = {};
    if (realpath(tmpPath.c_str(), realPath) == nullptr) {
        LOG(ERROR) << "Error to create: " << tmpPath;
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    int ret = env.GetPkgManager()->CreatePkgStream(outStream,
        std::string(realPath), info->unpackedSize, PkgStream::PkgStreamType_Write);
    if (ret != USCRIPT_SUCCESS || outStream == nullptr) {
        LOG(ERROR) << "Error to create output stream";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    ret = env.GetPkgManager()->ExtractFile(filePath, outStream);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Error to extract file";
        env.GetPkgManager()->ClosePkgStream(outStream);
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    FILE *fp = fopen(realPath, "rb");
    if (!fp) {
        LOG(ERROR) << "Open " << tmpPath << " failed: " << errno;
        env.GetPkgManager()->ClosePkgStream(outStream);
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    char partitionInfo[MAX_LOG_BUF_SIZE];
    size_t partitionCount = fread(partitionInfo, 1, MAX_LOG_BUF_SIZE, fp);
    fclose(fp);
    if (partitionCount <= LEAST_PARTITION_COUNT) {
        env.GetPkgManager()->ClosePkgStream(outStream);
        LOG(ERROR) << "Invalid partition size, too small";
        UPDATER_LAST_WORD(USCRIPT_ERROR_EXECUTE);
        return USCRIPT_ERROR_EXECUTE;
    }
    PartitonList newPartList {};
    if (ParsePartitionInfo(std::string(partitionInfo), newPartList) == 0) {
        env.GetPkgManager()->ClosePkgStream(outStream);
        UPDATER_LAST_WORD(USCRIPT_ABOART);
        return USCRIPT_ABOART;
    }
    if (newPartList.empty()) {
        LOG(ERROR) << "Partition is empty ";
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_SUCCESS; // Partitions table is empty not require partition.
    }
    DoNewPartitions(newPartList);
    env.GetPkgManager()->ClosePkgStream(outStream);
    return USCRIPT_SUCCESS;
}

int32_t UpdatePartitions::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    LOG(INFO) << "enter UpdatePartitions::Execute ";
    if (context.GetParamCount() != 1) {
        LOG(ERROR) << "Invalid UpdatePartitions::Execute param";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_INVALID_PARAM;
    }
    std::string filePath;
    int32_t ret = context.GetParam(0, filePath);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Fail to get filePath";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_INVALID_PARAM;
    } else {
        LOG(INFO) << "UpdatePartitions::Execute filePath " << filePath;
    }
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(filePath);
    if (info == nullptr) {
        LOG(ERROR) << "Error to get file info";
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_ERROR_EXECUTE;
    }
    return SetNewPartition(filePath, info, env);
}
} // namespace Updater
