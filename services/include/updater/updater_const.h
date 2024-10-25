/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef UPDATER_CONST_H
#define UPDATER_CONST_H

#include <string>

namespace Updater {
constexpr const char *INSTALL_TIME_FILE = "install_time";
constexpr const char *COMMAND_FILE = "/data/updater/command";
constexpr const char *TMP_LOG = "/tmp/updater.log";
constexpr const char *TMP_STAGE_LOG = "/tmp/updater_stage.log";
constexpr const char *TMP_ERROR_CODE_PATH = "/tmp/error_code.log";
constexpr const char *ERROR_CODE_PATH = "/data/updater/log/error_code.log";
constexpr const char *UPDATER_LOG_DIR = "/data/updater/log";
constexpr const char *UPDATER_LOG = "/data/updater/log/updater_log";
constexpr const char *SYS_INSTALLER_LOG = "/data/updater/log/sys_installer.log";
constexpr const char *UPDATER_STAGE_LOG = "/data/updater/log/updater_stage_log";
constexpr const char *UPDATER_LOG_FILE = "updater_log";
constexpr const char *UPDATER_STAGE_FILE = "updater_stage_log";
constexpr const char *UPDATER_PATH = "/data/updater";
constexpr const char *SYS_INSTALLER_PATH = "/mnt/sys_installer";
constexpr const char *UPDATER_LOCALE_FILE = "locale";
constexpr const char *UPDATER_RESULT_FILE = "updater_result";
constexpr const char *MODULE_UPDATE_RESULT_FILE = "module_update_result";
constexpr const char *MISC_FILE = "/dev/block/platform/soc/10100000.himci.eMMC/by-name/misc";
constexpr const char *MISC_PATH = "/misc";
constexpr const char *UPDATER_BINARY = "updater_binary";
constexpr const char *SDCARD_PATH = "/sdcard";
constexpr const char *UPDATER_HDC_LOG = "/data/updater/log/flashd_hdc.log";
constexpr const char *PREFIX_UFS_NODE = "/dev/block/sd";
constexpr const char *SDCARD_PACKAGE_SUFFIX = ".zip";
constexpr const char *POWEROFF = "shutdown";
constexpr const char *BOOTDEV_TYPE = "/proc/bootdevice/type";
constexpr const char *UPLOAD_LOG_TIME_FILE = "upload_time";

// update mode
constexpr const char *SDCARD_MODE = "sdcard_update";
constexpr const char *OTA_MODE = "update_package";
constexpr const char *USB_MODE = "usb_update";
constexpr const char *SDCARD_INTRAL_MODE = "sdcard_intral_update";
constexpr const char *UPDATRE_SCRIPT_ZIP = "/etc/updater_script.zip";
constexpr const char *FACTORY_INTERNAL_MODE = "factory_internal_update";

// sd update ext mode
constexpr const char *SDCARD_NORMAL_UPDATE = "sdUpdate";
constexpr const char *SDCARD_UPDATE_FROM_DEV = "sdUpdateFromDev";
constexpr const char *SDCARD_MAINIMG = "mainUpdate";
constexpr const char *SDCARD_UPDATE_FROM_DATA = "sdUpdateFromData";

#ifndef UPDATER_UT
constexpr const char *SDCARD_CARD_PATH = "/sdcard/updater";
constexpr const char *SDCARD_CARD_PKG_PATH = "/sdcard/updater/updater.zip";
constexpr const char *DEFAULT_LOCALE = "en-US";
constexpr const char *G_WORK_PATH = "/tmp/";
constexpr const char *MMC_BLOCK_DEV_NAME = "/dev/block/mmcblk0";
constexpr const char *MMC_SIZE_FILE = "/sys/class/block/mmcblk0/size";
#else
constexpr const char *SDCARD_CARD_PATH = "/data/sdcard/updater";
constexpr const char *SDCARD_CARD_PKG_PATH = "/data/updater/updater/updater_full.zip";
constexpr const char *G_WORK_PATH = "/data/updater/src/";
constexpr const char *UT_VERSION = "OpenHarmony 3.2.9.1";
constexpr const char *MMC_BLOCK_DEV_NAME = "/data/block/mmcblk0";
constexpr const char *MMC_SIZE_FILE = "/data/class/block/mmcblk0/size";
#endif

// update retry
constexpr const char *UPDATER_RETRY_TAG = "retry_update";
constexpr const char *VERIFY_FAILED_REBOOT = "reboot_verify_failed";
constexpr const char *IO_FAILED_REBOOT = "reboot_IO_failed";

constexpr int MAX_RETRY_COUNT = 3;
constexpr int MINIMAL_ARGC_LIMIT = 2;
constexpr int MAXIMAL_ARGC_LIMIT = 4;
constexpr int MAX_LOG_BUF_SIZE = 4096;
constexpr int MAX_LOG_NAME_SIZE = 100;
constexpr long MAX_LOG_SIZE = 2 * 1024 * 1024;
constexpr long MAX_LOG_DIR_SIZE = 10 * 1024 * 1024;
constexpr int MAX_RESULT_SIZE = 20;
constexpr int MAX_RESULT_BUFF_SIZE = 1000;
constexpr int VERIFICATION_PROGRESS_TIME = 60;
constexpr float VERIFICATION_PROGRESS_FRACTION = 0.25f;
constexpr int PREDICTED_ELAPSED_TIME = 30;
constexpr int PROGRESS_VALUE_MAX = 1;
constexpr int PROGRESS_VALUE_MIN = 0;
constexpr int SLEEP_TIME = 1;
constexpr int DECIMAL = 10;
constexpr int BINARY = 2;
constexpr int FAKE_TEMPRATURE = 40;
constexpr int32_t DEFAULT_PROCESS_NUM = 2;
constexpr int32_t MAX_BUFFER_SIZE = 512;
constexpr int32_t DEFAULT_PIPE_NUM = 2;
constexpr int32_t BINARY_MAX_ARGS = 3;
constexpr int32_t BINARY_SECOND_ARG = 2;
constexpr int32_t WRITE_SECOND_CMD = 2;
constexpr int REBOOT = 0;
constexpr int WIPE_DATA = 1;
constexpr int WIPE_CACHE = 2;
constexpr int FACTORY_RESET = 3;
constexpr int REBOOT_FASTBOOT = 4;
constexpr int UPDATE_FROM_SDCARD = 5;
constexpr int SHUTDOWN = 6;
constexpr int FULL_PERCENT_PROGRESS = 100;
constexpr int PROGRESS_VALUE_CONST = 2;
constexpr int SHOW_FULL_PROGRESS_TIME = 2000;
constexpr unsigned int LEAST_PARTITION_COUNT = 4;
constexpr unsigned int UI_SHOW_DURATION = 2000;
constexpr unsigned int INTERVAL_TIME = 300;
constexpr float EPSINON = 0.00001;
constexpr float FULL_EPSINON = 1;
} // namespace Updater
#endif
