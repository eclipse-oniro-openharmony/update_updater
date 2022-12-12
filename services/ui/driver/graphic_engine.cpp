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
#include "graphic_engine.h"
#include "common/graphic_startup.h"
#include "common/image_decode_ability.h"
#include "common/task_manager.h"
#include "draw/draw_utils.h"
#include "font/ui_font_header.h"
#include "log/log.h"
#include "updater_ui_const.h"
#include "utils.h"

namespace Updater {
GraphicEngine &GraphicEngine::GetInstance()
{
    static GraphicEngine instance;
    static bool isRegister = false;
    if (!isRegister) {
        OHOS::SoftEngine::InitGfxEngine(&instance);
        isRegister = true;
    }

    return instance;
}

void GraphicEngine::Init(uint32_t bkgColor, uint8_t mode, const char *fontPath)
{
    bkgColor_ = bkgColor;
    colorMode_ = mode;
    [[maybe_unused]] static bool initOnce = [this, fontPath] () {
        sfDev_ = std::make_unique<SurfaceDev>();
        if (!sfDev_->Init()) {
            LOG(INFO) << "GraphicEngine Init failed!";
            return false;
        }
        sfDev_->GetScreenSize(width_, height_);
        buffInfo_ = nullptr;
        virAddr_ = nullptr;
        InitFontEngine(fontPath);
        InitImageDecodeAbility();
        InitFlushThread();
        LOG(INFO) << "GraphicEngine Init width: " << width_ << ", height: " << height_ << ", bkgColor: " << bkgColor_;
        return true;
    } ();
}

void GraphicEngine::InitFontEngine(const char *fontPath) const
{
    constexpr uint32_t uiFontMemAlignment = 4;
    static uint32_t fontMemBaseAddr[OHOS::MIN_FONT_PSRAM_LENGTH / uiFontMemAlignment];
    OHOS::GraphicStartUp::InitFontEngine(reinterpret_cast<uintptr_t>(fontMemBaseAddr), OHOS::MIN_FONT_PSRAM_LENGTH,
        fontPath, DEFAULT_FONT_FILENAME);
    LOG(INFO) << "InitFontEngine VECTOR_FONT_DIR: " << fontPath << DEFAULT_FONT_FILENAME;
}

void GraphicEngine::InitImageDecodeAbility() const
{
    uint32_t imageType = OHOS::IMG_SUPPORT_BITMAP | OHOS::IMG_SUPPORT_JPEG | OHOS::IMG_SUPPORT_PNG;
    OHOS::ImageDecodeAbility::GetInstance().SetImageDecodeAbility(imageType);
}

void GraphicEngine::InitFlushThread()
{
    flushStop_ = false;
    flushLoop_ = std::thread(&GraphicEngine::FlushThreadLoop, this);
    flushLoop_.detach();
    LOG(INFO) << "init flush thread";
}

void GraphicEngine::FlushThreadLoop() const
{
    while (!flushStop_) {
        OHOS::TaskManager::GetInstance()->TaskHandler();
        Utils::UsSleep(THREAD_USLEEP_TIME);
    }
}

OHOS::BufferInfo *GraphicEngine::GetFBBufferInfo()
{
    if (buffInfo_ != nullptr) {
        return buffInfo_.get();
    }

    uint8_t pixelBytes = OHOS::DrawUtils::GetByteSizeByColorMode(colorMode_);
    if (pixelBytes == 0) {
        LOG(ERROR) << "GraphicEngine get pixelBytes fail";
        return nullptr;
    }

    if ((width_ == 0) || (height_ == 0)) {
        LOG(ERROR) << "input error, width: " << width_ << ", height: " << height_;
        return nullptr;
    }

    virAddr_ = std::make_unique<uint8_t[]>(width_ * height_ * pixelBytes);
    buffInfo_ = std::make_unique<OHOS::BufferInfo>();
    buffInfo_->rect = { 0, 0, static_cast<int16_t>(width_ - 1), static_cast<int16_t>(height_ - 1) };
    buffInfo_->mode = static_cast<OHOS::ColorMode>(colorMode_);
    buffInfo_->color = bkgColor_;
    buffInfo_->virAddr = virAddr_.get();
    buffInfo_->phyAddr = buffInfo_->virAddr;
    buffInfo_->stride = static_cast<uint32_t>(width_ * pixelBytes);
    buffInfo_->width = width_;
    buffInfo_->height = height_;

    return buffInfo_.get();
}

void GraphicEngine::Flush(const OHOS::Rect& flushRect)
{
    if ((sfDev_ == nullptr) || (buffInfo_ == nullptr)) {
        LOG(ERROR) << "null error";
        return;
    }

    sfDev_->Flip(reinterpret_cast<uint8_t *>(buffInfo_->virAddr));
}

uint16_t GraphicEngine::GetScreenWidth()
{
    return width_;
}

uint16_t GraphicEngine::GetScreenHeight()
{
    return height_;
}
} // namespace Updater
