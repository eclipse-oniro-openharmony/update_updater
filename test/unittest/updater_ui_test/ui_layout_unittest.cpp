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

#include <fstream>
#include <vector>
#include "file_ex.h"
#include "gtest/gtest.h"
#include "layout/layout_parser.h"
#include "view_api.h"

using namespace Updater;
using namespace std;
using namespace testing::ext;
namespace Fs = std::filesystem;
namespace Updater {
bool operator == (const UxViewCommonInfo &lhs, const UxViewCommonInfo &rhs)
{
    return std::tie(lhs.x, lhs.y, lhs.w, lhs.h, lhs.id, lhs.type, lhs.visible) ==
        std::tie(rhs.x, rhs.y, rhs.w, rhs.h, rhs.id, rhs.type, rhs.visible);
}

bool operator == (const UxBoxProgressInfo &lhs, const UxBoxProgressInfo &rhs)
{
    return std::tie(lhs.defaultValue, lhs.fgColor, lhs.bgColor, lhs.hasEp, lhs.endPoint) ==
        std::tie(rhs.defaultValue, rhs.fgColor, rhs.bgColor, rhs.hasEp, rhs.endPoint);
}

bool operator == (const UxLabelInfo &lhs, const UxLabelInfo &rhs)
{
    return std::tie(lhs.text, lhs.bgColor, lhs.align, lhs.fontColor, lhs.fontSize) ==
        std::tie(rhs.text, rhs.bgColor, rhs.align, rhs.fontColor, rhs.fontSize);
}

bool operator == (const UxImageInfo &lhs, const UxImageInfo &rhs)
{
    return std::tie(lhs.imgCnt, lhs.updInterval, lhs.resPath, lhs.filePrefix) ==
        std::tie(rhs.imgCnt, rhs.updInterval, rhs.resPath, rhs.filePrefix);
}

bool operator == (const UxLabelBtnInfo &lhs, const UxLabelBtnInfo &rhs)
{
    return std::tie(lhs.fontSize, lhs.text, lhs.txtColor, lhs.bgColor, lhs.focusedBgColor, lhs.focusedTxtColor) ==
        std::tie(rhs.fontSize, rhs.text, rhs.txtColor, rhs.bgColor, rhs.focusedBgColor, rhs.focusedTxtColor);
}

bool operator == (const UxViewInfo &lhs, const UxViewInfo &rhs)
{
    return lhs.commonInfo == rhs.commonInfo && lhs.specificInfo == rhs.specificInfo;
}
}
namespace {
class UpdaterUiLayoutParserUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() override;
    void TearDown() override;
};


// do something at the each function begining
void UpdaterUiLayoutParserUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdaterUiLayoutParserUnitTest Begin!" << endl;
}

// do something at the each function end
void UpdaterUiLayoutParserUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdaterUiLayoutParserUnitTest End!" << endl;
}

// init
void UpdaterUiLayoutParserUnitTest::SetUpTestCase(void)
{
    cout << "SetUpTestCase" << endl;
}

// end
void UpdaterUiLayoutParserUnitTest::TearDownTestCase(void)
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_label_parser, TestSize.Level0)
{
    UxPageInfo pageInfo;
    LayoutParser::GetInstance().LoadLayout("/data/updater/ui/label.json", pageInfo);
    ASSERT_EQ(pageInfo.viewInfos.size(), 1UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedLabel { UxViewCommonInfo { 0, 0, 100, 100, "label_id_0", "UILabel", true },
        UxLabelInfo {
        10,
        "Reboot to normal system",
        "center",
        "#ffffffff",
        "#ffffffff"
    } };
    EXPECT_EQ(pageInfo.viewInfos[0], expectedLabel);
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_progress_parser, TestSize.Level0)
{
    UxPageInfo pageInfo;
    LayoutParser::GetInstance().LoadLayout("/data/updater/ui/boxprogress.json", pageInfo);
    ASSERT_EQ(pageInfo.viewInfos.size(), 1UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedProgress { UxViewCommonInfo { 0, 0, 100, 100, "box_progress_0", "UIBoxProgress", true },
        UxBoxProgressInfo { 10, "#ffffffff", "#ffffffff" } };
    EXPECT_EQ(pageInfo.viewInfos[0], expectedProgress);
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_image_view_parser, TestSize.Level0)
{
    UxPageInfo pageInfo;
    LayoutParser::GetInstance().LoadLayout("/data/updater/ui/imageview.json", pageInfo);
    ASSERT_EQ(pageInfo.viewInfos.size(), 1UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedImage { UxViewCommonInfo { 0, 0, 100, 100, "image_view", "UIImageView", true },
        UxImageInfo { "/res/images", "", 100, 1 } };
    EXPECT_EQ(pageInfo.viewInfos[0], expectedImage);
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_all, TestSize.Level1)
{
    UxPageInfo pageInfo;
    LayoutParser::GetInstance().LoadLayout("/data/updater/ui/all.json", pageInfo);
    ASSERT_EQ(pageInfo.viewInfos.size(), 3UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedLabel { UxViewCommonInfo { 0, 0, 100, 100, "label_id_0", "UILabel", true },
        UxLabelInfo {
        10,
        "Reboot to normal system",
        "center",
        "#ffffffff",
        "#ffffffff"
    } };
    EXPECT_EQ(pageInfo.viewInfos[0], expectedLabel);

    UxViewInfo expectedImage { UxViewCommonInfo { 0, 0, 100, 100, "image_view", "UIImageView", true },
        UxImageInfo { "/res/images", "", 100, 1 } };
    EXPECT_EQ(pageInfo.viewInfos[1], expectedImage);

    UxViewInfo expectedProgress { UxViewCommonInfo { 0, 0, 100, 100, "box_progress_0", "UIBoxProgress",
        true },
        UxBoxProgressInfo { 10, "#ffffffff", "#ffffffff", "", false } };
    EXPECT_EQ(pageInfo.viewInfos[2], expectedProgress);
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_all_default, TestSize.Level0)
{
    UxPageInfo pageInfo;
    LayoutParser::GetInstance().LoadLayout("/data/updater/ui/menu.json", pageInfo);

    ASSERT_EQ(pageInfo.viewInfos.size(), 8UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expected { UxViewCommonInfo { 1280, 0, 800, 200, "Label_RebootToNormalSystem", "UILabel", true },
        UxLabelInfo {
        60,
        "Reboot to normal system",
        "center",
        "#ffffffff",
        "#000000ff"
    } };
    expected.commonInfo.visible = false;
    EXPECT_EQ(expected.commonInfo, pageInfo.viewInfos[0].commonInfo);
    EXPECT_EQ(expected.specificInfo, pageInfo.viewInfos[0].specificInfo);
    EXPECT_EQ(expected, pageInfo.viewInfos[0]);

    expected.commonInfo = { 1280, 200, 800, 200, "Label_UserdataReset", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Userdata reset";
    EXPECT_EQ(expected, pageInfo.viewInfos[1]);

    expected.commonInfo = { 1280, 400, 800, 200, "Label_UpdateFromSDCard", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Update from SD Card";
    EXPECT_EQ(expected, pageInfo.viewInfos[2]);

    expected.commonInfo = { 1280, 600, 800, 200, "Label_MenuDialogTitle", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Tip";
    std::get<UxLabelInfo>(expected.specificInfo).fontSize = 40;
    EXPECT_EQ(expected, pageInfo.viewInfos[3]);

    expected.commonInfo = { 1280, 800, 800, 200, "Label_MenuDialogNote", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Delete user date now...";
    EXPECT_EQ(expected, pageInfo.viewInfos[4]);

    expected.commonInfo = { 1280, 1000, 800, 200, "Label_MenuDialogNext", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Do you want to continue?";
    EXPECT_EQ(expected, pageInfo.viewInfos[5]);

    expected.commonInfo = { 1280, 1200, 800, 200, "Label_MenuDialogOK", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Continue";
    EXPECT_EQ(expected, pageInfo.viewInfos[6]);

    expected.commonInfo = { 1680, 1200, 800, 200, "Label_MenuDialogCancel", "UILabel", true };
    std::get<UxLabelInfo>(expected.specificInfo).text = "Cancel";
    EXPECT_EQ(expected, pageInfo.viewInfos[7]);
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_load_multiple_page_info, TestSize.Level0)
{
    std::vector<std::string> layoutFiles { "/data/updater/ui/imageview.json", "/data/updater/ui/boxprogress.json" };
    std::vector<UxPageInfo> pageInfos {};
    LayoutParser::GetInstance().LoadLayout(layoutFiles, pageInfos);

    ASSERT_EQ(pageInfos.size(), 2ul);
    UxViewInfo expectedImage { UxViewCommonInfo { 0, 0, 100, 100, "image_view", "UIImageView", true },
        UxImageInfo { "/res/images", "", 100, 1 } };
    EXPECT_EQ(pageInfos[0].id, "menu");
    ASSERT_EQ(pageInfos[0].viewInfos.size(), 1ul);
    EXPECT_EQ(pageInfos[0].viewInfos[0], expectedImage);

    UxViewInfo expectedProgress { UxViewCommonInfo { 0, 0, 100, 100, "box_progress_0", "UIBoxProgress", true },
        UxBoxProgressInfo { 10, "#ffffffff", "#ffffffff" } };
    EXPECT_EQ(pageInfos[1].id, "menu");
    ASSERT_EQ(pageInfos[1].viewInfos.size(), 1ul);
    EXPECT_EQ(pageInfos[1].viewInfos[0].commonInfo, expectedProgress.commonInfo);
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_load_sub_page_info, TestSize.Level0)
{
    std::vector<std::string> layoutFiles { "/data/updater/ui/subpage.json" };
    std::vector<UxPageInfo> pageInfos {};
    LayoutParser::GetInstance().LoadLayout(layoutFiles, pageInfos);

    ASSERT_EQ(pageInfos.size(), 1ul);
    auto &subPages = pageInfos[0].subpages;
    ASSERT_EQ(subPages.size(), 1UL);
    EXPECT_EQ(subPages[0].id, "subpage1");
    EXPECT_EQ(subPages[0].coms.size(), 3UL);
    EXPECT_EQ(subPages[0].coms[0], "a");
    EXPECT_EQ(subPages[0].coms[1], "b");
    EXPECT_EQ(subPages[0].coms[2], "c");
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_invalid_cases, TestSize.Level0)
{
    constexpr std::array files { "/data/updater/ui/noPageInfo.json", "/data/updater/ui/noComs.json",
        "/data/updater/ui/comsNoType.json", "/data/updater/ui/commonInvalid.json",
        "/data/updater/ui/typeInvalid.json", "/data/updater/ui/incompleteComInfo.json"};
    for (auto file : files) {
        std::vector<std::string> layoutFiles { file };
        std::vector<UxPageInfo> pageInfos {};
        EXPECT_EQ(true, OHOS::FileExists(file)) << file;
        EXPECT_EQ(false, LayoutParser::GetInstance().LoadLayout(layoutFiles, pageInfos));
    }
}
} // namespace
