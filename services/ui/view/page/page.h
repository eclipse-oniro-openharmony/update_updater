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

#ifndef PAGE_H
#define PAGE_H

#include <string>
#include <unordered_set>
#include <vector>
#include "components/ui_view_group.h"
#include "view_api.h"
#include "view_proxy.h"

namespace Updater {
class Page {
    DISALLOW_COPY_MOVE(Page);
public:
    virtual ~Page() = default;
    virtual std::string GetPageId() = 0;
    virtual void SetVisible(bool isVisible) = 0;
    [[nodiscard]] virtual bool IsVisible() const = 0;
    [[nodiscard]] virtual OHOS::UIViewGroup *GetView() const = 0;
    [[nodiscard]] virtual bool IsValid() const = 0;
    [[nodiscard]] virtual bool IsValidCom(const std::string &id) const = 0;
    virtual ViewProxy &operator[](const std::string &id) = 0;
    void UpdateFocus(bool isVisible);
    template<typename T, typename ... Args>
    static std::shared_ptr<T> Create(Args && ... args)
    {
        static_assert(std::is_base_of_v<Page, T>, "T should be derived class of Page");
        return std::shared_ptr<T>(new(std::nothrow) T(std::forward<Args>(args)...));
    }
protected:
    OHOS::UIView *focusedView_;
    Page() : focusedView_ {nullptr} {}
};
} // namespace Updater
#endif
