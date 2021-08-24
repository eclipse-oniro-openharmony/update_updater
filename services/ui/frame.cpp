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

#include "frame.h"
#include <linux/input.h>
#include "log/log.h"
#include "updater_ui_const.h"
#include "view.h"

namespace updater {
using namespace std;
extern int g_textLabelNum;
extern Frame *g_menuFrame;

Frame::Frame(unsigned int w, unsigned int h, View::PixelFormat pixType, SurfaceDev *sfDev)
{
    this->CreateBuffer(w, h, pixType);
    this->startX_ = 0;
    this->startY_ = 0;
    sfDev_ = sfDev;
    listIndex_ = 0;
    flushFlag_ = false;
#ifndef UPDATER_UT
    flushLoop_ = std::thread(&Frame::FlushThreadLoop, this);
    flushLoop_.detach();
#endif
#ifdef CONVERT_RL_SLIDE_TO_CLICK
    keyProcessLoop_ = std::thread(&Frame::ProcessKeyLoop, this);
    keyProcessLoop_.detach();
#endif
    currentActionIndex_ = 0;
}

Frame::~Frame()
{
    needStop_ = true;
    std::unique_lock<std::mutex> locker(mutex_);
    flushFlag_ = true;
    viewMapList_.clear();
}

void Frame::FlushThreadLoop()
{
    while (!needStop_) {
        std::unique_lock<std::mutex> locker(mutex_);
        while (!flushFlag_) {
            mCondFlush_.wait(mutex_, [&] {
                return flushFlag_;
            });
        }
        if (!IsVisiable()) {
            flushFlag_ = false;
            continue;
        }
        SyncBuffer();
        frameMutex_.lock();
        std::map<View*, int>::iterator iter;
        for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
            View* tmpView = (*iter).first;
            if (tmpView->IsVisiable()) {
                void* bufTmp = tmpView->GetBuffer();
                DrawSubView(tmpView->startX_, tmpView->startY_, tmpView->viewWidth_, tmpView->viewHeight_, bufTmp);
            }
        }
        frameMutex_.unlock();
        sfDev_->Flip(this->GetBuffer());
        flushFlag_ = false;
    }
}

void Frame::ProcessKeyLoop()
{
    while (!needStop_) {
        std::unique_lock<std::mutex> locker(keyMutex_);
        if (!keyEventNotify_) {
            mCondKey_.wait(keyMutex_, [&] {
                return keyEventNotify_;
            });
        }
        if (!IsVisiable()) {
            continue;
        }
        int key = keyFifo_.front();
        keyFifo_.pop_front();
        DoEvent(key);
        keyEventNotify_ = false;
    }
}

void Frame::ViewRegister(View *view)
{
    std::unique_lock<std::mutex> locker(frameMutex_);
    view->SetViewId(frameViewId + listIndex_);
    viewMapList_.insert(std::make_pair(view, frameViewId + listIndex_));
    if (view->IsFocusAble()) {
        maxActionIndex_++;
        LOG(INFO) << "---";
    }
    listIndex_++;
}

void Frame::OnDraw()
{
    std::unique_lock<std::mutex> locker(mutex_);
    flushFlag_ = true;
    mCondFlush_.notify_all();
}

void Frame::UpFoucs()
{
    currentActionIndex_--;
    if (currentActionIndex_ == -1) {
        currentActionIndex_ = 0;
        return;
    }

    frameMutex_.lock();
    int actionIndexTemp = 0;
    std::map<View*, int>::iterator iter;
    for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
        View* tmpView = (*iter).first;
        if (tmpView->IsVisiable() && tmpView->IsFocusAble()) {
            if (actionIndexTemp == currentActionIndex_) {
                frameMutex_.unlock();
                tmpView->OnFocus(true);
                frameMutex_.lock();
            }
            if (actionIndexTemp == currentActionIndex_ + 1) {
                frameMutex_.unlock();
                tmpView->OnFocus(false);
                frameMutex_.lock();
                break;
            }
        }
        actionIndexTemp++;
    }
    frameMutex_.unlock();
}

void Frame::DownFoucs()
{
    UPDATER_CHECK_ONLY_RETURN(currentActionIndex_ != g_textLabelNum, return);
    int actionIndexTemp = 0;
    frameMutex_.lock();
    std::map<View*, int>::iterator iter;
    currentActionIndex_++;
    View *view = nullptr;
    for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
        View *tmpView = (*iter).first;
        if (tmpView->IsVisiable() && tmpView->IsFocusAble()) {
            if (actionIndexTemp == currentActionIndex_ - 1) {
                frameMutex_.unlock();
                tmpView->OnFocus(false);
                frameMutex_.lock();
            }
            view = tmpView;
            if (currentActionIndex_ == g_textLabelNum && view != nullptr) {
                currentActionIndex_ = 0;
                frameMutex_.unlock();
                view->OnFocus(false);
                frameMutex_.lock();
            }
            if (actionIndexTemp == currentActionIndex_) {
                frameMutex_.unlock();
                tmpView->OnFocus(true);
                frameMutex_.lock();
                break;
            }
        } else {
            if (tmpView->IsVisiable() && tmpView->IsFocusAble()) {
                frameMutex_.unlock();
                tmpView->OnFocus(false);
                frameMutex_.lock();
            }
        }
        actionIndexTemp++;
    }
    if (iter == viewMapList_.end()) {
        currentActionIndex_ = 0;
        for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
            View* tmpView = (*iter).first;
            if (tmpView->IsVisiable() && tmpView->IsFocusAble()) {
                tmpView->OnFocus(true);
                break;
            }
        }
    }
    frameMutex_.unlock();
}

void Frame::SendKey(int key)
{
    frameMutex_.lock();
    int actionIndexTemp = 0;
    std::map<View*, int>::iterator iter;
    for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
        View* tmpView = (*iter).first;
        if (tmpView->IsVisiable() && tmpView->IsFocusAble()) {
            if (actionIndexTemp == currentActionIndex_) {
                frameMutex_.unlock();
                tmpView->OnKeyEvent(key);
                frameMutex_.lock();
                break;
            }
        }
        actionIndexTemp++;
    }
    frameMutex_.unlock();
}

void Frame::DoEvent(int key)
{
    UPDATER_ERROR_CHECK(IsVisiable(), "Is not visable", return);
    UPDATER_ERROR_CHECK(!IsFocusAble(), "Is not fouces", return);
    switch (key) {
        case KEY_UP:
            LOG(INFO) << "DispatchKeyEvent KEY_UP";
            UpFoucs();
            break;
        case KEY_DOWN:
            LOG(INFO) << "DispatchKeyEvent KEY_DOWN";
            DownFoucs();
            break;
        case KEY_POWER:
            LOG(INFO) << "DispatchKeyEvent KEY_POWER";
            SendKey(key);
            break;
        default:
            break;
    }
}

void Frame::DispatchKeyEvent(int key)
{
    UPDATER_CHECK_ONLY_RETURN(IsVisiable(), return);
    std::unique_lock<std::mutex> locker(keyMutex_);
    keyFifo_.push_back(key);
    keyEventNotify_ = true;
    mCondKey_.notify_all();
}

void Frame::DispatchKeyEvent(int id, int event)
{
    bool isClicked = (keyEvent_ == event);
    keyEvent_ = event;
    if (isClicked) {
        return;
    }
    if (!g_menuFrame->IsVisiable()) {
        event = -1;
    }
    switch (event) {
        case INVALID_EVENT:
            LOG(INFO) << "DispatchKeyEvent invalid";
            break;
        case PRESS_EVENT:
            btnId_ = id;
            PressEvent();
            LOG(INFO) << "DispatchKeyEvent press";
            break;
        case RELEASE_EVENT:
            btnId_ = id;
            ReleaseEvent();
            LOG(INFO) << "DispatchKeyEvent release";
            break;
        default:
            break;
        }
}

void Frame::ReleaseEvent()
{
    frameMutex_.lock();
    std::map<View*, int>::iterator iter;
    for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
        View* tmpView = (*iter).first;
        if (tmpView->IsVisiable() && btnId_ == tmpView->GetViewId()) {
            if (!g_menuFrame->IsVisiable()) {
                return;
            }
            frameMutex_.unlock();
            tmpView->OnKeyEvent(KEY_POWER);
            tmpView->OnFocus(false);
            frameMutex_.lock();
            break;
        }
    }
    frameMutex_.unlock();
}

void Frame::PressEvent()
{
    frameMutex_.lock();
    std::map<View*, int>::iterator iter;
    for (iter = viewMapList_.begin(); iter != viewMapList_.end(); ++iter) {
        View* tmpView = (*iter).first;
        if (tmpView->IsVisiable() && btnId_ == tmpView->GetViewId()) {
            if (!g_menuFrame->IsVisiable()) {
                return;
            }
            frameMutex_.unlock();
            tmpView->OnFocus(true);
            frameMutex_.lock();
            break;
        }
    }
    frameMutex_.unlock();
}
} // namespace updater