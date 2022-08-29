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

#ifndef SUB_PAGE_H
#define SUB_PAGE_H

#include "base_page.h"
#include "page.h"
#include "view_api.h"

namespace Updater {
class SubPage : public Page {
    friend class Page;
public:
    [[nodiscard]] bool BuildSubPage(UxSubPageInfo &subpageInfo);
    std::string GetPageId() override;
    void SetVisible(bool isVisible) override;
    bool IsVisible() const override;
    OHOS::UIViewGroup *GetView() const override;
    bool IsValid() const override;
    bool IsValidCom(const std::string &id) const override;
    ViewProxy &operator[](const std::string &id) override;
    static bool IsPageInfoValid(const UxSubPageInfo &info);
private:
    SubPage();
    SubPage(const std::shared_ptr<Page> &basePage, const std::string &pageId);
    void Reset();
    std::shared_ptr<Page> basePage_;
    std::string pageId_;
    std::vector<std::string> comsId_;
    bool isVisible_;
    OHOS::ColorType color_;
};
} // namespace Updater
#endif
