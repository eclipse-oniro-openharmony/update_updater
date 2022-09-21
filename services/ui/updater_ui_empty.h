
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

#ifndef UPDATER_UI_UPDATER_UI_EMPTY
#define UPDATER_UI_UPDATER_UI_EMPTY

#include <string>
#include "updater_ui.h"

namespace Updater {
class UpdaterUiEmpty {
public:
    static UpdaterUiEmpty &GetInstance()
    {
        static UpdaterUiEmpty instance;
        return instance;
    }
    void InitEnv() const {}
    [[nodiscard]] bool SetMode(UpdaterMode mode)
    {
        return true;
    }
    UpdaterMode GetMode() const
    {
        return UpdaterMode::MODEMAX;
    }
    void ShowLog(const std::string &tag, bool isClear = false) const {}
    void ShowLogRes(const std::string &tag, bool isClear = false) const {}
    void ShowUpdInfo(const std::string &tag, bool isClear = false) const {}
    void ClearText() const {}
    void ClearLog() const {}
    void ShowProgress(float value) const {}
    void ShowProgressPage() const {}
    void ShowSuccessPage() const {}
    void ShowFailedPage() const {}
    void ShowFactoryConfirmPage() {}
    void ShowMainpage() const {}
    void ShowProgressWarning(bool isShow) const {}
    bool IsInProgress() const
    {
        return false;
    }
    void Sleep(int ms) const {}
    void SaveScreen() const {}
};
} // namespace Updater
#endif