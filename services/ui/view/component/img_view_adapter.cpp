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
#include "img_view_adapter.h"
#include <sstream>
#include <unistd.h>
#include "core/render_manager.h"
#include "log/log.h"
#include "page/view_proxy.h"
#include "updater/updater_const.h"
#include "updater_ui_const.h"
#include "updater_ui_env.h"
#include "utils.h"

namespace Updater {
class ImgViewAdapter::ImgAnimatorCallback final : public OHOS::AnimatorCallback {
    DISALLOW_COPY_MOVE(ImgAnimatorCallback);
public:
    explicit ImgAnimatorCallback(ImgViewAdapter *view)
        : animator_(nullptr), stop_(true)
    {
        view_ = view;
        if (view_ == nullptr) {
            static ImgViewAdapter dummy;
            view_ = &dummy;
        }
    }
    ~ImgAnimatorCallback() = default;
    void Init()
    {
        animator_ = std::make_unique<OHOS::Animator>(this, view_, 0, true);
    }

    void Callback(OHOS::UIView *view) override
    {
        if (stop_) {
            return;
        }
        view_->ShowNextImage();
    }
    void Start()
    {
        view_->SetVisible(true);
        stop_ = false;
    }
    void Stop()
    {
        view_->SetVisible(false);
        stop_ = true;
    }
    OHOS::Animator *GetAnimator() const
    {
        return animator_.get();
    }
protected:
    ImgViewAdapter *view_;
    std::unique_ptr<OHOS::Animator> animator_;
    bool stop_;
};

ImgViewAdapter::~ImgViewAdapter() = default;

ImgViewAdapter::ImgViewAdapter() = default;

ImgViewAdapter::ImgViewAdapter(const UxViewInfo &info)
{
    const UxViewCommonInfo &common = info.commonInfo;
    const UxImageInfo &spec = std::get<UxImageInfo>(info.specificInfo);
    dir_ = spec.resPath;
    imgCnt_ = spec.imgCnt;
    interval_ = spec.updInterval;
    filePrefix_ = spec.filePrefix;
    currId_ = 0;
    valid_ = true;
    viewId_ = common.id;
    this->SetAutoEnable(false);
    this->SetPosition(common.x, common.y);
    this->SetWidth(common.w);
    this->SetHeight(common.h);
    this->SetVisible(common.visible);
    this->SetViewId(viewId_.c_str());
    LOG(INFO) << "dir:" << dir_ << ", imgCnt:" << imgCnt_ << ", interval:" << interval_;
    if (interval_ == 0) {
        this->SetSrc(dir_.c_str());
        this->SetResizeMode(OHOS::UIImageView::ImageResizeMode::FILL);
    } else {
        cb_ = std::make_unique<ImgAnimatorCallback>(this);
        cb_->Init();
    }
}

bool ImgViewAdapter::IsValid(const UxImageInfo &info)
{
    if (info.updInterval == 0) {
        return IsValidForStaticImg(info);
    }
    return IsValidForAnimator(info);
}

bool ImgViewAdapter::IsValidForStaticImg(const UxImageInfo &info)
{
    if (info.resPath.empty()) {
        LOG(ERROR) << "img viewinfo check failed, resPath is empty when this is a static image";
        return false;
    }
    return true;
}

bool ImgViewAdapter::IsValidForAnimator(const UxImageInfo &info)
{
    if (info.filePrefix.empty() || info.resPath.empty()) {
        LOG(ERROR) << "img viewinfo check failed, filePrefix is empty when this is a animator";
        return false;
    }

    if ((info.imgCnt > MAX_IMG_CNT) || (info.imgCnt == 0)) {
        LOG(ERROR) << "img viewinfo check failed, imgCnt: " << info.imgCnt;
        return false;
    }

    if (info.updInterval > MAX_INTERVAL_MS) {
        LOG(ERROR) << "img viewinfo check failed, updInterval: " << info.updInterval;
        return false;
    }

    return true;
}

bool ImgViewAdapter::Start()
{
    if (!valid_ || !animatorStop_) {
        return false;
    }
    if (cb_ == nullptr) {
        LOG(ERROR) << "cb_ is null";
        return false;
    }
    cb_->Start();
    cb_->GetAnimator()->Start();
    animatorStop_ = false;
    return true;
}

bool ImgViewAdapter::Stop()
{
    if (!valid_ || animatorStop_) {
        return false;
    }
    if (cb_ == nullptr) {
        LOG(ERROR) << "cb_ is null";
        return false;
    }
    cb_->GetAnimator()->Stop();
    cb_->Stop();
    animatorStop_ = true;
    return true;
}

void ImgViewAdapter::ShowNextImage()
{
    std::stringstream ss;
    ss << dir_ << filePrefix_ << std::setw(ANIMATION_FILE_NAME_LENGTH) << std::setfill('0') << currId_ << ".png";
    currPath_ = ss.str();
    if (access(currPath_.c_str(), F_OK) == -1) {
        LOG(ERROR) << "not exist: " << currPath_;
    }

    this->SetSrc(currPath_.c_str());
    this->SetResizeMode(OHOS::UIImageView::ImageResizeMode::FILL);
    currId_ = (currId_ + 1) % imgCnt_;
    Utils::UsSleep(interval_ * USECOND_TO_MSECOND);
}

#ifdef UPDATER_UT
const ImgViewAdapter::ImgAnimatorCallback *ImgViewAdapter::GetAnimatorCallback() const
{
    return cb_.get();
}

const OHOS::Animator *ImgViewAdapter::GetAnimator() const
{
    if (!cb_) {
        return nullptr;
    }
    return cb_->GetAnimator();
}


uint32_t ImgViewAdapter::GetCurrId() const
{
    return currId_;
}
#endif
} // namespace Updater
