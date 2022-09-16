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

#ifndef UPDATER_UI_FACADE_H
#define UPDATER_UI_FACADE_H

#include "macros.h"
#include "strategy/ui_strategy.h"
#include "updater_ui.h"

namespace Updater {
class UpdaterUiFacade {
    DISALLOW_COPY_MOVE(UpdaterUiFacade);
    using StrategyMap = std::unordered_map<UpdaterMode, UiStrategyCfg>;
public:
    UpdaterUiFacade();
    ~UpdaterUiFacade() = default;
    static UpdaterUiFacade &GetInstance();

    void InitEnv() const;

    [[nodiscard]] bool SetMode(UpdaterMode mode);
    UpdaterMode GetMode() const;

    void ShowLog(const std::string &tag, bool isClear = false) const;
    void ShowLogRes(const std::string &tag, bool isClear = false) const;
    void ShowUpdInfo(const std::string &tag, bool isClear = false) const;
    void ClearText() const;
    void ClearLog() const;

    void ShowProgress(float value) const;
    void ShowProgressPage() const;
    void ShowSuccessPage() const;
    void ShowFailedPage() const;
    void ShowFactoryConfirmPage();
    void ShowMainpage() const;
    void ShowProgressWarning(bool isShow) const;
    bool IsInProgress() const;

    void Sleep(int ms) const;
    void SaveScreen() const;
private:
    std::pair<bool, StrategyMap::const_iterator> CheckMode() const;
    void SetLogoVisible(bool isVisible) const;
    void SetProgressVisible(bool isVisible) const;
    void ShowMsg(const ComInfo &id, const std::string &tag, bool isClear) const;
    void ShowMsg(const ComInfo &id, const std::string &tag) const;

    void SetLogoProgress();
    const StrategyMap &strategies_;
    PageManager &pgMgr_;
    UpdaterMode mode_;
    std::unordered_map<UpdaterMode, std::unique_ptr<ProgressStrategy>> progressMap_ {};
    std::unordered_map<UpdaterMode, std::unique_ptr<LogoStrategy>> logoMap_ {};
};
} // namespace Updater
#endif
