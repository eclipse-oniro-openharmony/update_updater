/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "composite_ptable.h"
#include "log/log.h"

namespace Updater {
bool CompositePtable::ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize)
{
    if (!CheckBuff<uint32_t>(ptbImgBuffer, imgBufSize)) {
        LOG(ERROR) << "input invalid";
        return false;
    }
    std::vector<PtnInfo>().swap(partitionInfo_);
    for (const auto &iter : childs_) {
        if (!iter->ParsePartitionFromBuffer(ptbImgBuffer, imgBufSize)) {
            LOG(ERROR) << "parse partition from buffer failed";
            return false;
        }
        Append(iter->GetPtablePartitionInfo());
    }
    return true;
}

bool CompositePtable::LoadPtableFromDevice()
{
    std::vector<PtnInfo>().swap(partitionInfo_);
    for (const auto &iter : childs_) {
        if (!iter->LoadPtableFromDevice()) {
            LOG(ERROR) << "load ptable from device failed";
            return false;
        }
        Append(iter->GetPtablePartitionInfo());
    }
    return true;
}

bool CompositePtable::WritePartitionTable()
{
    for (const auto &iter : childs_) {
        if (!iter->WritePartitionTable()) {
            LOG(ERROR) << "write ptable failed";
            return false;
        }
    }
    return true;
}

bool CompositePtable::GetPtableImageBuffer(uint8_t *imageBuf, const uint32_t imgBufSize)
{
    if (!CheckBuff<uint32_t>(imageBuf, imgBufSize)) {
        LOG(ERROR) << "input invalid";
        return false;
    }
    for (const auto &iter : childs_) {
        if (!iter->GetPtableImageBuffer(imageBuf, imgBufSize)) {
            LOG(ERROR) << "get ptable image buffer failed";
            return false;
        }
    }
    return true;
}

bool CompositePtable::EditPartitionBuf(uint8_t *imageBuf, uint64_t imgBufSize, std::vector<PtnInfo> &modifyList)
{
    if (!CheckBuff<uint64_t>(imageBuf, imgBufSize)) {
        LOG(ERROR) << "input invalid";
        return false;
    }
    for (const auto &iter : childs_) {
        if (!iter->EditPartitionBuf(imageBuf, imgBufSize, modifyList)) {
            LOG(ERROR) << "get ptable image buffer failed";
            return false;
        }
    }
    return true;
}

void CompositePtable::AddChildPtable(std::unique_ptr<Ptable> child)
{
    if (child == nullptr) {
        LOG(ERROR) << "input is null";
        return;
    }
    if (!(child->InitPtable())) {
        LOG(ERROR) << "init child ptable failed";
        return;
    }
    childs_.emplace_back(std::move(child));
}

void CompositePtable::AppendChildPtnInfo(const std::vector<PtnInfo> &ptnInfo)
{
    partitionInfo_.insert(partitionInfo_.end(), ptnInfo.begin(), ptnInfo.end());
}
}
