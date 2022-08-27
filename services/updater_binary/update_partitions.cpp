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
#include "cJSON.h"
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
int UpdatePartitions::ParsePartitionInfo(const std::string &partitionInfo, PartitonList &newPartList) const
{
    cJSON* root = cJSON_Parse(partitionInfo.c_str());
    UPDATER_ERROR_CHECK(root != nullptr, "Error get root", return -1);
    cJSON* partitions = cJSON_GetObjectItem(root, "Partition");
    UPDATER_ERROR_CHECK(partitions != nullptr, "Error get Partitions", cJSON_Delete(root); return -1);
    int number = cJSON_GetArraySize(partitions);
    UPDATER_ERROR_CHECK(number > MIN_PARTITIONS_NUM, "Error partitions number < 3", cJSON_Delete(root); return -1);
    UPDATER_ERROR_CHECK(number < MAX_PARTITIONS_NUM, "Error partitions number > 20", cJSON_Delete(root); return -1);
    LOG(INFO) << "Partitions numbers " << number;
    int i = 0;
    for (i = 0; i < number; i++) {
        struct Partition* myPartition = static_cast<struct Partition*>(calloc(1, sizeof(struct Partition)));
        if (!myPartition) {
            LOG(ERROR) << "Allocate memory for partition failed: " << errno;
            cJSON_Delete(root);
            return 0;
        }
        cJSON* thisPartition = cJSON_GetArrayItem(partitions, i);
        UPDATER_ERROR_CHECK(thisPartition != nullptr, "Error get thisPartion", free(myPartition);
            myPartition = nullptr; break);

        cJSON* item = cJSON_GetObjectItem(thisPartition, "start");
        UPDATER_ERROR_CHECK(item != nullptr, "Error get start", free(myPartition); myPartition = nullptr; break);
        myPartition->start = static_cast<size_t>(item->valueint);

        item = cJSON_GetObjectItem(thisPartition, "length");
        UPDATER_ERROR_CHECK(item != nullptr, "Error get length", free(myPartition); myPartition = nullptr; break);
        myPartition->length = static_cast<size_t>(item->valueint);
        myPartition->partNum = 0;
        myPartition->devName = "mmcblk0px";

        item = cJSON_GetObjectItem(thisPartition, "partName");
        UPDATER_ERROR_CHECK(item != nullptr, "Error get partName", free(myPartition); myPartition = nullptr; break);
        myPartition->partName = (item->valuestring);

        item = cJSON_GetObjectItem(thisPartition, "fsType");
        UPDATER_ERROR_CHECK(item != nullptr, "Error get fsType", free(myPartition); myPartition = nullptr; break);
        myPartition->fsType = (item->valuestring);

        LOG(INFO) << "<start> <length> <devname> <partname> <fstype>";
        LOG(INFO) << myPartition->start << " " << myPartition->length << " " << myPartition->devName << " " <<
            myPartition->partName << " " << myPartition->fsType;
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

int32_t UpdatePartitions::Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context)
{
    LOG(INFO) << "enter UpdatePartitions::Execute ";
    if (context.GetParamCount() != 1) {
        LOG(ERROR) << "Invalid UpdatePartitions::Execute param";
        return USCRIPT_INVALID_PARAM;
    }
    std::string filePath;
    int32_t ret = context.GetParam(0, filePath);
    if (ret != USCRIPT_SUCCESS) {
        LOG(ERROR) << "Fail to get filePath";
        return USCRIPT_INVALID_PARAM;
    } else {
        LOG(INFO) << "UpdatePartitions::Execute filePath " << filePath;
    }
    Hpackage::PkgManager::StreamPtr outStream = nullptr;
    const FileInfo *info = env.GetPkgManager()->GetFileInfo(filePath);
    UPDATER_ERROR_CHECK(info != nullptr, "Error to get file info", return USCRIPT_ERROR_EXECUTE);
    std::string tmpPath = "/data/updater";
    std::string tmpPath1 = tmpPath + filePath;
    char realPath[PATH_MAX + 1] = {};
    UPDATER_ERROR_CHECK(realpath(tmpPath1.c_str(), realPath) != nullptr,
        "Error to create tmpPath1", return USCRIPT_ERROR_EXECUTE);
    ret = env.GetPkgManager()->CreatePkgStream(outStream,
        tmpPath1, info->unpackedSize, PkgStream::PkgStreamType_Write);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS && outStream != nullptr, "Error to create output stream",
        return USCRIPT_ERROR_EXECUTE);
    ret = env.GetPkgManager()->ExtractFile(filePath, outStream);
    UPDATER_ERROR_CHECK(ret == USCRIPT_SUCCESS, "Error to extract file",
        env.GetPkgManager()->ClosePkgStream(outStream); return USCRIPT_ERROR_EXECUTE);
    FILE *fp = fopen(realPath, "rb");
    if (!fp) {
        LOG(ERROR) << "Open " << tmpPath1 << " failed: " << errno;
        env.GetPkgManager()->ClosePkgStream(outStream);
        return USCRIPT_ERROR_EXECUTE;
    }
    char partitionInfo[MAX_LOG_BUF_SIZE];
    size_t partitionCount = fread(partitionInfo, 1, MAX_LOG_BUF_SIZE, fp);
    fclose(fp);
    if (partitionCount <= LEAST_PARTITION_COUNT) {
        env.GetPkgManager()->ClosePkgStream(outStream);
        LOG(ERROR) << "Invalid partition size, too small";
        return USCRIPT_ERROR_EXECUTE;
    }
    PartitonList newPartList {};
    if (ParsePartitionInfo(std::string(partitionInfo), newPartList) == 0) {
        env.GetPkgManager()->ClosePkgStream(outStream);
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
} // namespace Updater
