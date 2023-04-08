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
#ifndef BIN_FLOW_UPDATE
#define BIN_FLOW_UPDATE

#include <cstdio>
#include <functional>
#include <string>
#include <sys/wait.h>
#include <vector>
#include <map>

#include "applypatch/data_writer.h"
#include "package/pkg_manager.h"

namespace Updater {

using BinFlowUpdateStep = enum {
    BIN_UPDATE_STEP_PRE = 0,
    BIN_UPDATE_STEP_DO,
    BIN_UPDATE_STEP_POST
};

struct BinFlowUpdateInfo {
    std::vector<std::string> componentNames;
    uint32_t curIndex = 0;
    size_t imageWriteLen = 0;
    bool needNewData = false;
    std::map<BinFlowUpdateStep, std::function<int32_t (uint8_t *, uint32_t &)>> updateBinProcess_;
    BinFlowUpdateStep updateStep = BIN_UPDATE_STEP_PRE;
    std::unique_ptr<DataWriter> writer;
    const Hpackage::FileInfo *info = nullptr;
};

class BinFlowUpdate {
public:
    explicit BinFlowUpdate(uint32_t maxBufSize);
    virtual ~BinFlowUpdate();
    int32_t StartBinFlowUpdate(uint8_t *data, uint32_t len);
private:
    int32_t BinUpdatePreWrite(uint8_t *data, uint32_t &len);
    int32_t BinUpdateDoWrite(uint8_t *data, uint32_t &len);
    int32_t BinUpdatePostWrite(uint8_t *data, uint32_t &len);
    int32_t UpdateBinData(uint8_t *data, uint32_t &len);
    std::unique_ptr<DataWriter> GetDataWriter(const std::string &partition);

    uint32_t UpdateBinHead(uint8_t *data, uint32_t &len);
    bool AddRemainData(uint8_t *data, uint32_t &len);

    Hpackage::PkgManager::PkgManagerPtr pkgManager_;
    uint8_t *buffer_ = nullptr;
    uint32_t maxBufSize_ = 0;
    uint32_t curlen_ = 0;

    // bin images
    bool headInit_ = false;
    std::vector<std::string> componentNames_;
    uint32_t procCompIndex_ = 0;
    std::map<BinFlowUpdateStep, std::function<int32_t (uint8_t *, uint32_t &)>> updateBinProcess_;
    BinFlowUpdateInfo updateInfo_ {};
};
} // Updater
#endif /* BIN_FLOW_UPDATE */
