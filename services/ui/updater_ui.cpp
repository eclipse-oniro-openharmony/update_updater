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
#include "updater_ui.h"
#include <mutex>
#include <thread>
#include "language/language_ui.h"
#include "log/log.h"
#include "page/page_manager.h"
#include "scope_guard.h"
#include "updater_main.h"
#include "updater_ui_facade.h"
#include "utils.h"

namespace Updater {
namespace {
auto &g_uiFacade = UpdaterUiFacade::GetInstance();
constexpr uint32_t DISPLAY_TIME = 1 * 1000 * 1000; /* 1s*/
constexpr uint32_t FAIL_DELAY = 5 * 1000 * 1000;
constexpr uint32_t SUCCESS_DELAY = 3 * 1000 * 1000;
constexpr int CALLBACK_DELAY = 20 * 1000; /* 20ms */
std::mutex g_mtx;
bool g_isInCallback { false };
bool g_timerStopped { false };
bool IsAlreadyInCallback()
{
    std::lock_guard<std::mutex> lck(g_mtx);
    if (!g_isInCallback) {
        return false;
    }
    g_isInCallback = true;
    return true;
}

void ExitCallback()
{
    std::lock_guard<std::mutex> lck(g_mtx);
    g_isInCallback = false;
}

/**
 * avoid calling multipule callback simultaneously.
 * When defining a new callback, should place a
 * CALLBACK_GUARD at the beginning of callback function
 */
#define CALLBACK_GUARD           \
    if (IsAlreadyInCallback()) { \
        return;                  \
    }                            \
    ON_SCOPE_EXIT(exitCallback)  \
    {                            \
        ExitCallback();          \
    }
}  // namespace

std::ostream &operator<<(std::ostream &os, const ComInfo &com)
{
    os << "pageId: " << com.pageId << " comId: " << com.comId;
    return os;
}

void DoProgress()
{
    constexpr int maxSleepMs = 1000 * 1000;
    constexpr int minSleepMs = 3000;
    constexpr float ratio = 10.0;
    // if 100 as fullpercent, then 0.3 per step
    constexpr int progressValueStep = static_cast<int>(0.3 * ratio);
    constexpr int maxProgressValue = static_cast<int>(100 * ratio);
    int progressvalueTmp = 0;
    if (g_uiFacade.GetMode() != UpdaterMode::FACTORYRST && g_uiFacade.GetMode() != UpdaterMode::REBOOTFACTORYRST) {
        return;
    }
    g_uiFacade.ShowProgress(0);
    while (progressvalueTmp <= maxProgressValue) {
        progressvalueTmp = progressvalueTmp + progressValueStep;
        g_uiFacade.ShowProgress(progressvalueTmp / ratio);
        Utils::UsSleep(minSleepMs);
        if (progressvalueTmp >= maxProgressValue) {
            Utils::UsSleep(maxSleepMs);
            return;
        }
    }
}

void OnRebootEvt()
{
    LOG(INFO) << "On Label Reboot";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            PostUpdater(false);
            Utils::DoReboot("");
        }
    }.detach();
}

void OnLabelResetEvt()
{
    LOG(INFO) << "On Label Reset";
    CALLBACK_GUARD;
    if (!g_uiFacade.SetMode(UpdaterMode::FACTORYRST)) {
        return;
    }
    g_uiFacade.ShowFactoryConfirmPage();
}

void OnLabelSDCardEvt()
{
    LOG(INFO) << "On Label SDCard";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            if (!g_uiFacade.SetMode(UpdaterMode::SDCARD)) {
                return;
            }
            Utils::UsSleep(CALLBACK_DELAY);
            g_uiFacade.ClearText();
            g_uiFacade.ShowProgress(0);
            g_uiFacade.ShowLog(TR(LOG_SDCARD_NOTMOVE));
            Utils::UsSleep(DISPLAY_TIME);
            if (UpdaterFromSdcard() != UPDATE_SUCCESS) {
                g_uiFacade.ShowMainpage();
                return;
            }
            PostUpdater(false);
            Utils::DoReboot("");
        }
    }.detach();
}

void OnLabelSDCardNoDelayEvt()
{
    LOG(INFO) << "On Label SDCard";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            if (!g_uiFacade.SetMode(UpdaterMode::SDCARD)) {
                return;
            }
            Utils::UsSleep(CALLBACK_DELAY);
            if (auto res = UpdaterFromSdcard(); res != UPDATE_SUCCESS) {
                g_uiFacade.ShowLogRes(res == UPDATE_CORRUPT ? TR(LOGRES_VERIFY_FAILED) : TR(LOGRES_UPDATE_FAILED));
                g_uiFacade.ShowFailedPage();
                Utils::UsSleep(FAIL_DELAY);
                g_uiFacade.ShowMainpage();
                return;
            }
            g_uiFacade.ShowLogRes(TR(LABEL_UPD_OK_DONE));
            g_uiFacade.ShowSuccessPage();
            Utils::UsSleep(SUCCESS_DELAY);
            PostUpdater(false);
            Utils::DoReboot("");
        }
    }.detach();
}

void OnLabelCancelEvt()
{
    CALLBACK_GUARD;
    LOG(INFO) << "On Label Cancel";
    PageManager::GetInstance().GoBack();
}

void OnLabelOkEvt()
{
    LOG(INFO) << "On Label Ok";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            Utils::UsSleep(CALLBACK_DELAY);
            g_uiFacade.ShowMainpage();
            g_uiFacade.ClearText();
            g_uiFacade.ShowLog(TR(LOG_WIPE_DATA));
            if (!g_uiFacade.SetMode(UpdaterMode::FACTORYRST)) {
                return;
            }
            g_uiFacade.ShowProgress(0);
            g_uiFacade.ShowProgressPage();
            DoProgress();
            if (FactoryReset(USER_WIPE_DATA, "/data") == 0) {
                g_uiFacade.ShowLog(TR(LOG_WIPE_DONE));
                g_uiFacade.ShowSuccessPage();
            } else {
                g_uiFacade.ShowLog(TR(LOG_WIPE_FAIL));
                g_uiFacade.ShowFailedPage();
            }
        }
    }.detach();
}

void OnConfirmRstEvt()
{
    LOG(INFO) << "On Label Ok";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            if (!g_uiFacade.SetMode(UpdaterMode::FACTORYRST)) {
                return;
            }
            g_uiFacade.ShowUpdInfo(TR(LABEL_RESET_PROGRESS_INFO));
            g_uiFacade.ShowProgressPage();
            DoProgress();
            if (FactoryReset(USER_WIPE_DATA, "/data") != 0) {
                g_uiFacade.ShowLogRes(TR(LOG_WIPE_FAIL));
                g_uiFacade.ShowFailedPage();
                Utils::UsSleep(FAIL_DELAY);
                g_uiFacade.ShowMainpage();
            } else {
                g_uiFacade.ShowUpdInfo(TR(LOGRES_WIPE_FINISH));
                Utils::UsSleep(DISPLAY_TIME);
                g_uiFacade.ShowSuccessPage();
            }
        }
    }.detach();
}

void OnMenuShutdownEvt()
{
    LOG(INFO) << "On btn shutdown";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            LOG(DEBUG) << "shutdown";
            Utils::DoShutdown();
        }
    }.detach();
}

void OnMenuClearCacheEvt()
{
    LOG(INFO) << "On clear cache";
    std::thread {
        [] () {
            CALLBACK_GUARD;
            g_uiFacade.ClearText();
            if (!g_uiFacade.SetMode(UpdaterMode::FACTORYRST)) {
                return;
            }
            Utils::UsSleep(CALLBACK_DELAY);
            g_uiFacade.ShowUpdInfo(TR(LOG_CLEAR_CAHCE));
            g_uiFacade.ShowProgressPage();
            ClearMisc();
            DoProgress();
            g_uiFacade.ShowMainpage();
        }
    }.detach();
}

void StartLongPressTimer()
{
    static int downCount { 0 };
    if (!g_uiFacade.IsInProgress()) {
        return;
    }
    ++downCount;
    g_timerStopped = false;
    using namespace std::literals::chrono_literals;
    std::thread t { [lastdownCount = downCount] () {
        constexpr auto threshold = 2s;
        std::this_thread::sleep_for(threshold);
        /**
         * When the downCount of the last power key press changes,
         * it means that the last press has been released before
         * the timeout, then you can exit the callback directly
         */
        if (g_timerStopped || lastdownCount != downCount) {
            return;
        }
        // show warning
        g_uiFacade.ShowProgressWarning(true);
    }};
    t.detach();
}

void StopLongPressTimer()
{
    // no need to judge whether in progress page,
    // because may press power key in progress
    // page and release power key in other page
    g_timerStopped = true;
    // hide warning
    g_uiFacade.ShowProgressWarning(false);
}
} // namespace Updater
