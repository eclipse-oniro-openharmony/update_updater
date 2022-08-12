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

namespace Updater {
UpdaterUiFacade::UpdaterUiFacade()
    : strategies_ {UpdaterUiConfig::GetStrategy()}, pgMgr_ {PageManager::GetInstance()}, mode_ {MODEMAX}
{
}

UpdaterUiFacade &UpdaterUiFacade::GetInstance()
{
    static UpdaterUiFacade instance;
    return instance;
}

[[nodiscard]] bool UpdaterUiFacade::SetMode(UpdaterMode mode)
{
    if (mode < 0 || mode > MODEMAX) {
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

void UpdaterUiFacade::ShowLog(const std::string &tag, bool isClear) const
{
    ShowMsg(strategies_[mode_].labelLogId, tag, isClear);
}

void UpdaterUiFacade::ShowLogRes(const std::string &tag, bool isClear) const
{
    ShowMsg(strategies_[mode_].labelLogResId, tag, isClear);
}

void UpdaterUiFacade::ShowUpdInfo(const std::string &tag, bool isClear) const
{
    ShowMsg(strategies_[mode_].labelUpdId, tag, isClear);
}

void UpdaterUiFacade::ShowProgress(float value) const
{
    if (progress_ == nullptr) {
        LOG(ERROR) << "progress is null, can't show progress";
        return;
    }
    progress_->ShowProgress(value);
}

bool UpdaterUiFacade::IsInProgress() const
{
    return pgMgr_[strategies_[mode_].progressPage.progressPageId].IsVisible();
}

void UpdaterUiFacade::SetLogoVisible(bool isVisible) const
{
    if (logo_ == nullptr) {
        LOG(ERROR) << "logo is null, can't show logo";
        return;
    }
    isVisible ? logo_->Show() : logo_->Hide();
}

void UpdaterUiFacade::SetProgressVisible(bool isVisible) const
{
    if (progress_ == nullptr) {
        LOG(ERROR) << "progress is null, can't show progress";
        return;
    }
    isVisible ? progress_->Show() : progress_->Hide();
}

void UpdaterUiFacade::ShowProgressWarning(bool isShow) const
{
    auto &progressPg = strategies_[mode_].progressPage;
    pgMgr_[progressPg.progressPageId][progressPg.warningComId]->SetVisible(isShow);
}

void UpdaterUiFacade::ShowProgressPage() const
{
    if (IsInProgress()) {
        return;
    }
    SetProgressVisible(true);
    SetLogoVisible(true);
    ShowProgress(0);
    pgMgr_.ShowPage(strategies_[mode_].progressPage.progressPageId);
    ShowProgressWarning(false);
}

void UpdaterUiFacade::ShowSuccessPage() const
{
    LOG(DEBUG) << "show success page";
    SetProgressVisible(false);
    SetLogoVisible(false);
    StopLongPressTimer();
    pgMgr_.ShowPage(strategies_[mode_].resPage.successPageId);
}

void UpdaterUiFacade::ShowFailedPage() const
{
    LOG(DEBUG) << "show failed page";
    SetProgressVisible(false);
    SetLogoVisible(false);
    StopLongPressTimer();
    pgMgr_.ShowPage(strategies_[mode_].resPage.failPageId);
}

void UpdaterUiFacade::ShowFactoryConfirmPage()
{
    LOG(DEBUG) << "show confirm page";
    (void)SetMode(FACTORYRST);
    ClearLog();
    pgMgr_.ShowPage(strategies_[mode_].confirmPageId);
}

void UpdaterUiFacade::ShowMainpage() const
{
    pgMgr_.ShowMainPage();
}

void UpdaterUiFacade::ClearText() const
{
    ClearLog();
    ShowMsg(strategies_[mode_].labelUpdId, "");
}

void UpdaterUiFacade::ClearLog() const
{
    ShowMsg(strategies_[mode_].labelLogId, "");
    ShowMsg(strategies_[mode_].labelLogResId, "");
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
    ProgressPage &progressPage { strategies_[mode_].progressPage };
    progress_ = ProgressStrategy::Factory(progressPage.progressType, {
        progressPage.progressPageId, progressPage.progressComId
    });
    logo_ = LogoStrategy::Factory(progressPage.logoType, {
        progressPage.progressPageId, progressPage.logoComId
    });
}
}