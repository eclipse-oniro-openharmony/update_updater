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

#include "component_factory.h"
#include "box_progress_adapter.h"
#include "img_view_adapter.h"
#include "label_btn_adapter.h"
#include "text_label_adapter.h"

namespace Updater {
/*
 * T is component, T::SpecificInfoType is the specific info of this component.
 * ex. T is ImgViewAdapter, then T::SpecificInfoType is UxImageInfo
 * T is LabelBtnAdapter, then T::SpecificInfoType is UxLabelBtnInfo
 * T is BoxProgressAdapter, then T::SpecificInfoType is UxBoxProgressInfo
 * T is TextLabelAdapter, then T::SpecificInfoType is UxLabelInfo
 */
template<typename T>
class CreateFunctor {
public:
    auto operator()([[maybe_unused]] const typename T::SpecificInfoType &specific) const
        -> std::function<std::unique_ptr<OHOS::UIView>(const UxViewInfo &info)>
    {
        return [] (const UxViewInfo &info) { return std::make_unique<T>(info); };
    }
};

template<typename T>
class CheckFunctor {
public:
    auto operator()(const typename T::SpecificInfoType &specific) const
        -> std::function<bool(const UxViewInfo &info)>
    {
        return [&specific] ([[maybe_unused]] const UxViewInfo &info) { return T::IsValid(specific); };
    }
};

template<template<typename> typename Functor, typename ...Component>
class ComponentFactory::Visitor<Functor, std::tuple<Component...>>: Functor<Component>... {
public:
    /*
     * overloading only works within the same scope.
     * so import overloaded operator() into this scope
     * from base class
     */
    using Functor<Component>::operator()...;
    auto Visit(const UxViewInfo &info) const
    {
        return std::visit(*this, info.specificInfo)(info);
    }
};

/*
 * all supported component types listed here, you
 * should add a template argument when add a new
 * component type
 */
using ComponentTypes = std::tuple<BoxProgressAdapter, ImgViewAdapter, TextLabelAdapter, LabelBtnAdapter>;

static_assert(std::tuple_size_v<ComponentTypes> == std::variant_size_v<SpecificInfo>,
    "SpecificInfo's size should be equal to the ComponentTypes' size");

std::unique_ptr<OHOS::UIView> ComponentFactory::CreateUxComponent(const UxViewInfo &info)
{
    return Visitor<CreateFunctor, ComponentTypes> {}.Visit(info);
}

bool ComponentFactory::CheckUxComponent(const UxViewInfo &info)
{
    return Visitor<CheckFunctor, ComponentTypes> {}.Visit(info);
}
} // namespace Updater
