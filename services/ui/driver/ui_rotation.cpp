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

#include "ui_rotation.h"
#include "log/log.h"
#include "securec.h"

namespace Updater {
UiRotation &UiRotation::GetInstance(void)
{
    static UiRotation uiRotation;
    return uiRotation;
}

void UiRotation::InitRotation(int realWidth, int realHeight, uint8_t pixelBytes)
{
    LOG(INFO) << "InitRotation realWidth = " << realWidth << ", realHeight = " << realHeight <<
        ", pixelBytes = " << static_cast<int>(pixelBytes);
    pixelBytes_ = pixelBytes;
    RotateWidthHeight(realWidth, realHeight);
    switch (degree_) {
        case UI_ROTATION_DEGREE::UI_ROTATION_90: // 90: input number 90
            sinR_ = 1;
            cosR_ = 0;
            offsetX_ = height_ - 1;
            offsetY_ = 0;
            oldRowBytes_ = width_ * pixelBytes_;
            newRowBytes_ = height_ * pixelBytes_;
            break;
        case UI_ROTATION_DEGREE::UI_ROTATION_180: // 180: input number 180
            sinR_ = 0;
            cosR_ = -1;
            offsetX_ = width_ - 1;
            offsetY_ = height_ - 1;
            oldRowBytes_ = width_ * pixelBytes_;
            newRowBytes_ = width_ * pixelBytes_;
            break;
        case UI_ROTATION_DEGREE::UI_ROTATION_270: // 270: input number 270
            sinR_ = -1;
            cosR_ = 0;
            offsetX_ = 0;
            offsetY_ = width_ - 1;
            oldRowBytes_ = width_ * pixelBytes_;
            newRowBytes_ = height_ * pixelBytes_;
            break;
        default:
            sinR_ = 0;
            cosR_ = 1;
            offsetX_ = 0;
            offsetY_ = 0;
            oldRowBytes_ = width_ * pixelBytes_;
            newRowBytes_ = width_ * pixelBytes_;
    }
}

void UiRotation::SetDegree(UI_ROTATION_DEGREE degree)
{
    degree_ = degree;
}

int UiRotation::GetWidth(void)
{
    return width_;
}

int UiRotation::GetHeight(void)
{
    return height_;
}

void UiRotation::RotateWidthHeight(int realWidth, int realHeight)
{
    if (degree_ == UI_ROTATION_DEGREE::UI_ROTATION_0 || degree_ == UI_ROTATION_DEGREE::UI_ROTATION_180) {
        width_ = realWidth;
        height_ = realHeight;
    } else {
        width_ = realHeight;
        height_ = realWidth;
    }
}

void UiRotation::SetFlushRange(const OHOS::Rect &rect)
{
    rect_ = rect;
}

void UiRotation::RotateBuffer(const uint8_t *origBuf, uint8_t *dstBuf, uint32_t size)
{
    if (degree_ == UI_ROTATION_DEGREE::UI_ROTATION_0) {
        if (memcpy_s(dstBuf, size, origBuf, size) != EOK) {
            LOG(ERROR) << "flip memcpy_s fail";
        }
        return;
    }

    int x {}, y {};
    const unsigned char *srcP = nullptr;
    unsigned char *dstP = nullptr;
    for (int h = rect_.GetTop(); h < rect_.GetBottom(); h++) {
        for (int w = rect_.GetLeft(); w < rect_.GetRight(); w++) {
            x = offsetX_ + w * cosR_ - h * sinR_;
            y = offsetY_ + h * cosR_ + w * sinR_;
            srcP = origBuf + h * oldRowBytes_ + w * pixelBytes_;
            dstP = dstBuf + y * newRowBytes_ + x * pixelBytes_;
            for (int j = 0; j < pixelBytes_; j++) {
                *dstP++ = *srcP++;
            }
        }
    }
}

std::pair<int,int> UiRotation::RotateXY(int x, int y)
{
    if (degree_ == UI_ROTATION_DEGREE::UI_ROTATION_0) {
        return {x, y};
    }
    return {cosR_ * (x - offsetX_) + sinR_ * (y - offsetY_), cosR_ * (y - offsetY_) - sinR_ * (x - offsetX_)};
}
}