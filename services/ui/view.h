/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef UPDATER_UI_VIEW_H
#define UPDATER_UI_VIEW_H
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>

namespace updater {
class View {
public:
    struct BRGA888Pixel {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };

    struct RGB888Pixel {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    };

    enum PixelFormat {
        BGRA888,
    };
    View() {};
    virtual ~View();
    virtual void SetBackgroundColor(BRGA888Pixel *color);
    virtual void DrawSubView(int x, int y, int w, int h, void *buf);
    virtual void OnKeyEvent(int key);
    virtual void OnDraw();
    virtual void Hide();
    virtual void Show();
    virtual void OnFocus(bool isFocused);
    void* GetBuffer() const;
    void* GetRawBuffer() const;
    int GetBufferSize() const
    {
        return bufferSize_;
    }
    void SyncBuffer();
    void SetViewId(int id);
    int GetViewId() const;
    void FreeBuffer();
    bool IsVisiable() const;
    bool IsSelected() const;
    bool IsFocusAble() const;
    void SetFocusAble(bool foucsAble);
    int startX_ = 0;
    int startY_ = 0;
    int viewWidth_ = 0;
    int viewHeight_ = 0;
    std::mutex mutex_;
protected:
    void* CreateBuffer(int w, int h, int pixelFormat);
    char* viewBuffer_ = nullptr;
    char* shadowBuffer_ = nullptr;
private:
    int bufferSize_ = 0;
    bool isVisiable_ = true;
    int viewId_ = 0;
    bool focusable_ = false;
    bool isFocused_ = false;
};
} // namespace updater
#endif // UPDATER_UI_VIEW_H
