/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef UI_TEST_COMMON_H
#define UI_TEST_COMMON_H

#include "engines/gfx/soft_engine.h"
#include "macros.h"

class TestGraphicEngine : public OHOS::SoftEngine {
    DISALLOW_COPY_MOVE(TestGraphicEngine);
public:
    TestGraphicEngine() = default;
    virtual ~TestGraphicEngine() = default;
    static TestGraphicEngine &GetInstance()
    {
        static TestGraphicEngine instance;
        static bool isRegister = false;
        if (!isRegister) {
            OHOS::SoftEngine::InitGfxEngine(&instance);
            isRegister = true;
        }
        return instance;
    }
    void Flush(const OHOS::Rect& flushRect) override {}
    OHOS::BufferInfo *GetFBBufferInfo() override
    {
        return nullptr;
    }
    uint16_t GetScreenWidth() override
    {
        return 1000u;
    }
    uint16_t GetScreenHeight() override
    {
        return 1000u;
    }
};

#endif