/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef UPDATER_UI_STUB_H
#define UPDATER_UI_STUB_H

#include "macros.h"
#ifdef UPDATER_UI_SUPPORT
#include "updater_ui_facade.h"
#else
#include "updater_ui_empty.h"
#endif

namespace Updater {
class UpdaterUiStub {
    DISALLOW_COPY_MOVE(UpdaterUiStub);
public:
    UpdaterUiStub() = default;
    ~UpdaterUiStub() = default;
#ifdef UPDATER_UI_SUPPORT
    static UpdaterUiFacade &GetInstance();
#else
    /* add extra parameter with default value. Because c++ function mangling name
     * don't consider return type, so UpdaterUiFacade &GetInstance() and
     * UpdaterUiEmpty &GetInstance() is regarded as same function when linking
     * which may cause undefined behavior. This will happen when you define UPDATER_UI_SUPPORT
     * in one translation unit but don't define UPDATER_UI_SUPPORT in another. So when adding
     * an extra parameter in UpdaterUiEmpty &GetInstance(), linker will regard these two function
     * as different and report an undefined symbol error which would be safer.
     */
    static UpdaterUiEmpty &GetInstance([[maybe_unused]] bool extra = false);
#endif
};
} // namespace Updater
#endif
