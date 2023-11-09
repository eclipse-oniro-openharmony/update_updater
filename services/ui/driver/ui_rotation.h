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

#ifndef UI_ROTATION_H
#define UI_ROTATION_H

#include <cstdint>
#include <utility>
#include "gfx_utils/rect.h"
#include "macros.h"

namespace Updater {
enum class UI_ROTATION_DEGREE {
    UI_ROTATION_0,
    UI_ROTATION_90,
    UI_ROTATION_180,
    UI_ROTATION_270
};

class UiRotation final {
    DISALLOW_COPY_MOVE(UiRotation);
public:
    UiRotation() = default;
    ~UiRotation() = default;
    static UiRotation &GetInstance(void);
    void SetDegree(UI_ROTATION_DEGREE degree);
    void InitRotation(int realWidth, int realHeight, uint8_t pixelBytes);
    int GetWidth(void);
    int GetHeight(void);
    void RotateBuffer(const uint8_t *origBuf, uint8_t *dstBuf, uint32_t size);
    void SetFlushRange(const OHOS::Rect &rect);
    std::pair<int,int> RotateXY(int x, int y);
private:
    void RotateWidthHeight(int realWidth, int realHeight);
    int width_ {};
    int height_ {};
    uint8_t pixelBytes_ {};
    int sinR_ {};
    int cosR_ {};
    int offsetX_ {};
    int offsetY_ {};
    int oldRowBytes_ {};
    int newRowBytes_ {};
    OHOS::Rect rect_ {};
    UI_ROTATION_DEGREE degree_ {UI_ROTATION_DEGREE::UI_ROTATION_0};
};
}

#endif // UI_ROTATION_H