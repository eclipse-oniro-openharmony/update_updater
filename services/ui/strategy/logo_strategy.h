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

#ifndef LOGO_STRATEGY_H
#define LOGO_STRATEGY_H

#include <memory>
#include "page/page_manager.h"
#include "updater_ui.h"

namespace Updater {
class LogoStrategy {
public:
    explicit LogoStrategy(const ComInfo &id) : id_ {id}, pgMgr_ {PageManager::GetInstance()} {}
    virtual ~LogoStrategy() = default;
    virtual void Show() const = 0;
    virtual void Hide() const = 0;
    static std::unique_ptr<LogoStrategy> Factory(const std::string &type, const ComInfo &id);
protected:
    ComInfo id_;
    PageManager &pgMgr_;
};

class AnimatorLogo final : public LogoStrategy {
public:
    explicit AnimatorLogo(const ComInfo &id) : LogoStrategy(id) { }
    ~AnimatorLogo() override = default;
    void Show() const override;
    void Hide() const override;
};

class ImageLogo final : public LogoStrategy {
public:
    explicit ImageLogo(const ComInfo &id) : LogoStrategy(id) { }
    ~ImageLogo() override = default;
    void Show() const override;
    void Hide() const override;
};
} // namespace Updater
#endif
