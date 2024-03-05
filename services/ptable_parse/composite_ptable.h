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

#ifndef UPDATER_COMPOSITE_PTABLE_H
#define UPDATER_COMPOSITE_PTABLE_H

#include "ptable.h"

namespace Updater {
class CompositePtable : public Ptable {
    DISALLOW_COPY_MOVE(CompositePtable);
public:
    CompositePtable() = default;
    ~CompositePtable() override {}

    bool ParsePartitionFromBuffer(uint8_t *ptbImgBuffer, const uint32_t imgBufSize) override;
    bool LoadPtableFromDevice() override;
    bool WritePartitionTable() override;
    bool EditPartitionBuf(uint8_t *imageBuf, uint64_t imgBufSize, std::vector<PtnInfo> &modifyList) override;
    bool GetPtableImageBuffer(uint8_t *imageBuf, const uint32_t imgBufSize) override;
    void AddChildPtable(std::unique_ptr<Ptable> child) override;

private:
    bool CheckBuff(const uint8_t *buf, const uint64_t size)
    {
        return (buf != nullptr) && (size > 0);
    }

    void AppendChildPtnInfo(const std::vector<PtnInfo> &ptnInfo);
    std::vector<std::unique_ptr<Ptable>> childs_ {};
};
} // namespace Updater
#endif // UPDATER_COMPOSITE_PTABLE_H
