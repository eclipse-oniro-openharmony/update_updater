/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "fs_manager/mount.h"
#include "flashd/flashd.h"
#include "log/log.h"
#include "misc_info/misc_info.h"
#include "updater/updater_const.h"
#include "updater/updater.h"
#include "updater_main.h"
#include "utils.h"

using namespace Updater;

namespace Updater {
std::tuple<std::vector<BootMode> &, BootMode &> GetBootModes(void)
{
    constexpr int defaultModeIdx = 0;
    static std::vector<BootMode> bootModes {
        { IsUpdater, "UPDATER", "updater.hdc.configfs", Updater::UpdaterMain },
        { IsFlashd, "FLASHD", "updater.flashd.configfs", Flashd::flashd_main }
    };
    return {bootModes, bootModes[defaultModeIdx]};
}

void AddMode(const BootMode &mode)
{
    std::get<0>(GetBootModes()).push_back(mode);
}
}

int main(int argc, char **argv)
{
    const auto &[modes, defaultMode] = GetBootModes();

    struct UpdateMessage boot {};
    // read from misc
    if (!ReadUpdaterMiscMsg(boot)) {
        // read misc failed, default enter updater mode
        defaultMode.InitMode();
        LOG(INFO) << "enter updater Mode";
        return defaultMode.entryFunc(argc, argv);
    }

    auto it = std::find_if(modes.begin(), modes.end(), [&boot] (const auto &bootMode) {
        return bootMode.cond != nullptr && bootMode.cond(boot);
    });

	// misc check failed for each mode, then enter updater mode
    if (it == modes.end() || it->entryFunc == nullptr) {
        defaultMode.InitMode();
        LOG(INFO) << "find valid mode failed, enter updater Mode";
        return defaultMode.entryFunc(argc, argv);
    }
    it->InitMode();
    LOG(INFO) << "enter " << it->modeName << " mode";
    return it->entryFunc(argc, argv);
}
