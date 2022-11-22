/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "gtest/gtest.h"
#include "box_progress_adapter.h"
#include "dock/focus_manager.h"
#include "label_btn_adapter.h"
#include "text_label_adapter.h"

using namespace testing::ext;
using namespace Updater;

namespace {
class UpdaterUiComponentUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

constexpr static int32_t MAX_PROGRESS_VALUE = 100;
constexpr static int32_t MIN_PROGRESS_VALUE = 0;

void CheckCommInfo(OHOS::UIView &view, const UxViewCommonInfo &common)
{
    EXPECT_EQ(view.GetX(), common.x);
    EXPECT_EQ(view.GetY(), common.x);
    EXPECT_EQ(view.GetWidth(), common.w);
    EXPECT_EQ(view.GetHeight(), common.h);
    EXPECT_STREQ(view.GetViewId(), common.id.c_str());
    EXPECT_EQ(view.IsVisible(), common.visible);
}

HWTEST_F(UpdaterUiComponentUnitTest, test_box_progress_constructor, TestSize.Level0)
{
    UxBoxProgressInfo specInfo {50, "#ffffffff", "#000000ff", "", false};
    UxViewCommonInfo commonInfo {10, 10, 1000, 1000, "id", "UIBoxProgress", false};
    BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
    CheckCommInfo(boxProgress, commonInfo);

    auto fgColor = StrToColor(specInfo.fgColor);
    auto bgColor = StrToColor(specInfo.bgColor);
    EXPECT_EQ(boxProgress.GetBackgroundStyle(OHOS::STYLE_BACKGROUND_COLOR), bgColor.full);
    EXPECT_EQ(boxProgress.GetBackgroundStyle(OHOS::STYLE_BACKGROUND_OPA), bgColor.alpha);
    EXPECT_EQ(boxProgress.GetForegroundStyle(OHOS::STYLE_BACKGROUND_COLOR), fgColor.full);
    EXPECT_EQ(boxProgress.GetForegroundStyle(OHOS::STYLE_BACKGROUND_OPA), fgColor.alpha);
    EXPECT_EQ(boxProgress.GetValue(), specInfo.defaultValue);EXPECT_EQ(boxProgress.GetRangeMin(), 0);
    EXPECT_EQ(boxProgress.GetRangeMax(), commonInfo.w - 1);
}

HWTEST_F(UpdaterUiComponentUnitTest, test_box_progress_is_valid, TestSize.Level0)
{
    EXPECT_FALSE(BoxProgressAdapter::IsValid(UxBoxProgressInfo {101, "", "", "", false}));
    EXPECT_FALSE(BoxProgressAdapter::IsValid(UxBoxProgressInfo {50, "#", "", "", false}));
    EXPECT_FALSE(BoxProgressAdapter::IsValid(UxBoxProgressInfo {50, "#000000ff", "", "", false}));
    EXPECT_FALSE(BoxProgressAdapter::IsValid(UxBoxProgressInfo {50, "#000000ff", "#000000ff", "", true}));
    EXPECT_TRUE(BoxProgressAdapter::IsValid(UxBoxProgressInfo {50, "#000000ff", "#000000ff", "", false}));
}

HWTEST_F(UpdaterUiComponentUnitTest, test_box_progress_set_value_without_ep, TestSize.Level0)
{
    UxBoxProgressInfo specInfo {50, "#ffffffff", "#000000ff", "", false};
    UxViewCommonInfo commonInfo {10, 10, 1000, 1000, "id", "UIBoxProgress", false};
    {
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        constexpr int32_t validValue = 50;

        boxProgress.SetValue(-1);
        EXPECT_EQ(boxProgress.GetValue(), 0);
        boxProgress.SetValue(validValue);
        EXPECT_EQ(boxProgress.GetValue(), validValue / MAX_PROGRESS_VALUE * (commonInfo.w - 1));
        boxProgress.SetValue(MAX_PROGRESS_VALUE + 1);
        EXPECT_EQ(boxProgress.GetValue(), commonInfo.w - 1);
    }
    {
        specInfo.hasEp = true;
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        constexpr int32_t validValue = 50;
        boxProgress.SetValue(validValue);
        EXPECT_EQ(boxProgress.GetValue(), validValue / MAX_PROGRESS_VALUE * (commonInfo.w - 1));
    }
}

HWTEST_F(UpdaterUiComponentUnitTest, test_box_progress_set_visible_without_ep, TestSize.Level0)
{
    {
        UxViewInfo info {{10, 10, 1000, 1000, "id", "UIBoxProgress", false},
        UxBoxProgressInfo {50, "#ffffffff", "#000000ff", "", false}};
        BoxProgressAdapter boxProgress {info};
        boxProgress.SetVisible(true);
        EXPECT_TRUE(boxProgress.IsVisible());
    }
    {
        UxViewInfo info {{10, 10, 1000, 1000, "id", "UIBoxProgress", false},
        UxBoxProgressInfo {50, "#ffffffff", "#000000ff", "", true}};
        BoxProgressAdapter boxProgress {info};
        boxProgress.SetVisible(true);
        EXPECT_TRUE(boxProgress.IsVisible());
    }
}

HWTEST_F(UpdaterUiComponentUnitTest, test_box_progress_init_endpoint, TestSize.Level0)
{
    UxBoxProgressInfo specInfo {50, "#ffffffff", "#000000ff", "", false};
    UxViewCommonInfo commonInfo {10, 10, 1000, 1000, "id", "UIBoxProgress", false};
    {
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        EXPECT_TRUE(boxProgress.InitEp());
    }
    {
        specInfo.hasEp = true;
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        EXPECT_FALSE(boxProgress.InitEp());
    }
    {
        OHOS::UIViewGroup parent {};
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        parent.Add(&boxProgress);
        EXPECT_FALSE(boxProgress.InitEp());
    }
    {
        constexpr auto epId = "endpoint";
        specInfo.endPoint = epId;
        OHOS::UIViewGroup parent {};
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        LabelBtnAdapter labelBtn {};
        labelBtn.SetViewId(epId);
        parent.Add(&boxProgress);
        parent.Add(&labelBtn);
        EXPECT_FALSE(boxProgress.InitEp());
    }
    {
        constexpr auto epId = "endpoint";
        specInfo.endPoint = epId;
        OHOS::UIViewGroup parent {};
        BoxProgressAdapter boxProgress {UxViewInfo {commonInfo, specInfo}};
        ImgViewAdapter imgView {};
        imgView.SetViewId(epId);
        parent.Add(&boxProgress);
        parent.Add(&imgView);
        EXPECT_TRUE(boxProgress.InitEp());
    }
}

HWTEST_F(UpdaterUiComponentUnitTest, test_box_progress_with_ep, TestSize.Level0)
{
    constexpr auto epId = "endpoint";
    OHOS::UIViewGroup parent {};
    ImgViewAdapter epView {};
    BoxProgressAdapter boxProgress {UxViewInfo {UxViewCommonInfo {10, 10, 1000, 1000, "id", "UIBoxProgress", false},
        UxBoxProgressInfo {50, "#ffffffff", "#000000ff", epId, false}}};
    epView.SetViewId(epId);
    parent.Add(&boxProgress);
    parent.Add(&epView);

    boxProgress.SetVisible(true);
    EXPECT_TRUE(boxProgress.IsVisible());
    EXPECT_TRUE(epView.IsVisible());
    
    boxProgress.SetVisible(false);
    EXPECT_FALSE(boxProgress.IsVisible());
    EXPECT_FALSE(epView.IsVisible());

    constexpr float testValue = 50;
    constexpr float halfDivisor = 2.0;
    boxProgress.SetValue(testValue);
    float rate = static_cast<float>(boxProgress.GetValue()) / (boxProgress.GetRangeMax() - 1);
    EXPECT_EQ(epView.GetX(), static_cast<int16_t>(boxProgress.GetX() -
        epView.GetWidth() / halfDivisor + boxProgress.GetWidth() * rate));
    EXPECT_EQ(epView.GetY(), static_cast<int16_t>(boxProgress.GetY() -
        epView.GetHeight() / halfDivisor + boxProgress.GetHeight() * rate));
}

HWTEST_F(UpdaterUiComponentUnitTest, test_label_btn_adapter_constructor, TestSize.Level0)
{
    constexpr auto labelText = "hello";
    UxLabelBtnInfo specInfo {100, "hello", "#ffffffff", "#000000ff", "#000000ff", "#ffffffff", true};
    UxViewCommonInfo commonInfo {0, 0, 0, 0, "id", "UILabelButton", false};
    UxViewInfo info {commonInfo, specInfo};
    LabelBtnAdapter labelBtn {info};
    CheckCommInfo(labelBtn, commonInfo);

    auto fontColor = StrToColor(specInfo.txtColor);
    auto bgColor = StrToColor(specInfo.bgColor);
    EXPECT_EQ(labelBtn.GetStyle(OHOS::STYLE_TEXT_COLOR), fontColor.full);
    EXPECT_EQ(labelBtn.GetStyle(OHOS::STYLE_TEXT_OPA), fontColor.alpha);
    EXPECT_EQ(labelBtn.GetStyle(OHOS::STYLE_BACKGROUND_COLOR), bgColor.full);
    EXPECT_EQ(labelBtn.GetStyle(OHOS::STYLE_BACKGROUND_OPA), bgColor.alpha);
    EXPECT_EQ(labelBtn.IsFocusable(), specInfo.focusable);
    EXPECT_STREQ(labelBtn.GetText(), labelText);
}

HWTEST_F(UpdaterUiComponentUnitTest, test_label_btn_adapter_on_press, TestSize.Level0)
{
    LabelBtnAdapter labelBtn1 {UxViewInfo {0, 0, 50, 50, "id", "UILabel", false,
        UxLabelBtnInfo {100, "", "#000000ff", "#ffffffff", "#ffffffff", "#000000ff", true}}};
    LabelBtnAdapter labelBtn2 {UxViewInfo {100, 100, 50, 50, "id", "UILabel", false,
        UxLabelBtnInfo {100, "", "#000000ff", "#ffffffff", "#ffffffff", "#000000ff", true}}};
    OHOS::FocusManager::GetInstance()->RequestFocus(&labelBtn2);
    labelBtn1.OnPressEvent(OHOS::PressEvent {OHOS::Point {}});
    EXPECT_EQ(OHOS::FocusManager::GetInstance()->GetFocusedView(), &labelBtn1);
    EXPECT_EQ(labelBtn1.GetLabelStyle(OHOS::STYLE_TEXT_COLOR), OHOS::ColorType(0xff, 0xff, 0xff, 0xff).full);
    EXPECT_EQ(labelBtn2.GetLabelStyle(OHOS::STYLE_TEXT_COLOR), OHOS::ColorType(0, 0, 0, 0xff).full);
    EXPECT_EQ(labelBtn1.GetStyle(OHOS::STYLE_BACKGROUND_COLOR), OHOS::ColorType(0, 0, 0, 0xff).full);
    EXPECT_EQ(labelBtn2.GetStyle(OHOS::STYLE_BACKGROUND_COLOR), OHOS::ColorType(0xff, 0xff, 0xff, 0xff).full);
}

HWTEST_F(UpdaterUiComponentUnitTest, test_label_btn_adapter_is_valid, TestSize.Level0)
{
    EXPECT_FALSE(LabelBtnAdapter::IsValid(UxLabelBtnInfo {201, "", "", "", "", "", false}));
    EXPECT_FALSE(LabelBtnAdapter::IsValid(UxLabelBtnInfo {100, "", "#000000ff", "", "", "", false}));
    EXPECT_FALSE(LabelBtnAdapter::IsValid(UxLabelBtnInfo {100, "", "#000000ff", "#000000ff", "", "", false}));
    EXPECT_FALSE(LabelBtnAdapter::IsValid(UxLabelBtnInfo {100, "", "#000000ff",
        "#000000ff", "#000000ff", "", false}));
    EXPECT_TRUE(LabelBtnAdapter::IsValid(UxLabelBtnInfo {100, "", "#000000ff",
        "#000000ff", "#000000ff", "#000000ff", false}));
}

HWTEST_F(UpdaterUiComponentUnitTest, test_label_btn_adapter_set_text, TestSize.Level0)
{
    LabelBtnAdapter labelBtn {UxViewInfo {0, 0, 0, 0, "id", "UILabel", false,
        UxLabelBtnInfo {100, "", "#000000ff", "#000000ff", "#000000ff", "#000000ff", false}}};
    constexpr auto testString = "test text";
    labelBtn.SetText(testString);
    EXPECT_STREQ(labelBtn.GetText(), testString);

    labelBtn.SetText("*");
    EXPECT_STREQ(labelBtn.GetText(), testString);

    labelBtn.SetText("");
    EXPECT_STREQ(labelBtn.GetText(), "");
}

HWTEST_F(UpdaterUiComponentUnitTest, test_text_label_adapter_is_info_valid, TestSize.Level0)
{
    EXPECT_FALSE(TextLabelAdapter::IsValid(UxLabelInfo {255, "", "", "", ""}));
    EXPECT_FALSE(TextLabelAdapter::IsValid(UxLabelInfo {255, "", "", "#000000ff", ""}));
    EXPECT_TRUE(TextLabelAdapter::IsValid(UxLabelInfo {255, "", "", "#000000ff", "#000000ff"}));
}

HWTEST_F(UpdaterUiComponentUnitTest, test_text_label_adapter_constructor, TestSize.Level0)
{
    constexpr auto labelText = "hello";
    UxLabelInfo specInfo {100, "hello", "center", "#000000ff", "#000000ff"};
    UxViewCommonInfo commonInfo {0, 0, 0, 0, "id", "UILabel", false};
    UxViewInfo info {commonInfo, specInfo};
    TextLabelAdapter textLabel {info};
    CheckCommInfo(textLabel, commonInfo);

    auto fontColor = StrToColor(specInfo.fontColor);
    auto bgColor = StrToColor(specInfo.bgColor);
    EXPECT_EQ(textLabel.GetHorAlign(), GetAlign(specInfo.align));
    EXPECT_EQ(textLabel.GetStyle(OHOS::STYLE_TEXT_COLOR), fontColor.full);
    EXPECT_EQ(textLabel.GetStyle(OHOS::STYLE_TEXT_OPA), fontColor.alpha);
    EXPECT_EQ(textLabel.GetStyle(OHOS::STYLE_BACKGROUND_COLOR), bgColor.full);
    EXPECT_EQ(textLabel.GetStyle(OHOS::STYLE_BACKGROUND_OPA), bgColor.alpha);
    EXPECT_STREQ(textLabel.GetText(), labelText);
}

HWTEST_F(UpdaterUiComponentUnitTest, test_text_label_adapter_set_text, TestSize.Level0)
{
    TextLabelAdapter textLabel {UxViewInfo {0, 0, 0, 0, "id", "UILabel", false,
        UxLabelInfo {255, "", "", "#000000ff", "#000000ff"}}};
    constexpr auto testString = "test text";
    textLabel.SetText(testString);
    EXPECT_STREQ(textLabel.GetText(), testString);

    textLabel.SetText("*");
    EXPECT_STREQ(textLabel.GetText(), testString);

    textLabel.SetText("");
    EXPECT_STREQ(textLabel.GetText(), "");
}
}