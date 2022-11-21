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
#include "view/page/view_proxy.h"
#include "view/component/label_btn_adapter.h"
#include "view/component/text_label_adapter.h"

using Updater::ViewProxy;
using Updater::TextLabelAdapter;
using Updater::LabelBtnAdapter;
using namespace testing::ext;

namespace {
class UpdaterUiViewProxyUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() override {}
    void TearDown() override {}
};

HWTEST_F(UpdaterUiViewProxyUnitTest, test_as_when_view_is_nullptr, TestSize.Level0)
{
    ViewProxy viewproxy {};

    std::string err;
    viewproxy.As(err);
    EXPECT_EQ(err, " view is null");
}

HWTEST_F(UpdaterUiViewProxyUnitTest, test_as_when_view_is_nullptr_with_custom_err_msg, TestSize.Level0)
{
    ViewProxy viewproxy { nullptr, "A" };

    std::string err;
    viewproxy.As(err);
    EXPECT_EQ(err, "A view is null");

    err = "";
    viewproxy.As<Updater::TextLabelAdapter>(err);
    EXPECT_EQ(err, "A view is null");
}

HWTEST_F(UpdaterUiViewProxyUnitTest, test_as_when_view_type_not_matched, TestSize.Level0)
{
    std::unique_ptr<OHOS::UIView> label = std::make_unique<TextLabelAdapter>();
    OHOS::UIView *real = label.get();
    ViewProxy viewproxy { std::move(label), "label1" };

    std::string err {};
    EXPECT_NE(viewproxy.As<LabelBtnAdapter>(err), real);
    EXPECT_EQ(err, "label1 view's real type not matched");

    err = "";
    EXPECT_EQ(viewproxy.As<TextLabelAdapter>(err), real);
    EXPECT_EQ(err, "");
}

HWTEST_F(UpdaterUiViewProxyUnitTest, test_as_without_err_out_param, TestSize.Level0)
{
    std::unique_ptr<OHOS::UIView> label = std::make_unique<TextLabelAdapter>();
    OHOS::UIView *real = label.get();
    ViewProxy viewproxy { std::move(label), "label1" };

    EXPECT_NE(viewproxy.As<LabelBtnAdapter>(), real);
    EXPECT_EQ(viewproxy.As<TextLabelAdapter>(), real);
}

HWTEST_F(UpdaterUiViewProxyUnitTest, test_operator_arrow, TestSize.Level0)
{
    OHOS::UIView *dummy = ViewProxy {}.operator->();

    ViewProxy viewproxy { std::make_unique<TextLabelAdapter>(), "label1" };
    EXPECT_NE(viewproxy.operator->(), dummy);
    EXPECT_EQ(ViewProxy {}.operator->(), dummy);
}
}