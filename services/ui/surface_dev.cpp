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
#include "surface_dev.h"
#include "log/log.h"
#include "updater_ui_const.h"

namespace Updater {
void SurfaceDev::Flip(const void *buf)
{
    if (!buf) {
        LOG(ERROR) << "buf is null";
        return;
    }
    this->FlipBuffer(buf);
}

SurfaceDev::SurfaceDev(SurfaceDev::DevType devType)
{
    screenSizeW_ = SCREEN_WIDTH;
    screenSizeH_ = SCREEN_HEIGHT;
    if (devType == SurfaceDev::DevType::DRM_DEVICE) {
        this->LoadDrmDriver();
    } else {
        LOG(ERROR) << " Only Support drm driver.";
    }
}

void SurfaceDev::GetScreenSize(int &w, int &h)
{
    w = screenSizeW_;
    h = screenSizeH_;
}

SurfaceDev::~SurfaceDev()
{
    LOG(INFO)<<"SurfaceDev end";
}
} // namespace updater