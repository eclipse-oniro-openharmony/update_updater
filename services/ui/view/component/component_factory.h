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

#ifndef UPDATER_UI_COMPONENT_FACTORY_H
#define UPDATER_UI_COMPONENT_FACTORY_H

#include <memory>
#include <string>
#include "components/ui_view_group.h"
#include "view_api.h"

namespace Updater {
class ComponentFactory final {
    template<template<typename> typename Functor, typename ... Component>
    class Visitor;
public:
    static std::unique_ptr<OHOS::UIViewGroup> CreateUxGroup()
    {
        return std::make_unique<OHOS::UIViewGroup>();
    }
    static std::unique_ptr<OHOS::UIView> CreateUxComponent(const UxViewInfo &info);
    static bool CheckUxComponent(const UxViewInfo &info);
};
} // namespace Updater
#endif