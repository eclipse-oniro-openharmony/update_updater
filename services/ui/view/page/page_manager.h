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

#ifndef PAGE_MANANGER_H
#define PAGE_MANANGER_H

#include <filesystem>
#include <stack>
#include <string_view>
#include <vector>
#include "base_page.h"
#include "macros.h"
#include "page.h"
#include "updater_ui.h"
#include "view_api.h"

namespace Updater {
class PageManager final {
    DISALLOW_COPY_MOVE(PageManager);
public:
    static PageManager &GetInstance();
    bool Init(std::vector<UxPageInfo> &pageInfos, std::string_view entry);
    void ShowPage(const std::string &id);
    void GoBack();
    bool IsValidCom(const ComInfo &pageComId) const;
    Page &operator[](const std::string &id) const;
    void ShowMainPage();
    ViewProxy &operator[](const ComInfo &comInfo) const;
    void Reset();
#ifdef UPDATER_UT
    std::vector<std::string> Report();
#endif
private:
    PageManager() = default;
    ~PageManager() = default;
    bool InitImpl(UxPageInfo &pageInfo, std::string_view entry);
    void EnQueuePage(const std::shared_ptr<Page> &page);
    bool BuildSubPages(const std::string &pageId, const std::shared_ptr<Page> &basePage,
        std::vector<UxSubPageInfo> &subPageInfos, std::string_view entry);
    bool IsValidPage(const std::shared_ptr<Page> &pg) const;
    static constexpr size_t MAX_PAGE_QUEUE_SZ = 3;
    std::deque<std::shared_ptr<Page>> pageQueue_ {};
    std::unordered_map<std::string, std::shared_ptr<Page>> pageMap_ {};
    std::vector<std::shared_ptr<Page>> pages_ {};
    std::shared_ptr<Page> curPage_ {};
    std::shared_ptr<Page> mainPage_ {};
};
} // namespace Updater
#endif
