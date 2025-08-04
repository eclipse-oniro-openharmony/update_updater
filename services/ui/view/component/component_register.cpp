/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "component_register.h"
#include "log/log.h"
#include "updater_init.h"
#include "updater_ui_traits.h"

namespace Updater {
void RegisterComponents()
{
    LOG(INFO) << "Register Ux components";
    (void)BoxProgressAdapter::RegisterHook();
    (void)ImgViewAdapter::RegisterHook();
    (void)TextLabelAdapter::RegisterHook();
    (void)LabelBtnAdapter::RegisterHook();

    UpdaterInit::GetInstance().InvokeEvent(UpdaterInitEvent::UPDATER_COMPONENT_REGISTER_EVENT);
    LOG(INFO) << "Ux components ready";
}

}  // namespace Updater
