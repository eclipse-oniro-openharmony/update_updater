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

#ifndef PROGRESS_STRATEGY_H
#define PROGRESS_STRATEGY_H

#include <memory>
#include "page/page_manager.h"
#include "updater_ui.h"

namespace Updater {
class ProgressStrategy {
public:
    explicit ProgressStrategy(const ComInfo &id) : id_ {id}, pgMgr_ {PageManager::GetInstance()} {}
    virtual ~ProgressStrategy() = default;
    virtual void ShowProgress(float value) const = 0;
    void Show() const;
    void Hide() const;
    static std::unique_ptr<ProgressStrategy> Factory(const std::string &type, const ComInfo &id);
protected:
    ComInfo id_;
    PageManager &pgMgr_;
};

class TxtProgress final : public ProgressStrategy {
public:
    explicit TxtProgress(const ComInfo &id) : ProgressStrategy(id) { }
    ~TxtProgress() override = default;
    void ShowProgress(float value) const override;
};

class BarProgress final : public ProgressStrategy {
public:
    explicit BarProgress(const ComInfo &id);
    ~BarProgress() override = default;
    void ShowProgress(float value) const override;
};
} // namespace Updater
#endif
