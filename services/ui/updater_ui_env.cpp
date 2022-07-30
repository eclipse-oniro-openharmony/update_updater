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
#include "updater_ui_env.h"

#include <chrono>
#include <fstream>
#include <map>
#include <thread>
#include <unistd.h>
#include "callback_manager.h"
#include "common/graphic_startup.h"
#include "common/screen.h"
#include "components/root_view.h"
#include "graphic_engine.h"
#include "input_event.h"
#include "language/language_ui.h"
#include "log/log.h"
#include "page/page_manager.h"
#include "updater_ui_config.h"

namespace Updater {
namespace {
constexpr std::array BRIGHTNESS_FILES {
    std::pair { "/sys/class/leds/lcd_backlight0/brightness", "/sys/class/leds/lcd_backlight0/max_brightness" },
    std::pair { "/sys/class/leds/lcd-backlight/brightness", "/sys/class/leds/lcd-backlight/max_brightness" }
};

constexpr uint32_t WHITE_BGCOLOR = 0x000000ff;
}

UpdaterUiEnv &UpdaterUiEnv::GetInstance()
{
    static UpdaterUiEnv instance;
    return instance;
}

void UpdaterUiEnv::Init()
{
    if (isInited_) {
        return 0;
    }
    InitDisplayDriver(); // init display driver
    InitEngine(); // Graphic UI init
    InitPages(); // page manager init
    InitEvts(); // init input driver and input events callback
    InitInputDriver(); // init input driver and input events callback
    isInited_ = true;
}

void UpdaterUiEnv::InitEngine() const
{
    OHOS::GraphicStartUp::Init();
    UIGraphicEngine::GetInstance().Init(screenW_, screenH_, WHITE_BGCOLOR, OHOS::ColorMode::ARGB8888, *sfDev_);
    InitRootView();
    LOG(INFO) << "UxInitEngine done";
}

void UpdaterUiEnv::InitPages() const
{
    // load page layout, language resource, ui strategy
    if (!UpdaterUiConfig::Init()) {
        LOG(ERROR) << "page config failed";
        return;
    }

    // get main menu's id
    std::string_view mainPage = UpdaterUiConfig::GetMainPage();

    // get page infos
    std::vector<UxPageInfo> &pageInfos = UpdaterUiConfig::GetPageInfos();

    // build pages from pageInfo vector
    PageManager::GetInstance().Init(pageInfos, mainPage);
}

void UpdaterUiEnv::InitEvts() const
{
    CallbackManager::Init(UpdaterUiConfig::GetFocusCfg());
}

void UpdaterUiEnv::InitInputDriver() const
{
    HdfInit();
}

void UpdaterUiEnv::InitDisplayDriver()
{
    sfDev_ = std::make_unique<SurfaceDev>();
    sfDev_->GetScreenSize(screenW_, screenH_);
    std::find_if(std::begin(BRIGHTNESS_FILES), std::end(BRIGHTNESS_FILES), [this] (auto filePair) {
        return InitBrightness(filePair.first, filePair.second);
    });
}

void UpdaterUiEnv::InitRootView() const
{
    using namespace OHOS;
    OHOS::RootView::GetInstance()->SetPosition(0, 0);
    OHOS::RootView::GetInstance()->SetStyle(STYLE_BACKGROUND_COLOR, Color::Black().full);
    OHOS::RootView::GetInstance()->Resize(Screen::GetInstance().GetWidth(), Screen::GetInstance().GetHeight());
    OHOS::RootView::GetInstance()->Invalidate();
}

bool UpdaterUiEnv::InitBrightness(const char *brightnessFile, const char *maxBrightnessFile) const
{
    if (access(brightnessFile, R_OK | W_OK) != 0 || access(maxBrightnessFile, W_OK) != 0) {
        LOG(ERROR) << "can't access brigntness file";
        return false;
    }

    std::ifstream ifs { maxBrightnessFile };
    if (!ifs.is_open()) {
        LOG(ERROR) << "open " << maxBrightnessFile << " failed";
        return false;
    }
    int maxValue = 0;
    ifs >> maxValue;
    if (ifs.fail() || ifs.bad()) {
        LOG(ERROR) << "read int from " << maxBrightnessFile << " failed sd maxValue = " << maxValue;
        return false;
    }

    std::ofstream ofs { brightnessFile };
    if (!ofs.is_open()) {
        LOG(ERROR) << "open " << brightnessFile << " failed";
        return false;
    }

    constexpr std::size_t SHIFT_WIDTH = 3;
    // set to one eighth of max brigtness
    ofs << (static_cast<std::size_t>(maxValue) >> SHIFT_WIDTH);
    if (ofs.fail() || ofs.bad()) {
        LOG(ERROR) << "write int to " << brightnessFile << " failed";
        return false;
    }
    return true;
}
}
