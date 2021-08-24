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
#include "updater_ui.h"
#include <cstdio>
#include "animation_label.h"
#include "frame.h"
#include "input_event.h"
#include "log/log.h"
#include "progress_bar.h"
#include "securec.h"
#include "surface_dev.h"
#include "updater_main.h"
#include "updater_ui_const.h"
#include "utils.h"
#include "view.h"

namespace updater {
using utils::String2Int;

constexpr int LABEL_HEIGHT = 13;
constexpr int MAX_IMGS = 62;
constexpr int DIALIG_COLOR_A = 0xAA;
constexpr int DIALOG_COLOR = 0x00;
constexpr int DISPLAY_TIME = 1000 * 1000;

int g_updateFlag = 0;
int g_textLabelNum = 0;

Frame *g_menuFrame;
Frame *g_updateFrame;
TextLabel *g_textLabel0;
TextLabel *g_textLabel2;
TextLabel *g_textLabel3;
TextLabel *g_logLabel;
TextLabel *g_logResultLabel;
TextLabel *g_updateInfoLabel;
AnimationLable *g_anmimationLabel;
ProgressBar *g_progressBar;

TextLabel *g_dialogTitle;
TextLabel *g_dialogNote;
TextLabel *g_dialogNoteNext;
TextLabel *g_dialogCancalBtn;
TextLabel *g_dialogOkBtn;
SurfaceDev *g_sfDev;

static void ClearText()
{
    if (g_logLabel != nullptr) {
        g_logLabel->SetText("");
    }
    if (g_logResultLabel != nullptr) {
        g_logResultLabel->SetText("");
    }
    if (g_updateInfoLabel != nullptr) {
        g_updateInfoLabel->SetText("");
    }
}

void ShowText(TextLabel *label, std::string text)
{
    if (label != nullptr) {
        ClearText();
        label->SetText(text.c_str());
    }
}

static void HideDialog()
{
    if (!g_menuFrame->IsVisiable()) {
        return;
    }
    if (g_dialogTitle != nullptr) {
        g_dialogTitle->Hide();
    }
    if (g_dialogNote != nullptr) {
        g_dialogNote->Hide();
    }
    if (g_dialogNoteNext != nullptr) {
        g_dialogNoteNext->Hide();
    }
    if (g_dialogCancalBtn != nullptr) {
        g_dialogCancalBtn->Hide();
    }
    if (g_dialogOkBtn != nullptr) {
        g_dialogOkBtn->Hide();
    }
}

static void ShowDialog()
{
    if (!g_menuFrame->IsVisiable()) {
        return;
    }
    if (g_dialogTitle != nullptr) {
        g_dialogTitle->Show();
    }
    if (g_dialogNote != nullptr) {
        g_dialogNote->Show();
    }
    if (g_dialogNoteNext != nullptr) {
        g_dialogNoteNext->Show();
    }
    if (g_dialogCancalBtn != nullptr) {
        g_dialogCancalBtn->Show();
    }
    if (g_dialogOkBtn != nullptr) {
        g_dialogOkBtn->Show();
    }
}

static void ShowMenu()
{
    if (g_menuFrame == nullptr) {
        return;
    }
    if (g_textLabel0 != nullptr) {
        g_textLabel0->Show();
    }
    if (g_textLabel2 != nullptr) {
        g_textLabel2->Show();
    }
    if (g_textLabel3 != nullptr) {
        g_textLabel3->Show();
    }
    g_menuFrame->Show();
}

static void HideMenu()
{
    if (g_menuFrame == nullptr) {
        return;
    }
    if (g_textLabel0 != nullptr) {
        g_textLabel0->Hide();
    }
    if (g_textLabel2 != nullptr) {
        g_textLabel2->Hide();
    }
    if (g_textLabel3 != nullptr) {
        g_textLabel3->Hide();
    }
    g_menuFrame->Hide();
}

void OnKeyEvent(int viewId)
{
    if (!g_menuFrame->IsVisiable()) {
        return;
    }
    ClearText();
    if (viewId == g_textLabel0->GetViewId() && g_textLabel0->IsVisiable()) {
        HideDialog();
        PostUpdater();
        utils::DoReboot("");
    } else if (viewId == g_textLabel2->GetViewId() && g_textLabel2->IsVisiable()) {
        ShowDialog();
    } else if (viewId == g_textLabel3->GetViewId() && g_textLabel3->IsVisiable()) {
        HideDialog();
        g_logLabel->SetText("Don't remove SD Card!");
        usleep(DISPLAY_TIME);
        UpdaterStatus status = UpdaterFromSdcard();
        if (status != UPDATE_SUCCESS) {
            ShowUpdateFrame(false);
            ShowMenu();
            return;
        }
        PostUpdater();
        utils::DoReboot("");
    } else if (viewId == g_dialogCancalBtn->GetViewId() && g_dialogCancalBtn->IsVisiable()) {
        HideDialog();
    } else if (viewId == g_dialogOkBtn->GetViewId() && g_dialogOkBtn->IsVisiable()) {
        HideDialog();
        HideMenu();
        g_logLabel->SetText("Wipe data");
        g_updateFlag = 1;
        ShowUpdateFrame(true);
        DoProgress();
        int ret = FactoryReset(USER_WIPE_DATA, "/data");
        if (ret != 0) {
            g_logLabel->SetText("Wipe data failed");
        } else {
            g_logLabel->SetText("Wipe data done");
        }
        ShowUpdateFrame(false);
        ShowMenu();
    }
}

void LoadImgs()
{
    for (int i = 0; i < MAX_IMGS; i++) {
        std::string nameBuf;
        if (i < LOOP_TOP_PICTURES) {
            nameBuf = "/resources/loop0000";
            nameBuf.append(std::to_string(i)).append(".png");
        } else {
            nameBuf = "/resources/loop000";
            nameBuf.append(std::to_string(i)).append(".png");
        }
        g_anmimationLabel->AddImg(nameBuf);
    }
}

void ShowUpdateFrame(bool isShow)
{
    const int sleepMs = 300 * 100;
    if (isShow) {
        g_menuFrame->Hide();
        g_updateInfoLabel->SetText("");
        g_updateFrame->Show();
        g_anmimationLabel->Start();
        return;
    }
    usleep(sleepMs);
    g_anmimationLabel->Stop();
    g_progressBar->Show();
    g_updateFrame->Hide();
    g_menuFrame->Show();
    g_updateFlag = 0;
}

void DoProgress()
{
    const int sleepMs = 300 * 1000;
    const int maxSleepMs = 1000 * 1000;
    const int progressValueStep = 10;
    const int maxProgressValue = 100;
    std::string progressValue;
    int progressvalueTmp = 0;
    if (!g_updateFlag) {
        return;
    }
    g_progressBar->SetProgressValue(0);
    while (progressvalueTmp <= maxProgressValue) {
        if (!(g_updateInfoLabel->IsVisiable()) || !(g_progressBar->IsVisiable()) ||
            !(g_updateFrame->IsVisiable())) {
            LOG(INFO) <<"is not visable in  updater_frame";
        }
        usleep(sleepMs);
        if (g_updateFlag == 1) {
            progressvalueTmp = progressvalueTmp + progressValueStep;
            g_progressBar->SetProgressValue(progressvalueTmp);
            if (progressvalueTmp >= maxProgressValue) {
                usleep(maxSleepMs);
                return;
            }
        }
    }
}

struct FocusInfo {
    bool focus;
    bool focusable;
};

struct Bold {
    bool top;
    bool bottom;
};

static void TextLabelInit(TextLabel *t, const std::string &text, struct Bold bold,
    struct FocusInfo focus, View::BRGA888Pixel color)
{
    if (t != nullptr) {
        t->SetText(text.c_str());
        t->SetOutLineBold(bold.top, bold.bottom);
        t->OnFocus(focus.focus);
        t->SetBackgroundColor(&color);
        t->SetFocusAble(focus.focusable);
    }
}

static void InitDialogButton(int height, int width, View::BRGA888Pixel bgColor)
{
    const int okStartY = 450;
    const int cancelNextStartY = 451;
    g_dialogOkBtn = new TextLabel(0, okStartY, width, DIALOG_OK_WIDTH, g_menuFrame);
    g_dialogOkBtn->Hide();
    struct FocusInfo info {false, false};
    struct Bold bold {false, false};
    g_dialogOkBtn->SetViewId(DIALOG_OK_ID);
    info = {false, false};
    bold = {false, false};
    TextLabelInit(g_dialogOkBtn, "Continue", bold, info, bgColor);
    if (!g_dialogOkBtn) {
        LOG(ERROR) << "g_dialogOkBtn is null";
        return;
    }
    g_dialogOkBtn->SetOnClickCallback(OnKeyEvent);

    g_dialogCancalBtn = new TextLabel(DIALOG_CANCEL_X, cancelNextStartY, DIALOG_OK_WIDTH, DIALOG_OK_WIDTH, g_menuFrame);
    g_dialogCancalBtn->Hide();
    info = {false, false};
    bold = {false, false};
    TextLabelInit(g_dialogCancalBtn, "Cancel", bold, info, bgColor);
    if (!g_dialogCancalBtn) {
        LOG(ERROR) << "g_dialogCancalBtn is null";
        return;
    }
    g_dialogCancalBtn->SetViewId(DIALOG_CANCEL_ID);
    g_dialogCancalBtn->SetOnClickCallback(OnKeyEvent);
}

static void DialogInit(int height, int width)
{
    if (g_menuFrame == nullptr) {
        LOG(ERROR) << "Frame is null";
        return;
    }
    const int titleHeight = 100;
    const int titleStartX = 250;
    const int noteStartY = 350;
    const int noteNextStartY = 400;
    View::BRGA888Pixel color;
    color.r = DIALOG_COLOR;
    color.g = DIALOG_COLOR;
    color.b = DIALOG_COLOR;
    color.a = DIALIG_COLOR_A;
    g_dialogTitle = new TextLabel(0, titleStartX, width, titleHeight, g_menuFrame);
    g_dialogTitle->SetTextAlignmentMethod(TextLabel::AlignmentMethod::ALIGN_CENTER,
        TextLabel::AlignmentMethod::ALIGN_CENTER);
    g_dialogTitle->Hide();
    struct FocusInfo info {false, false};
    struct Bold bold {false, false};
    TextLabelInit(g_dialogTitle, "Tip", bold, info, color);
    if (!g_dialogTitle) {
        LOG(ERROR) << "g_dialogTitle is null";
        return;
    }

    g_dialogNote = new TextLabel(0, noteStartY, width, HEIGHT4, g_menuFrame);
    g_dialogNote->Hide();
    info = {false, false};
    bold = {false, false};
    TextLabelInit(g_dialogNote, "Delete user date now...", bold, info, color);
    if (!g_dialogNote) {
        LOG(ERROR) << "g_dialogNote is null";
        return;
    }

    g_dialogNoteNext = new TextLabel(0, noteNextStartY, width, HEIGHT4, g_menuFrame);
    g_dialogNoteNext->Hide();
    info = {false, false};
    bold = {false, false};
    TextLabelInit(g_dialogNoteNext, "Do you want to continue?", bold, info, color);
    if (!g_dialogNoteNext) {
        LOG(ERROR) << "g_dialogNoteNext is null";
        return;
    }
    InitDialogButton(width, height, color);
}

static void MenuItemInit(int height, int width, View::BRGA888Pixel bgColor)
{
    if (g_menuFrame == nullptr) {
        LOG(ERROR) << "Frame is null";
        return;
    }
    g_textLabel0 = new TextLabel(0, height * LABEL0_OFFSET / LABEL_HEIGHT, width, height /
        LABEL_HEIGHT, g_menuFrame);
    struct FocusInfo info {false, true};
    struct Bold bold {true, false};
    TextLabelInit(g_textLabel0, "Reboot to normal system", bold, info, bgColor);
    if (!g_textLabel0) {
        LOG(ERROR) << "g_textLabel0 is null";
        return;
    }
    g_textLabel0->SetOnClickCallback(OnKeyEvent);
    g_textLabelNum++;

    g_textLabel2 = new TextLabel(0, height * LABEL1_OFFSET / LABEL_HEIGHT, width, height /
        LABEL_HEIGHT, g_menuFrame);
    info = {false, true};
    bold = {false, false};
    TextLabelInit(g_textLabel2, "Userdata reset", bold, info, bgColor);
    if (!g_textLabel2) {
        LOG(ERROR) << "g_textLabel2 is null";
        return;
    }
    g_textLabel2->SetOnClickCallback(OnKeyEvent);
    g_textLabelNum++;

    g_textLabel3 = new TextLabel(0, height * LABEL2_OFFSET / LABEL_HEIGHT, width, height /
        LABEL_HEIGHT, g_menuFrame);
    info = {false, true};
    bold = {false, true};
    TextLabelInit(g_textLabel3, "Update from SD Card", bold, info, bgColor);
    if (!g_textLabel3) {
        LOG(ERROR) << "g_textLabel3 is null";
        return;
    }
    g_textLabel3->SetOnClickCallback(OnKeyEvent);
    g_textLabelNum++;
    g_textLabel0->SetViewId(LABEL_ID_0);
    g_textLabel2->SetViewId(LABEL_ID_1);
    g_textLabel3->SetViewId(LABEL_ID_2);
}

void UpdaterUiInit()
{
    constexpr char alpha = 0xff;
    int screenH = 0;
    int screenW = 0;
    g_sfDev = new SurfaceDev(SurfaceDev::DevType::DRM_DEVICE);
    g_sfDev->GetScreenSize(screenW, screenH);
    View::BRGA888Pixel bgColor {0x00, 0x00, 0x00, alpha};

    g_menuFrame = new Frame(screenW, screenH, View::PixelFormat::BGRA888, g_sfDev);
    g_menuFrame->SetBackgroundColor(&bgColor);
    g_menuFrame->Hide();

    MenuItemInit(screenH, screenW, bgColor);
    DialogInit(screenH, screenW);

    g_logLabel = new TextLabel(START_X1, START_Y1, WIDTH1, HEIGHT1, g_menuFrame);
    struct FocusInfo info {false, false};
    struct Bold bold {false, false};
    TextLabelInit(g_logLabel, "", bold, info, bgColor);

    g_logResultLabel = new TextLabel(START_X2, START_Y2, WIDTH2, HEIGHT2, g_menuFrame);
    TextLabelInit(g_logResultLabel, "", bold, info, bgColor);

    g_updateFrame = new Frame(screenW, screenH, View::PixelFormat::BGRA888, g_sfDev);
    g_updateFrame->SetBackgroundColor(&bgColor);
    g_updateFrame->Hide();

    g_anmimationLabel = new AnimationLable(START_X_SCALE, START_Y_SCALE,
            screenW * WIDTH_SCALE1 / WIDTH_SCALE2, screenH >> 1, g_updateFrame);
    g_anmimationLabel->SetBackgroundColor(&bgColor);
    LoadImgs();
    g_progressBar = new ProgressBar(START_X3, START_Y3, WIDTH3, HEIGHT3, g_updateFrame);

    g_updateInfoLabel = new TextLabel(START_X5, START_Y5, screenW, HEIGHT5, g_updateFrame);
    g_updateInfoLabel->SetOutLineBold(false, false);
    g_updateInfoLabel->SetBackgroundColor(&bgColor);
    HdfInit();
}

void DeleteView()
{
    if (g_updateInfoLabel != nullptr) {
        delete g_updateInfoLabel;
    }
    if (g_anmimationLabel != nullptr) {
        delete g_anmimationLabel;
    }
    if (g_progressBar != nullptr) {
        delete g_progressBar;
    }
    if (g_dialogTitle != nullptr) {
        delete g_dialogTitle;
    }
    if (g_dialogNote != nullptr) {
        delete g_dialogNote;
    }
    if (g_dialogNoteNext != nullptr) {
        delete g_dialogNoteNext;
    }
    if (g_dialogCancalBtn != nullptr) {
        delete g_dialogCancalBtn;
    }
    if (g_dialogOkBtn != nullptr) {
        delete g_dialogOkBtn;
    }
    if (g_logResultLabel != nullptr) {
        delete g_logResultLabel;
    }
    if (g_textLabel0 != nullptr) {
        delete g_textLabel0;
    }
    if (g_textLabel2 != nullptr) {
        delete g_textLabel2;
    }
    if (g_textLabel3 != nullptr) {
        delete g_textLabel3;
    }
    if (g_logLabel == nullptr) {
        delete g_logLabel;
    }
    if (g_updateFrame != nullptr) {
        delete g_updateFrame;
    }
    if (g_menuFrame != nullptr) {
        delete g_menuFrame;
    }
    if (g_sfDev != nullptr) {
        delete g_sfDev;
    }
}
} // namespace updater
