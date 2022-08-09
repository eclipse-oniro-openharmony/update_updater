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

#ifndef UPDATER_UI_FBDEV_DRIVER_H
#define UPDATER_UI_FBDEV_DRIVER_H

#include <cstdint>
#include <linux/fb.h>
#include "macros.h"
#include "graphic_drv.h"

namespace Updater {
struct FbBufferObject {
    uint32_t width {};
    uint32_t height {};
    uint32_t size {};
    void *vaddr {};
};

class FbdevDriver : public GraphicDrv {
    DISALLOW_COPY_MOVE(FbdevDriver);
public:
    FbdevDriver() = default;
    ~FbdevDriver() override;
    bool Init() override;
    void Flip(const uint8_t *buf) override;
    void GetGrSurface(GrSurface &surface) override;
private:
    void FBLog() const;
    void ReleaseFb(const struct FbBufferObject *fbo);
    struct FbBufferObject buff_ {};
    struct fb_fix_screeninfo finfo_ {};
    struct fb_var_screeninfo vinfo_ {};
};
} // namespace Updater
#endif
