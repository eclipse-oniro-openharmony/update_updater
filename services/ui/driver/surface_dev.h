/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef UPDATER_UI_SURFACE_DEV_H
#define UPDATER_UI_SURFACE_DEV_H
#include <string>
#include "macros.h"
#include "graphic_drv.h"

namespace Updater {
enum class DevType {
    FB_DEVICE = 0,
    DRM_DEVICE,
    UNKNOW_DEVICE
};

class SurfaceDev {
    DISALLOW_COPY_MOVE(SurfaceDev);
public:
    SurfaceDev() = default;
    ~SurfaceDev();
    void Flip(const uint8_t* buf) const;
    void GetScreenSize(uint16_t &w, uint16_t &h) const;
    bool Init();
private:
    DevType GetDevType() const;
    std::unique_ptr<GraphicDrv> MakeDevDrv(DevType devType);
    std::unique_ptr<GraphicDrv> drv_ {};
};
} // namespace Updater
#endif
