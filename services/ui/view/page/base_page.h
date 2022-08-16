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

#ifndef BASE_PAGE_H
#define BASE_PAGE_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "page.h"
#include "view_api.h"

namespace Updater {
class BasePage : public Page {
public:
    BasePage();
    BasePage(int16_t width, int16_t height);
    virtual ~BasePage() = default;
    void BuildPage(const UxPageInfo &pageInfo);
    std::string &GetPageId() override;
    void SetVisible(bool isVisible) override;
    bool IsVisible() const override;
    const std::unique_ptr<OHOS::UIViewGroup> &GetView() const override;
    bool IsValid() const override;
    bool IsValidCom(const std::string &id) const override;
    ViewProxy &operator[](const std::string &id) override;
    static bool IsPageInfoValid(const UxPageInfo &pageInfo);
private:
    void BuildComs(const UxPageInfo &pageInfo);
    void BuildCom(const UxViewInfo &viewInfo, int &minY);
    int16_t width_;
    int16_t height_;
    std::unique_ptr<OHOS::UIViewGroup> root_;
    std::vector<std::unique_ptr<ViewProxy>> coms_;
    std::unordered_map<std::string_view, ViewProxy *> comsMap_;
    std::string pageId_;
    OHOS::UIView::ViewExtraMsg extraMsg_;
    UxBRGAPixel color_;
};
} // namespace Updater
#endif
