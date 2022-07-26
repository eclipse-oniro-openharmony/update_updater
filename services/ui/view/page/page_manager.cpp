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

#include "page_manager.h"
#include "common/screen.h"
#include "components/root_view.h"
#include "sub_page.h"

namespace Updater {
PageManager &PageManager::GetInstance()
{
    static PageManager instance;
    return instance;
}

void PageManager::Init(std::vector<UxPageInfo> &pageInfos, std::string_view entry)
{
    for (auto &pageInfo : pageInfos) {
        auto basePage =
            std::make_unique<BasePage>(OHOS::Screen::GetInstance().GetWidth(), OHOS::Screen::GetInstance().GetHeight());
        basePage->BuildPage(pageInfo);
        basePage->SetVisible(false);
        OHOS::RootView::GetInstance()->Add(basePage->GetView());
        BuildSubPages(basePage->GetPageId(), *basePage, pageInfo.subpages, entry);
        pages_.push_back(std::move(basePage));
        pageMap_[pageInfo.id] = pages_.back().get();
        if (pageInfo.id == entry) {
            mainPage_ = pages_.back().get();
        }
    }
    if (!IsValidPage(mainPage_)) {
        LOG(ERROR) << "entry " << entry << " is invalid ";
        return;
    }
    curPage_ = mainPage_;
}

void PageManager::BuildSubPages(const std::string &pageId, BasePage &basePage,
    std::vector<UxSubPageInfo> &subPageInfos, std::string_view entry)
{
    for (auto &subPageInfo : subPageInfos) {
        const std::string &subPageId = pageId + ":" + subPageInfo.id;
        auto subPage = std::make_unique<SubPage>(subPageInfo, basePage, subPageId);
        pages_.push_back(std::move(subPage));
        LOG(INFO) << subPageId << " builded";
        if (subPageId == entry) {
            mainPage_ = pages_.back().get();
        }
        pageMap_[subPageId] = pages_.back().get();
    }
}

bool PageManager::IsValidCom(const ComInfo &pageComId) const
{
    const std::string &pageId = pageComId.pageId;
    const std::string &comId = pageComId.comId;
    auto it = pageMap_.find(pageId);
    if (it == pageMap_.end() || it->second == nullptr) {
        LOG(ERROR) << "page id " << pageId << "not valid";
        return false;
    }
    Page &page = *(it->second);
    return page.IsValidCom(comId);
}

bool PageManager::IsValidPage(const Page *pg) const
{
    return pg != nullptr && pg->IsValid();
}

void PageManager::ShowPage(const std::string &id)
{
    if (!IsValidPage(curPage_)) {
        LOG(ERROR) << "cur page is null";
        return;
    }
    if (id == curPage_->GetPageId()) {
        curPage_->SetVisible(true);
        LOG(WARNING) << "show cur page again";
        return;
    }
    curPage_->SetVisible(false);
    auto newPage = &(*this)[id];
    if (!IsValidPage(newPage)) {
        LOG(ERROR) << "show page failed, id = " << id;
        return;
    }
    EnQueuePage(curPage_);
    curPage_ = newPage;
    curPage_->SetVisible(true);
}

void PageManager::ShowMainPage()
{
    if (!IsValidPage(mainPage_)) {
        LOG(ERROR) << "main page invalid, can't show main page";
        return;
    }
    ShowPage(mainPage_->GetPageId());
}

void PageManager::GoBack()
{
    LOG(DEBUG) << "go back";
    if (!IsValidPage(curPage_)) {
        LOG(ERROR) << "cur page is null";
        return;
    }
    if (pageQueue_.empty()) {
        LOG(ERROR) << "queue empty, can't go back";
        return;
    }
    curPage_->SetVisible(false);
    curPage_ = pageQueue_.front();
    pageQueue_.pop_front();
    curPage_->SetVisible(true);
}

Page &PageManager::operator[](const std::string &id) const
{
    static BasePage dummy;
    auto it = pageMap_.find(id);
    if (it == pageMap_.end()) {
        return dummy;
    }
    if (it->second == nullptr) {
        return dummy;
    }
    return *(it->second);
}

ViewProxy PageManager::operator[](const ComInfo &comInfo) const
{
    return (*this)[comInfo.pageId][comInfo.comId];
}

void PageManager::EnQueuePage(Page *page)
{
    if (page == nullptr) {
        LOG(ERROR) << "enqueue null page";
        return;
    }
    pageQueue_.push_front(page);
    if (pageQueue_.size() > MAX_PAGE_QUEUE_SZ) {
        pageQueue_.pop_back();
    }
}
}