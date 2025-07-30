/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

template <typename T>
const typename T::SpecificInfoType &CastSpecificInfoAs(const UxViewSpecificInfo *specPtr)
{
    return static_cast<const SpecificInfoWrapper<T> *>(specPtr)->data;
}

template <typename T>
bool Equals(const std::unique_ptr<UxViewSpecificInfo> &lhs, const std::unique_ptr<UxViewSpecificInfo> &rhs)
{
    const UxViewSpecificInfo *lhsPtr = lhs.get();
    const UxViewSpecificInfo *rhsPtr = rhs.get();
    if (lhsPtr == rhsPtr) {
        return true;
    }
    if (lhsPtr == nullptr || rhsPtr == nullptr) {
        return false;
    }

    if (lhsPtr->GetType() != rhsPtr->GetType()) {
        return false;
    }
    if (lhsPtr->GetStructKey() != rhsPtr->GetStructKey()) {
        return false;
    }

    const typename T::SpecificInfoType &lhsRef = CastSpecificInfoAs<T>(lhsPtr);
    const typename T::SpecificInfoType &rhsRef = CastSpecificInfoAs<T>(rhsPtr);

    return lhsRef == rhsRef;
}

}
namespace {
class UpdaterUiLayoutParserUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() override;
    void TearDown() override;

protected:
    void LoadValidConfigs(const std::vector<std::string> &layoutFiles, std::vector<UxPageInfo> &pages);
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

void UpdaterUiLayoutParserUnitTest::LoadValidConfigs(
    const std::vector<std::string> &layoutFiles, std::vector<UxPageInfo> &pages)
{
    ASSERT_GE(layoutFiles.size(), 1UL);
    ASSERT_TRUE(pages.empty());
    EXPECT_TRUE(LayoutParser::GetInstance().LoadLayout(layoutFiles, pages));
    ASSERT_EQ(pages.size(), layoutFiles.size());
}

template <typename T>
UxViewInfo CreateViewInfo(UxViewCommonInfo commonInfo, typename T::SpecificInfoType specInfo)
{
    UxViewInfo info {commonInfo, ComponentFactory::CreateSpecificInfo(T::COMPONENT_TYPE)};
    EXPECT_NE(info.specificInfo.get(), nullptr);

    static_cast<SpecificInfoWrapper<T> *>(info.specificInfo.get())->data = specInfo;
    return info;
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_label_parser, TestSize.Level0)
{
    std::vector<UxPageInfo> pages;
    LoadValidConfigs({"/data/updater/ui/label.json"}, pages);
    UxPageInfo &pageInfo = pages[0];

    ASSERT_EQ(pageInfo.viewInfos.size(), 1UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedLabel =
        CreateViewInfo<TextLabelAdapter>(UxViewCommonInfo { 0, 0, 100, 100, "label_id_0", "UILabel", true },
        UxLabelInfo {10,
            "Reboot to normal system",
            "center",
            "#ffffffff",
            "#ffffffff",
            "normal",
            {"#ffffffff", "#ffffffff", false},
            false,
            "ellipsis"});
    EXPECT_EQ(pageInfo.viewInfos[0].commonInfo, expectedLabel.commonInfo);
    EXPECT_TRUE(Equals<TextLabelAdapter>(pageInfo.viewInfos[0].specificInfo, expectedLabel.specificInfo));
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_progress_parser, TestSize.Level0)
{
    std::vector<UxPageInfo> pages;
    LoadValidConfigs({"/data/updater/ui/boxprogress.json"}, pages);
    UxPageInfo &pageInfo = pages[0];

    ASSERT_EQ(pageInfo.viewInfos.size(), 1UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedProgress =
        CreateViewInfo<BoxProgressAdapter>(UxViewCommonInfo {0, 0, 100, 100, "box_progress_0", "UIBoxProgress", true },
            UxBoxProgressInfo {10, "#ffffffff", "#ffffffff" });
    EXPECT_EQ(pageInfo.viewInfos[0].commonInfo, expectedProgress.commonInfo);
    EXPECT_TRUE(Equals<BoxProgressAdapter>(pageInfo.viewInfos[0].specificInfo, expectedProgress.specificInfo));
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_image_view_parser, TestSize.Level0)
{
    std::vector<UxPageInfo> pages;
    LoadValidConfigs({"/data/updater/ui/imageview.json"}, pages);
    UxPageInfo &pageInfo = pages[0];

    ASSERT_EQ(pageInfo.viewInfos.size(), 1UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    UxViewInfo expectedImage = CreateViewInfo<ImgViewAdapter>(
        UxViewCommonInfo {0, 0, 100, 100, "image_view", "UIImageView", true}, UxImageInfo {"/res/images", "", 100, 1});
    EXPECT_EQ(pageInfo.viewInfos[0].commonInfo, expectedImage.commonInfo);
    EXPECT_TRUE(Equals<ImgViewAdapter>(pageInfo.viewInfos[0].specificInfo, expectedImage.specificInfo));
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_all, TestSize.Level1)
{
    std::vector<UxPageInfo> pages;
    LoadValidConfigs({"/data/updater/ui/all.json"}, pages);
    UxPageInfo &pageInfo = pages[0];

    ASSERT_EQ(pageInfo.viewInfos.size(), 3UL);
    EXPECT_EQ(pageInfo.id, "menu"s);

    {
        UxViewInfo expectedLabel =
            CreateViewInfo<TextLabelAdapter>(UxViewCommonInfo {0, 0, 100, 100, "label_id_0", "UILabel", true},
                UxLabelInfo {10,
                    "Reboot to normal system",
                    "center",
                    "#ffffffff",
                    "#ffffffff",
                    "normal",
                    {"#ffffffff", "#ffffffff", false},
                    false,
                    "ellipsis"});
        EXPECT_EQ(pageInfo.viewInfos[0].commonInfo, expectedLabel.commonInfo);
        EXPECT_TRUE(Equals<TextLabelAdapter>(pageInfo.viewInfos[0].specificInfo, expectedLabel.specificInfo));
    }

    {
        UxViewInfo expectedImage =
            CreateViewInfo<ImgViewAdapter>(UxViewCommonInfo {0, 0, 100, 100, "image_view", "UIImageView", true},
                UxImageInfo {"/res/images", "", 100, 1});
        EXPECT_EQ(pageInfo.viewInfos[1].commonInfo, expectedImage.commonInfo);
        EXPECT_TRUE(Equals<ImgViewAdapter>(pageInfo.viewInfos[1].specificInfo, expectedImage.specificInfo));
    }

    {
        UxViewInfo expectedProgress = CreateViewInfo<BoxProgressAdapter>(
            UxViewCommonInfo {0, 0, 100, 100, "box_progress_0", "UIBoxProgress", true},
            UxBoxProgressInfo {10, "#ffffffff", "#ffffffff", "", false});
        EXPECT_EQ(pageInfo.viewInfos[2].commonInfo, expectedProgress.commonInfo);
        EXPECT_TRUE(Equals<BoxProgressAdapter>(pageInfo.viewInfos[2].specificInfo, expectedProgress.specificInfo));
    }
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_load_multiple_page_info, TestSize.Level0)
{
    std::vector<UxPageInfo> pageInfos {};
    LoadValidConfigs({"/data/updater/ui/imageview.json", "/data/updater/ui/boxprogress.json"}, pageInfos);

    UxViewInfo expectedImage = CreateViewInfo<ImgViewAdapter>(
        UxViewCommonInfo {0, 0, 100, 100, "image_view", "UIImageView", true}, UxImageInfo {"/res/images", "", 100, 1});
    EXPECT_EQ(pageInfos[0].id, "menu");
    ASSERT_EQ(pageInfos[0].viewInfos.size(), 1ul);
    EXPECT_EQ(pageInfos[0].viewInfos[0].commonInfo, expectedImage.commonInfo);
    EXPECT_TRUE(Equals<ImgViewAdapter>(pageInfos[0].viewInfos[0].specificInfo, expectedImage.specificInfo));

    UxViewInfo expectedProgress =
        CreateViewInfo<BoxProgressAdapter>(UxViewCommonInfo {0, 0, 100, 100, "box_progress_0", "UIBoxProgress", true},
            UxBoxProgressInfo {10, "#ffffffff", "#ffffffff"});
    EXPECT_EQ(pageInfos[1].id, "menu");
    ASSERT_EQ(pageInfos[1].viewInfos.size(), 1ul);
    EXPECT_EQ(pageInfos[1].viewInfos[0].commonInfo, expectedProgress.commonInfo);
    EXPECT_TRUE(Equals<BoxProgressAdapter>(pageInfos[1].viewInfos[0].specificInfo, expectedProgress.specificInfo));
}

HWTEST_F(UpdaterUiLayoutParserUnitTest, test_load_sub_page_info, TestSize.Level0)
{
    std::vector<UxPageInfo> pageInfos {};
    LoadValidConfigs({"/data/updater/ui/subpage.json"}, pageInfos);

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
        EXPECT_FALSE(LayoutParser::GetInstance().LoadLayout(layoutFiles, pageInfos));
    }
}

class UpdaterUiMenuParserUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() override;
    void TearDown() override;

protected:
    struct TestMenuItem {
        UxViewCommonInfo common;
        UxLabelInfo label;

        friend bool operator==(const TestMenuItem &lhs, const TestMenuItem &rhs)
        {
            return (lhs.common == rhs.common) && (lhs.label == rhs.label);
        }
    };
    static bool GetMenuItem(size_t index, TestMenuItem &item);
    static TestMenuItem GetDefaultValue();

    inline static std::vector<UxPageInfo> pages_;
};

void UpdaterUiMenuParserUnitTest::SetUp(void)
{
    cout << "Updater Unit UpdaterUiMenuParserUnitTest Begin!" << endl;
    ASSERT_TRUE(LayoutParser::GetInstance().LoadLayout({"/data/updater/ui/menu.json"}, pages_));
}

void UpdaterUiMenuParserUnitTest::TearDown(void)
{
    cout << "Updater Unit UpdaterUiMenuParserUnitTest End!" << endl;
}

void UpdaterUiMenuParserUnitTest::SetUpTestCase()
{
    cout << "SetUpTestCase" << endl;
}

void UpdaterUiMenuParserUnitTest::TearDownTestCase()
{
    cout << "TearDownTestCase" << endl;
}

bool UpdaterUiMenuParserUnitTest::GetMenuItem(size_t index, UpdaterUiMenuParserUnitTest::TestMenuItem &item)
{
    const SpecificInfoWrapper<TextLabelAdapter> emptyItem {};

    if (pages_.size() != 1UL) {
        return false;
    }

    if (index >= pages_[0].viewInfos.size()) {
        return false;
    }

    UxViewInfo &refInfo = pages_[0].viewInfos[index];
    item.common = refInfo.commonInfo;

    if (refInfo.specificInfo == nullptr) {
        return false;
    }
    if (refInfo.specificInfo->GetType() != emptyItem.GetType()) {
        return false;
    }
    if (refInfo.specificInfo->GetStructKey() != emptyItem.GetStructKey()) {
        return false;
    }

    const UxLabelInfo &refLabel = CastSpecificInfoAs<TextLabelAdapter>(refInfo.specificInfo.get());
    item.label = refLabel;
    return true;
}

UpdaterUiMenuParserUnitTest::TestMenuItem UpdaterUiMenuParserUnitTest::GetDefaultValue()
{
    // the default node
    TestMenuItem item {.common = UxViewCommonInfo {.visible = true},
        .label = UxLabelInfo {.align = "center",
            .fontColor = "#ffffffff",
            .bgColor = "#000000ff",
            .style = "normal",
            .focusInfo =
                LabelFocusInfo {.focusedFontColor = "#ffffffff", .focusedBgColor = "#000000ff", .focusable = false},
            .touchable = false,
            .lineBreakMode = "ellipsis"}};
    return item;
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_basic, TestSize.Level0)
{
    ASSERT_EQ(pages_.size(), 1UL);
    ASSERT_EQ(pages_[0].viewInfos.size(), 8UL);
    EXPECT_EQ(pages_[0].id, "menu"s);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_00, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(0, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_RebootToNormalSystem";
    expected.common.x = 1280;
    expected.common.y = 0;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.common.visible = false;
    expected.label.text = "Reboot to normal system";
    expected.label.fontSize = 60;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_01, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(1, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_UserdataReset";
    expected.common.x = 1280;
    expected.common.y = 200;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Userdata reset";
    expected.label.fontSize = 60;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_02, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(2, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_UpdateFromSDCard";
    expected.common.x = 1280;
    expected.common.y = 400;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Update from SD Card";
    expected.label.fontSize = 60;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_03, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(3, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_MenuDialogTitle";
    expected.common.x = 1280;
    expected.common.y = 600;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Tip";
    expected.label.fontSize = 40;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_04, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(4, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_MenuDialogNote";
    expected.common.x = 1280;
    expected.common.y = 800;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Delete user date now...";
    expected.label.fontSize = 40;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_05, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(5, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_MenuDialogNext";
    expected.common.x = 1280;
    expected.common.y = 1000;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Do you want to continue?";
    expected.label.fontSize = 40;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_06, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(6, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_MenuDialogOK";
    expected.common.x = 1280;
    expected.common.y = 1200;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Continue";
    expected.label.fontSize = 40;

    EXPECT_EQ(item, expected);
}

HWTEST_F(UpdaterUiMenuParserUnitTest, test_menu_item_07, TestSize.Level0)
{
    TestMenuItem item;
    ASSERT_TRUE(GetMenuItem(7, item));

    TestMenuItem expected = GetDefaultValue();
    expected.common.type = "UILabel";
    expected.common.id = "Label_MenuDialogCancel";
    expected.common.x = 1680;
    expected.common.y = 1200;
    expected.common.w = 800;
    expected.common.h = 200;
    expected.label.text = "Cancel";
    expected.label.fontSize = 40;

    EXPECT_EQ(item, expected);
}

}  // namespace
