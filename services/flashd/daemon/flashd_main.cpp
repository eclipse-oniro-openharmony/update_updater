/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
#include "daemon_common.h"
#include "daemon_updater.h"
#include "flashd/flashd.h"
#include "updater/updater.h"
#include "updater_main.h"

using namespace Hdc;
namespace Flashd {
int flashd_main(int argc, char **argv)
{
    Base::SetLogLevel(LOG_LAST);  // debug log print
    Base::SetLogCache(false);
    UpdaterInit::GetInstance().InvokeEvent(FLAHSD_PRE_INIT_EVENT);

    std::vector<std::string> args = Updater::ParseParams(argc, argv);
    bool enableUsb = false;
    bool enableTcp = false;
    WRITE_LOG(LOG_DEBUG, "flashd main run %d", argc);
    const int size = 64;
    char modeSet[size] = "";
    WRITE_LOG(LOG_DEBUG, "Background mode, persist.hdc.mode %s", modeSet);
    for (std::string arg : args) {
        if (arg.find("-l") != std::string::npos) {
            int logLevel = atoi(arg.c_str() + strlen("-l"));
            if (logLevel < 0 || logLevel > LOG_LAST) {
                logLevel = LOG_LAST;
            }
            Base::SetLogLevel(logLevel);
        } else if (arg.find("-t") != std::string::npos || strncmp(modeSet, "tcp", 3) == 0) { // 3 tcp
            enableTcp = true;
        } else if (arg.find("-u") != std::string::npos) {
            enableUsb = true;
        }
    }

    if (!enableTcp && !enableUsb) {
        Base::PrintMessage("Both TCP and USB are disable, default enable usb");
        enableUsb = true;
    }
    HdcDaemon daemon(false);
    daemon.InitMod(enableTcp, enableUsb);
#ifndef UPDATER_UT
    daemon.WorkerPendding();
#endif
    return 0;
}
}
