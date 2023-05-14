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
#ifndef UPDATER_UI_IMG_VIEW_ADAPTER_H
#define UPDATER_UI_IMG_VIEW_ADAPTER_H

#include <string>
#include <thread>
#include "animator/animator_manager.h"
#include "component_common.h"
#include "components/ui_image_view.h"
#include "macros.h"

namespace Updater {
struct UxImageInfo {
    std::string resPath;
    std::string filePrefix;
    uint32_t imgCnt;
    uint32_t updInterval;
};
struct UxViewInfo;
class ImgViewAdapter : public OHOS::UIImageView, public ComponentCommon<ImgViewAdapter> {
    DISALLOW_COPY_MOVE(ImgViewAdapter);
    class ImgAnimatorCallback;
    static constexpr uint32_t MAX_IMG_CNT = 300;
    static constexpr uint32_t MAX_INTERVAL_MS = 5000;
    static constexpr uint32_t USECOND_TO_MSECOND = 1000;
    static constexpr int ANIMATION_FILE_NAME_LENGTH = 5;
public:
    using SpecificInfoType = UxImageInfo;
    static constexpr auto COMPONENT_TYPE = "UIImageView";
    ImgViewAdapter();
    explicit ImgViewAdapter(const UxViewInfo &info);
    ~ImgViewAdapter();
    bool Start();
    bool Stop();
#ifdef UPDATER_UT
    const ImgAnimatorCallback *GetAnimatorCallback() const;
    const OHOS::Animator *GetAnimator() const;
    uint32_t GetCurrId() const;
#endif
    static bool IsValid(const UxImageInfo &info);
private:
    void ShowNextImage();
    void ThreadCb();
    static bool IsValidForAnimator(const UxImageInfo &info);
    static bool IsValidForStaticImg(const UxImageInfo &info);
    bool animatorStop_ { true };
    bool valid_ { false };
    uint32_t currId_ { 0 };
    uint32_t imgCnt_ { 0 };
    uint32_t interval_ { 0 };
    std::string currPath_ {};
    std::string dir_ {};
    std::string filePrefix_ {};
    std::unique_ptr<ImgAnimatorCallback> cb_ {};
};
} // namespace Updater
#endif // UPDATER_UI_ANIMATION_LABLE_H
