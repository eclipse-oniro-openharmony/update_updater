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
#include "font/ui_font_cache_manager.h"
#include "font/ui_font_header.h"
#include "log/log.h"
#include "updater_ui_const.h"
#include "ui_rotation.h"
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

__attribute__((weak)) void PostInitSurfDev(std::unique_ptr<SurfaceDev> &surfDev, GrSurface &surface)
{
    LOG(INFO) << "not inited the post InitSurfDev process";
    return;
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
        GrSurface surface {};
        sfDev_->GetScreenSize(width_, height_, surface);
        PostInitSurfDev(sfDev_, surface);
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
    constexpr uint32_t fontPsramSize = OHOS::MIN_FONT_PSRAM_LENGTH * 2; // 2 : alloc more ram to optimize perfomance
    constexpr uint32_t fontCacheSize = 0xC8000; // fontCacheSize should match with psram length
    static uint32_t fontMemBaseAddr[fontPsramSize / uiFontMemAlignment];
    static uint8_t icuMemBaseAddr[OHOS::SHAPING_WORD_DICT_LENGTH];
    OHOS::UIFontCacheManager::GetInstance()->SetBitmapCacheSize(fontCacheSize);
    OHOS::GraphicStartUp::InitFontEngine(reinterpret_cast<uintptr_t>(fontMemBaseAddr), fontPsramSize,
        fontPath, DEFAULT_FONT_FILENAME);
    OHOS::GraphicStartUp::InitLineBreakEngine(reinterpret_cast<uintptr_t>(icuMemBaseAddr),
        OHOS::SHAPING_WORD_DICT_LENGTH, fontPath, DEFAULT_LINE_BREAK_RULE_FILENAME);
    LOG(INFO) << "fontPath = " << fontPath << ", InitFontEngine DEFAULT_FONT_FILENAME = " << DEFAULT_FONT_FILENAME <<
        ", InitLineBreakEngine DEFAULT_LINE_BREAK_RULE_FILENAME = " << DEFAULT_LINE_BREAK_RULE_FILENAME;
}

void GraphicEngine::InitImageDecodeAbility() const
{
    uint32_t imageType = OHOS::IMG_SUPPORT_BITMAP | OHOS::IMG_SUPPORT_JPEG | OHOS::IMG_SUPPORT_PNG;
    OHOS::ImageDecodeAbility::GetInstance().SetImageDecodeAbility(imageType);
}

void GraphicEngine::InitFlushThread()
{
    flushStop_ = false;
    flushLoop_ = std::thread([this] {
        this->FlushThreadLoop();
    });
    flushLoop_.detach();
    LOG(INFO) << "init flush thread";
}

__attribute__((weak)) void InitFlushBatteryStatusExt(void)
{
}

__attribute__((weak)) void SetBrightness(int value)
{
    LOG(INFO) << "not set backlight";
}

void GraphicEngine::FlushThreadLoop() const
{
    while (!flushStop_) {
        OHOS::TaskManager::GetInstance()->TaskHandler();
        InitFlushBatteryStatusExt();
        Utils::UsSleep(sleepTime_);
    }
    // clear screen after stop
    LOG(INFO) << "disable clear stop stopClear_ = " << stopClear_;
    if (stopClear_) {
        UiRotation::GetInstance().SetDegree(UI_ROTATION_DEGREE::UI_ROTATION_0);
        uint8_t pixelBytes = OHOS::DrawUtils::GetByteSizeByColorMode(colorMode_);
        uint32_t picSize = 0;
        if (__builtin_mul_overflow(width_ * height_, pixelBytes, &picSize)) {
            return;
        }
        (void)memset_s(buffInfo_->virAddr, picSize, 0, picSize);
        sfDev_->Flip(reinterpret_cast<uint8_t *>(buffInfo_->virAddr));
    }
}

void GraphicEngine::DisableClearStop(void)
{
    stopClear_ = false;
}

void GraphicEngine::StopEngine(void)
{
    SetBrightness(0);
    flushStop_ = true;
    Utils::UsSleep(THREAD_USLEEP_TIME * 10); // 10: wait for stop 100ms
}

void GraphicEngine::SetSleepTime(uint32_t sleepTime)
{
    sleepTime_ = sleepTime;
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
    UiRotation::GetInstance().InitRotation(width_, height_, pixelBytes);
    width_ = UiRotation::GetInstance().GetWidth();
    height_ = UiRotation::GetInstance().GetHeight();
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
    std::lock_guard<std::mutex> lock {mtx_};
    UiRotation::GetInstance().SetFlushRange(flushRect);
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
