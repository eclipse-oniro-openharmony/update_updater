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
#include "control/callback_manager.h"
#include "language/language_ui.h"
#include "driver/graphic_engine.h"
#include "log/log.h"
#include "page/page_manager.h"
#include "scope_guard.h"
#include "updater_main.h"
#include "updater_ui_facade.h"
#include "updater/updater_const.h"
#include "utils.h"
#include "updater_ui_stub.h"

namespace Updater {
namespace {
constexpr uint32_t DISPLAY_TIME = 1 * 1000 * 1000; /* 1s */
constexpr uint32_t SUCCESS_DELAY = 3 * 1000 * 1000;
constexpr int CALLBACK_DELAY = 20 * 1000; /* 20ms */

inline auto &GetFacade()
{
    return UpdaterUiFacade::GetInstance();
}
}  // namespace

void DoProgress()
{
    constexpr int maxSleepMs = 1000 * 1000;
    constexpr int minSleepMs = 3000;
    constexpr float ratio = 10.0;
    // if 100 as fullpercent, then 0.3 per step
    constexpr int progressValueStep = static_cast<int>(0.3 * ratio);
    constexpr int maxProgressValue = static_cast<int>(100 * ratio);
    int progressvalueTmp = 0;
    if (GetFacade().GetMode() != UPDATERMODE_FACTORYRST && GetFacade().GetMode() != UPDATERMODE_REBOOTFACTORYRST) {
        return;
    }
    GetFacade().ShowProgress(0);
    while (progressvalueTmp <= maxProgressValue) {
        progressvalueTmp = progressvalueTmp + progressValueStep;
        GetFacade().ShowProgress(progressvalueTmp / ratio);
        Utils::UsSleep(minSleepMs);
        if (progressvalueTmp >= maxProgressValue) {
            Utils::UsSleep(maxSleepMs);
            return;
        }
    }
}

DEFINE_ASYN_CALLBACK(OnRebootEvt)
{
    LOG(INFO) << "On Label Reboot";
    GraphicEngine::GetInstance().StopEngine();
    PostUpdater(false);
    Utils::UpdaterDoReboot("", "Updater reboot btn event");
}

DEFINE_SYNC_CALLBACK(OnLabelResetEvt)
{
    LOG(INFO) << "On Label Reset";
    if (!GetFacade().SetMode(UPDATERMODE_FACTORYRST)) {
        return;
    }
    GetFacade().ShowFactoryConfirmPage();
}

DEFINE_ASYN_CALLBACK(OnLabelSDCardEvt)
{
    LOG(INFO) << "On Label SDCard";
    if (!GetFacade().SetMode(UPDATERMODE_SDCARD)) {
        return;
    }
    Utils::UsSleep(CALLBACK_DELAY);
    GetFacade().ClearText();
    GetFacade().ShowProgress(0);
    GetFacade().ShowLog(TR(LOG_SDCARD_NOTMOVE));
    Utils::UsSleep(DISPLAY_TIME);
    UpdaterParams upParams;
    upParams.updateMode = SDCARD_UPDATE;
    if (UpdaterFromSdcard(upParams) != UPDATE_SUCCESS) {
        GetFacade().ShowMainpage();
        return;
    }
    PostUpdater(true);
    Utils::UpdaterDoReboot("", "Updater sdcard update success reboot");
}

DEFINE_ASYN_CALLBACK(OnLabelSDCardNoDelayEvt)
{
    LOG(INFO) << "On Label SDCard No Delay";
    if (!GetFacade().SetMode(UPDATERMODE_SDCARD)) {
        return;
    }
    Utils::UsSleep(CALLBACK_DELAY);
    UpdaterParams upParams;
    upParams.updateMode = SDCARD_UPDATE;
    UPDATER_UI_INSTANCE.ShowProgressPage();
    if (auto res = UpdaterFromSdcard(upParams); res != UPDATE_SUCCESS) {
        Utils::RemoveUpdateInfoFromMisc("sdcard_update");
        GetFacade().ShowLogRes(res == UPDATE_CORRUPT ? TR(LOGRES_VERIFY_FAILED) : TR(LOGRES_UPDATE_FAILED));
        GetFacade().ShowFailedPage();
        return;
    }
    GetFacade().ShowLogRes(TR(LABEL_UPD_OK_DONE));
    GetFacade().ShowSuccessPage();
    Utils::UsSleep(SUCCESS_DELAY);
    PostUpdater(true);
    Utils::UpdaterDoReboot("", "Updater sdcard update success reboot");
}

DEFINE_ASYN_CALLBACK(OnLabelSDUpdateResEvt)
{
    LOG(INFO) << "On Label SDCard To Reserve Userdata";
    if (!GetFacade().SetMode(UPDATERMODE_SDCARD)) {
        return;
    }
    Utils::UsSleep(CALLBACK_DELAY);
    UpdaterParams upParams;
    upParams.updateMode = SDCARD_UPDATE;
    UPDATER_UI_INSTANCE.ShowProgressPage();
    Utils::SetMessageToMisc("boot-updater", 0, "sdcard_intral_update"); // set retain userdata
    if (!Utils::CheckUpdateMode(Updater::SDCARD_INTRAL_MODE)) {
        LOG(ERROR) << "sdcard_intral_update write to misc failed";
        GetFacade().ShowFailedPage();
        return;
    }
    LOG(INFO) << "sdcard_intral_update write to misc success";
    if (auto res = UpdaterFromSdcard(upParams); res != UPDATE_SUCCESS) {
        Utils::RemoveUpdateInfoFromMisc("sdcard_update");
        GetFacade().ShowLogRes(res == UPDATE_CORRUPT ? TR(LOGRES_VERIFY_FAILED) : TR(LOGRES_UPDATE_FAILED));
        GetFacade().ShowFailedPage();
        return;
    }
    GetFacade().ShowLogRes(TR(LABEL_UPD_OK_DONE));
    GetFacade().ShowSuccessPage();
    Utils::UsSleep(SUCCESS_DELAY);
    PostUpdater(true);
    Utils::UpdaterDoReboot("", "Updater sdcard update success reboot");
}

DEFINE_SYNC_CALLBACK(OnLabelCancelEvt)
{
    LOG(INFO) << "On Label Cancel";
    PageManager::GetInstance().GoBack();
}

DEFINE_SYNC_CALLBACK(OnReturnToMainEvt)
{
    LOG(INFO) << "On Return To Main";
    PageManager::GetInstance().ShowMainPage();
}

DEFINE_ASYN_CALLBACK(OnLabelOkEvt)
{
    LOG(INFO) << "On Label Ok";
    Utils::UsSleep(CALLBACK_DELAY);
    GetFacade().ShowMainpage();
    GetFacade().ClearText();
    GetFacade().ShowLog(TR(LOG_WIPE_DATA));
    if (!GetFacade().SetMode(UPDATERMODE_FACTORYRST)) {
        return;
    }
    GetFacade().ShowProgress(0);
    GetFacade().ShowProgressPage();
    DoProgress();
    if (FactoryReset(USER_WIPE_DATA, "/data") == 0) {
        GetFacade().ShowLog(TR(LOG_WIPE_DONE));
        GetFacade().ShowSuccessPage();
    } else {
        GetFacade().ShowLog(TR(LOG_WIPE_FAIL));
        GetFacade().ShowFailedPage();
    }
}

DEFINE_ASYN_CALLBACK(OnConfirmRstEvt)
{
    LOG(INFO) << "On Label Ok";
    if (!GetFacade().SetMode(UPDATERMODE_FACTORYRST)) {
        return;
    }
    Utils::AddUpdateInfoToMisc("user_wipe_data", std::nullopt);
    GetFacade().ShowUpdInfo(TR(LABEL_RESET_PROGRESS_INFO));
    GetFacade().ShowProgressPage();
    DoProgress();
    if (FactoryReset(USER_WIPE_DATA, "/data") != 0) {
        Utils::RemoveUpdateInfoFromMisc("user_wipe_data");
        GetFacade().ShowLogRes(TR(LOG_WIPE_FAIL));
        GetFacade().ShowFailedPage();
    } else {
        GetFacade().ShowSuccessPage();
        PostUpdater(true);
        Utils::UsSleep(SUCCESS_DELAY);
        Utils::UpdaterDoReboot("", "Updater factory reset success");
    }
}

DEFINE_ASYN_CALLBACK(OnMenuShutdownEvt)
{
    LOG(DEBUG) << "shutdown";
    GraphicEngine::GetInstance().StopEngine();
    Utils::DoShutdown("Updater shutdown btn event");
}

DEFINE_ASYN_CALLBACK(OnMenuClearCacheEvt)
{
    LOG(INFO) << "On clear cache";
    GetFacade().ClearText();
    if (!GetFacade().SetMode(UPDATERMODE_FACTORYRST)) {
        return;
    }
    Utils::UsSleep(CALLBACK_DELAY);
    GetFacade().ShowUpdInfo(TR(LOG_CLEAR_CAHCE));
    GetFacade().ShowProgressPage();
    ClearMisc();
    DoProgress();
    GetFacade().ShowMainpage();
}
} // namespace Updater
