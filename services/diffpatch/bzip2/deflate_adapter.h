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

#ifndef DEFLATE_ADAPTER_H
#define DEFLATE_ADAPTER_H
#include <iostream>
#include "diffpatch.h"

namespace UpdatePatch {
class DeflateAdapter {
public:
    static constexpr uint32_t BUFFER_SIZE = 1024 * 32;
    DeflateAdapter() = default;
    virtual ~DeflateAdapter() {}

    virtual int32_t Open()
    {
        return 0;
    };
    virtual int32_t Close()
    {
        return 0;
    };
    virtual int32_t WriteData(const BlockBuffer &srcData)
    {
        return 0;
    };
    virtual int32_t FlushData(size_t &offset)
    {
        return 0;
    };

protected:
    bool init_ = false;
};
} // namespace UpdatePatch
#endif // DEFLATE_ADAPTER_H
