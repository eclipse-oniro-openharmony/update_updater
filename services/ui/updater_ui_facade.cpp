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

#include "updater_ui_facade.h"
#include <thread>
#include "component/text_label_adapter.h"
#include "updater_ui_config.h"
#include "updater_ui_env.h"
#include "updater_ui_tools.h"

namespace Updater {
UpdaterUiFacade::UpdaterUiFacade()
    : strategies_ {UpdaterUiConfig::GetStrategy()}, pgMgr_ {PageManager::GetInstance()}, mode_ {UpdaterMode::MODEMAX}
{
}

UpdaterUiFacade &UpdaterUiFacade::GetInstance()
{
    static UpdaterUiFacade instance;
    return instance;
}

void UpdaterUiFacade::InitEnv() const
{
    UpdaterUiEnv::Init();
}

[[nodiscard]] bool UpdaterUiFacade::SetMode(UpdaterMode mode)
{
    if (mode < UpdaterMode::SDCARD || mode >= UpdaterMode::MODEMAX) {
        LOG(ERROR) << "updater mode invalid";
        return false;
    }
    if (mode == mode_) {
        return true;
    }
    mode_ = mode;
    SetLogoProgress();
    return true;
}

UpdaterMode UpdaterUiFacade::GetMode() const
{
    return mode_;
}

std::pair<bool, UpdaterUiFacade::StrategyMap::const_iterator> UpdaterUiFacade::CheckMode() const
{
    if (mode_ < UpdaterMode::SDCARD || mode_ >= UpdaterMode::MODEMAX) {
        LOG(ERROR) << "mode invalid";
        return {false, strategies_.cend()};
    }
    auto it = strategies_.find(mode_);
    if (it == strategies_.end()) {
        LOG(ERROR) << "mode has not a strategy for it";
        return {false, strategies_.cend()};
    }
    return {true, it};
}

void UpdaterUiFacade::ShowLog(const std::string &tag, bool isClear) const
{
    if (auto [res, it] = CheckMode(); res) {
        ShowMsg(it->second.labelLogId, tag, isClear);
    }
}

void UpdaterUiFacade::ShowLogRes(const std::string &tag, bool isClear) const
{
    if (auto [res, it] = CheckMode(); res) {
        ShowMsg(it->second.labelLogResId, tag, isClear);
    }
}

void UpdaterUiFacade::ShowUpdInfo(const std::string &tag, bool isClear) const
{
    if (auto [res, it] = CheckMode(); res) {
        ShowMsg(it->second.labelUpdId, tag, isClear);
    }
}

void UpdaterUiFacade::ShowProgress(float value) const
{
    if (!CheckMode().first) {
        return;
    }
    static float lastValue = 0.0;
    if (abs(value - lastValue) > 0.01) { // 0.01 : The progress bar changes by more than 0.01
        LOG(INFO) << "current progress " << value;
        lastValue = value;
    }
    if (auto it = progressMap_.find(mode_); it->second != nullptr) {
        it->second->ShowProgress(value);
        return;
    }
    LOG(ERROR) << "progress is null, can't show progress";
}

bool UpdaterUiFacade::IsInProgress() const
{
    if (auto [res, it] = CheckMode(); res) {
        return pgMgr_[it->second.progressPage.progressPageId].IsVisible();
    }
    return false;
}

void UpdaterUiFacade::SetLogoVisible(bool isVisible) const
{
    if (!CheckMode().first) {
        return;
    }
    if (auto it = logoMap_.find(mode_); it->second != nullptr) {
        isVisible ? it->second->Show() : it->second->Hide();
        return;
    }
    LOG(ERROR) << "logo is null, can't show logo";
}

void UpdaterUiFacade::SetProgressVisible(bool isVisible) const
{
    if (!CheckMode().first) {
        return;
    }
    if (auto it = progressMap_.find(mode_); it->second != nullptr) {
        isVisible ? it->second->Show() : it->second->Hide();
        return;
    }
    LOG(ERROR) << "progress is null, can't show progress";
}

void UpdaterUiFacade::ShowProgressWarning(bool isShow) const
{
    if (auto [res, it] = CheckMode(); res) {
        const auto &progressPg = it->second.progressPage;
        pgMgr_[progressPg.progressPageId][progressPg.warningComId]->SetVisible(isShow);
    }
}

void UpdaterUiFacade::ShowProgressPage() const
{
    auto [res, it] = CheckMode();
    if (IsInProgress() || !res) {
        return;
    }
    SetProgressVisible(true);
    SetLogoVisible(true);
    ShowProgress(0);
    pgMgr_.ShowPage(it->second.progressPage.progressPageId);
    ShowProgressWarning(false);
}

void UpdaterUiFacade::ShowSuccessPage() const
{
    auto [res, it] = CheckMode();
    if (!res) {
        return;
    }
    LOG(DEBUG) << "show success page";
    SetProgressVisible(false);
    SetLogoVisible(false);
    StopLongPressTimer();
    pgMgr_.ShowPage(it->second.resPage.successPageId);
}

void UpdaterUiFacade::ShowFailedPage() const
{
    auto [res, it] = CheckMode();
    if (!res) {
        return;
    }
    LOG(DEBUG) << "show failed page";
    SetProgressVisible(false);
    SetLogoVisible(false);
    StopLongPressTimer();
    pgMgr_.ShowPage(it->second.resPage.failPageId);
}

void UpdaterUiFacade::ShowFactoryConfirmPage()
{
    auto [res, it] = CheckMode();
    if (!res) {
        return;
    }
    LOG(DEBUG) << "show confirm page";
    ClearLog();
    pgMgr_.ShowPage(it->second.confirmPageId);
}

void UpdaterUiFacade::ShowMainpage() const
{
    pgMgr_.ShowMainPage();
}

void UpdaterUiFacade::ClearText() const
{
    auto [res, it] = CheckMode();
    if (!res) {
        return;
    }
    ClearLog();
    ShowMsg(it->second.labelUpdId, "");
}

void UpdaterUiFacade::ClearLog() const
{
    if (auto [res, it] = CheckMode(); res) {
        ShowMsg(it->second.labelLogId, "");
        ShowMsg(it->second.labelLogResId, "");
    }
}

void UpdaterUiFacade::ShowMsg(const ComInfo &id, const std::string &tag, bool isClear) const
{
    if (isClear) {
        LOG(INFO) << "clear all log label's text";
        ClearText();
    }
    pgMgr_[id.pageId][id.comId].As<TextLabelAdapter>()->SetText(tag);
}

void UpdaterUiFacade::ShowMsg(const ComInfo &id, const std::string &tag) const
{
    pgMgr_[id.pageId][id.comId].As<TextLabelAdapter>()->SetText(tag);
}

void UpdaterUiFacade::SetLogoProgress()
{
    auto [res, it] = CheckMode();
    if (!res) {
        return;
    }
    const ProgressPage &progressPage { it->second.progressPage };
    if (progressMap_.find(mode_) == progressMap_.end()) {
        progressMap_[mode_] = ProgressStrategy::Factory(progressPage.progressType, {
            progressPage.progressPageId, progressPage.progressComId
        });
    }
    if (logoMap_.find(mode_) == logoMap_.end()) {
        logoMap_[mode_] = LogoStrategy::Factory(progressPage.logoType, {
            progressPage.progressPageId, progressPage.logoComId
        });
    }
}

void UpdaterUiFacade::Sleep(int ms) const
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void UpdaterUiFacade::SaveScreen() const
{
    UpdaterUiTools::SaveUxBuffToFile("/tmp/mainpage.png");
}
} // namespace Updater