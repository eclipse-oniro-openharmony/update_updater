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
#ifndef UPDATE_UI_LANGUAGE_UI_MSG_ID_H
#define UPDATE_UI_LANGUAGE_UI_MSG_ID_H

namespace Updater {
enum class UpdaterUiMsgID {
    LOG_SDCARD_NOTFIND = 0,
    LOG_SDCARD_ABNORMAL,
    LOG_NOPKG,
    LOG_SDCARD_NOTMOVE,
    LOG_SDCARD_FAIL,
    LOG_LOWPOWER,
    UPD_SETPART_FAIL,
    LOG_UPDFAIL,
    LOG_RETRY_INSTALL,
    LOGRES_FACTORY_FAIL,
    LOGRES_FACTORY_DONE,
    LOGRES_WIPE_FAIL,
    LOGRES_WIPE_FINISH,
    LOGRES_UPDATE_OK,
    LOGRES_UPDATE_FAILED,
    LOGRES_VERIFY_FAILED,
    LOG_WIPE_DATA,
    LOG_WIPE_FAIL,
    LOG_WIPE_DONE,
    LOG_CLEAR_CAHCE,
    UPD_VERIFYPKG,
    UPD_SPACE_NOTENOUGH,
    UPD_SDCARD_UPDATE,
    UPD_UPDFAIL,
    UPD_NOPKG,
    UPD_VERIFYFAIL,
    UPD_VERIFYPKGFAIL,
    UPD_OK,
    UPD_INSTALL_FAIL,
    UPD_INSTALL_START,
    LABEL_REBOOT_TO_NORMAL,
    LABEL_USER_DATA_RESET,
    LABEL_UPDATE_FROM_SDCARD,
    LABEL_TIP,
    LABEL_NOTE_DELETE_USERDATA,
    LABEL_NOTE_WHETHER_CONTINUE,
    LABEL_NOTE_CONTINUE,
    LABEL_NOTE_CANCEL,
    LABEL_UPD_LOGO,
    LABEL_UPD_INFO,
    LABEL_UPD_FAIL_TIP,
    LABEL_UPD_FAIL_REBOOT,
    LABEL_UPD_OK_DONE,
    LABEL_UPD_OK_REBOOT,
    LABEL_CLEAR_CACHE,
    LABEL_REBOOT_DEVICE,
    LABEL_REBOOT,
    LABEL_SAFE_MODE,
    LABEL_SHUTDOWN,
    LABEL_MENU_TITLE,
    LABEL_MENU_USAGE,
    LABEL_RESET_INFORM,
    LABEL_RESET_CONFIRM_TITLE,
    LABEL_UPD_SDCARD_INFO,
    LABEL_RESET_PROGRESS_INFO,
    DEFAULT_STRING,
};
} // namespace Updater
#endif /* UPDATE_UI_LANGUAGE_UI_MSG_ID_H */
