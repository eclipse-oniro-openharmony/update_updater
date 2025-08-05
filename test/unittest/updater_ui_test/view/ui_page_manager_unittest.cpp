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

#include <utility>
#include "gtest/gtest.h"
#include "ui_test_graphic_engine.h"
#include "view/component/component_register.h"
#include "view/component/img_view_adapter.h"
#include "view/component/text_label_adapter.h"
#include "view/page/page_manager.h"

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
std::string ToString(const std::vector<std::string> &vec)
{
    if (vec.empty()) {
        return "{}";
    }
    std::string res = "{";
    for (auto &str : vec) {
        res += str + ",";
    }
    res.pop_back();
    res += "}";
    return res;
}

template <typename T>
UxViewInfo CreateViewInfo(UxViewCommonInfo commonInfo, typename T::SpecificInfoType specInfo)
{
    UxViewInfo info {commonInfo, ComponentFactory::CreateSpecificInfo(T::COMPONENT_TYPE)};
    EXPECT_NE(info.specificInfo.get(), nullptr);
    static_cast<SpecificInfoWrapper<T> *>(info.specificInfo.get())->data = specInfo;
    return info;
}

class UpdaterUiPageManagerUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        TestGraphicEngine::GetInstance();
    }
    static void TearDownTestCase(void) {}
    void SetUp() override
    {
        RegisterComponents();
        pageInfos_ = MakeUxPages();
    }
    void TearDown() override
    {
        pageInfos_.clear();
    }

protected:
    static std::vector<UxPageInfo> MakeUxPages();

private:
    static UxPageInfo MakeTestPage1();
    static UxPageInfo MakeTestPage2();
    static UxPageInfo MakeTestPage3();

protected:
    constexpr static const char *MAIN_PAGE_ID = "page1";
    inline static std::vector<Updater::UxPageInfo> pageInfos_;
};

inline PageManager &GetInstance()
{
    return PageManager::GetInstance();
}

constexpr std::array VALID_COMINFOS {
    std::pair {"page1", "label_id_0"}, std::pair {"page1", "image_view"},
    std::pair {"page1:subpage1", "label_id_0"}, std::pair {"page1:subpage1", "image_view"},
    std::pair {"page1:subpage2", "image_view"}, std::pair {"page1:subpage3", "label_id_0"},
    std::pair {"page2", "label_id_0"}, std::pair {"page2", "image_view"},
    std::pair {"page2:subpage1", "label_id_0"}, std::pair {"page2:subpage2", "image_view"},
    std::pair {"page3", "label_id_0"}, std::pair {"page3", "image_view"}
};

UxPageInfo UpdaterUiPageManagerUnitTest::MakeTestPage1()
{
    UxPageInfo page1;
    page1.id = "page1";
    page1.bgColor = "#000000ff";
    page1.viewInfos.emplace_back(
        CreateViewInfo<TextLabelAdapter>(
            UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
            UxLabelInfo {50, "this is page1", "center", "#ff0000ff", "#000000ff", "normal",
                {"#ff0000ff", "#000000ff", false}, false, "ellipsis"}));
    // just test page manger, image need not to exist
    page1.viewInfos.emplace_back(
        CreateViewInfo<ImgViewAdapter>(
            UxViewCommonInfo {300, 700, 400, 400, "image_view", "UIImageView", false},
            UxImageInfo {"/resources/img1", "empty", 100, 0}));
    page1.subpages.emplace_back(UxSubPageInfo {"subpage1", "#000000ff", {"label_id_0", "image_view"}});
    page1.subpages.emplace_back(UxSubPageInfo {"subpage2", "#000000ff", {"image_view"}});
    page1.subpages.emplace_back(UxSubPageInfo {"subpage3", "#000000ff", {"label_id_0"}});
    return page1;
}

UxPageInfo UpdaterUiPageManagerUnitTest::MakeTestPage2()
{
    UxPageInfo page2;
    page2.id = "page2";
    page2.bgColor = "#ffffffff";
    page2.viewInfos.emplace_back(
        CreateViewInfo<TextLabelAdapter>(
            UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
            UxLabelInfo {50, "this is page2", "center", "#00ff00ff", "#000000ff", "normal",
                {"#00ff00ff", "#000000ff", false}, false, "ellipsis"}));
    // just test page manger, image need not to exist
    page2.viewInfos.emplace_back(
        CreateViewInfo<ImgViewAdapter>(
            UxViewCommonInfo {300, 700, 400, 400, "image_view", "UIImageView", false},
            UxImageInfo {"/resources/img2", "empty", 100, 0}));
    page2.subpages.emplace_back(UxSubPageInfo {"subpage1", "#000000ff", {"label_id_0"}});
    page2.subpages.emplace_back(UxSubPageInfo {"subpage2", "#000000ff", {"image_view"}});
    return page2;
}

UxPageInfo UpdaterUiPageManagerUnitTest::MakeTestPage3()
{
    UxPageInfo page3;
    page3.id = "page3";
    page3.bgColor = "#000000ff";
    page3.viewInfos.emplace_back(
        CreateViewInfo<TextLabelAdapter>(
            UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
            UxLabelInfo {50, "this is page2", "center", "#0000ffff", "#000000ff", "normal",
                {"#0000ffff", "#000000ff", false}, false, "ellipsis"}));
    // just test page manger, image need not to exist
    page3.viewInfos.emplace_back(
        CreateViewInfo<ImgViewAdapter>(
            UxViewCommonInfo {300, 700, 400, 400, "image_view", "UIImageView", false},
            UxImageInfo {"/resources/img2", "empty", 100, 0}));
    return page3;
}

std::vector<UxPageInfo> UpdaterUiPageManagerUnitTest::MakeUxPages()
{
    std::vector<UxPageInfo> pages;

    pages.emplace_back(MakeTestPage1());
    pages.emplace_back(MakeTestPage2());
    pages.emplace_back(MakeTestPage3());
    return pages;
}

inline bool CheckResult(const std::vector<std::string> &expected, std::vector<std::string> &&checked)
{
    std::sort(checked.begin(), checked.end());
    EXPECT_TRUE(expected == checked) << ToString(expected) << "!=" << ToString(checked);
    return expected == checked;
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_invalid_page_id, TestSize.Level0)
{
    // invalid page id
    std::vector<UxPageInfo> invalidPageId;
    invalidPageId.emplace_back(UxPageInfo {"", "#000000ff", {}, {}});
    EXPECT_FALSE(GetInstance().Init(invalidPageId, ""));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_duplicate_page_id, TestSize.Level0)
{
    // duplicate page id
    std::vector<UxPageInfo> duplicatePageIds;
    duplicatePageIds.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    duplicatePageIds.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    EXPECT_FALSE(GetInstance().Init(duplicatePageIds, "page1"));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_invalid_page_view, TestSize.Level0)
{
    // invalid page's view info
    std::vector<UxPageInfo> invalidPageView;
    invalidPageView.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    invalidPageView[0].viewInfos.emplace_back(CreateViewInfo<TextLabelAdapter>(
            UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
            UxLabelInfo {50, "this is page1", "center", "#ff0ff", "#0", "normal",
                {"#ff0ff", "#0", false}, false, "ellipsis"}));
    EXPECT_FALSE(GetInstance().Init(invalidPageView, "page1"));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_invalid_subpage_id, TestSize.Level0)
{
    GetInstance().Reset();
    // invalid subpage id
    std::vector<UxPageInfo> invalidSubpageId;
    invalidSubpageId.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    invalidSubpageId[0].viewInfos.emplace_back(
        CreateViewInfo<TextLabelAdapter>(
                UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
                UxLabelInfo {50, "this is page1", "center", "#000000ff", "#000000ff", "normal",
                    {"#000000ff", "#000000ff", false}, false, "ellipsis"}));
    invalidSubpageId[0].subpages.emplace_back(UxSubPageInfo {"", "#000000ff", {"label_id_0"}});
    EXPECT_FALSE(GetInstance().Init(invalidSubpageId, "page1"));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_duplicate_subpage_ids, TestSize.Level0)
{
    // duplicate subpage id
    std::vector<UxPageInfo> duplicateSubpageIds;
    duplicateSubpageIds.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    duplicateSubpageIds[0].viewInfos.emplace_back(
        CreateViewInfo<TextLabelAdapter>(
                UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
                UxLabelInfo {50, "this is page1", "center", "#000000ff", "#000000ff", "normal",
                    {"#000000ff", "#000000ff", false}, false, "ellipsis"}));
    duplicateSubpageIds[0].subpages.emplace_back(UxSubPageInfo {"subpage1", "#000000ff", {"label_id_0"}});
    duplicateSubpageIds[0].subpages.emplace_back(UxSubPageInfo {"subpage1", "#000000ff", {"label_id_0"}});
    EXPECT_FALSE(GetInstance().Init(duplicateSubpageIds, "page1"));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_invalid_subpage_component_id, TestSize.Level0)
{
    // invalid subpage's component id
    std::vector<UxPageInfo> invalidSubpageComponentId;
    invalidSubpageComponentId.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    invalidSubpageComponentId[0].viewInfos.emplace_back(
        CreateViewInfo<TextLabelAdapter>(
                UxViewCommonInfo {300, 400, 600, 200, "label_id_0", "UILabel", true},
                UxLabelInfo {50, "this is page1", "center", "#000000ff", "#000000ff", "normal",
                    {"#000000ff", "#000000ff", false}, false, "ellipsis"}));
    invalidSubpageComponentId[0].subpages.emplace_back(UxSubPageInfo {"subpage1", "#000000ff", {"invalid_id"}});
    EXPECT_FALSE(GetInstance().Init(invalidSubpageComponentId, "page1"));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_init_failed_invalid_main_page_id, TestSize.Level0)
{
    GetInstance().Reset();

    // invalid main page id
    std::vector<UxPageInfo> invalidMainPageId;
    invalidMainPageId.emplace_back(UxPageInfo {"page1", "#000000ff", {}, {}});
    invalidMainPageId.emplace_back(UxPageInfo {"page2", "#000000ff", {}, {}});
    invalidMainPageId.emplace_back(UxPageInfo {"page3", "#000000ff", {}, {}});

    EXPECT_FALSE(GetInstance().Init(invalidMainPageId, "page"));
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_is_valid_com, TestSize.Level0)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    for (auto [pageId, comId] : VALID_COMINFOS) {
        EXPECT_TRUE(GetInstance().IsValidCom(ComInfo {pageId, comId})) << pageId << "[" << comId << "] is invalid";
    }
    const std::array inValidCominfos {
        ComInfo {"page1", "invalid"}, ComInfo {"invalid", "image_view"}, ComInfo {"page1:subpage2", "label_id_0"}
    };
    for (auto cominfo : inValidCominfos) {
        EXPECT_FALSE(GetInstance().IsValidCom(cominfo)) << cominfo.pageId << "[" << cominfo.comId << "] is valid";
    }
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_operator_subscript_for_pages, TestSize.Level0)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    auto *dummyPage = &GetInstance()[""];
    for (auto pageId : { "page1", "page2", "page3", "page1:subpage1",  "page1:subpage2",  "page1:subpage3",
        "page2:subpage1", "page2:subpage2"}) {
        EXPECT_NE(&GetInstance()[pageId], dummyPage);
    }
    EXPECT_EQ(&GetInstance()["nonexist"], dummyPage);
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_operator_subscript_for_coms, TestSize.Level0)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    
    std::string errMsg {};
    for (auto [pageId, comId] : { std::pair {"page1", "label_id_0"}, std::pair {"page1:subpage1", "label_id_0"},
        std::pair {"page1:subpage3", "label_id_0"}, std::pair {"page2", "label_id_0"},
        std::pair {"page2:subpage1", "label_id_0"}, std::pair {"page3", "label_id_0"}}) {
            GetInstance()[ComInfo {pageId, comId}].As<TextLabelAdapter>(errMsg);
            ASSERT_EQ(errMsg, "") << errMsg;
        }
    for (auto [pageId, comId] : { std::pair {"page1", "image_view"}, std::pair {"page1:subpage1", "image_view"},
        std::pair {"page1:subpage2", "image_view"}, std::pair {"page2", "image_view"},
        std::pair {"page2:subpage2", "image_view"}, std::pair {"page3", "image_view"}}) {
            GetInstance()[ComInfo {pageId, comId}].As<ImgViewAdapter>(errMsg);
            ASSERT_EQ(errMsg, "") << errMsg;
        }
    GetInstance()[ComInfo {"", ""}].As(errMsg);
    EXPECT_EQ(errMsg, " view is null");
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_after_init_success, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    EXPECT_TRUE(CheckResult(GetInstance().Report(), {}));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_main_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page2");
    GetInstance().ShowMainPage();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { MAIN_PAGE_ID }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page2");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_non_exist_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowMainPage();
    GetInstance().ShowPage("nonexist");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { MAIN_PAGE_ID }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_switch_between_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page2");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));
    GetInstance().ShowPage("page3");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page3" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_sub_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page2:subpage1");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2:subpage1", "page2" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_non_exist_sub_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page2");
    GetInstance().ShowPage("page2:subpage3");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_switch_between_sub_pages, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page2");
    GetInstance().ShowPage("page1:subpage1");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page1:subpage1", "page1" }));

    GetInstance().ShowPage("page2:subpage2");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2:subpage2", "page2" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_switch_between_sub_page_and_page, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowPage("page1:subpage1");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page1:subpage1", "page1" }));

    GetInstance().ShowPage("page2");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));

    GetInstance().ShowPage("page1:subpage2");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page1:subpage2", "page1" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_go_back, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowMainPage();
    GetInstance().ShowPage("page2");
    GetInstance().ShowPage("page3");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page3" }));

    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));

    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { MAIN_PAGE_ID }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_same_page_and_goback, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowMainPage();
    GetInstance().ShowPage("page1");
    GetInstance().ShowPage("page2");
    GetInstance().ShowPage("page2");
    GetInstance().ShowPage("page3");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page3" }));

    // don't show and enqueue page showing currently
    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));

    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page1" }));
    GetInstance().Reset();
}

HWTEST_F(UpdaterUiPageManagerUnitTest, test_page_manager_show_more_page_than_queue_limit, TestSize.Level1)
{
    ASSERT_EQ(GetInstance().Init(pageInfos_, MAIN_PAGE_ID), true);
    GetInstance().ShowMainPage();
    GetInstance().ShowPage("page3");
    GetInstance().ShowPage("page1");
    GetInstance().ShowPage("page2");
    GetInstance().ShowPage("page3");
    GetInstance().ShowPage("page2");
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));

    // Don't show and enqueue page showing currently
    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page3" }));

    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page2" }));

    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page1" }));

    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page3" }));

    // max queue size is 4, when bigger than 4, queue tail will be pop, so can't show page3 here
    GetInstance().GoBack();
    EXPECT_TRUE(CheckResult(GetInstance().Report(), { "page3" }));
    GetInstance().Reset();
}
}  // namespace
