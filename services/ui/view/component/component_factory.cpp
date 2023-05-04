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
        -> std::function<std::unique_ptr<ComponentInterface>(const UxViewInfo &info)>
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
class Visitor : Functor<Component>... {
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

std::unique_ptr<ComponentInterface> ComponentFactory::CreateUxComponent(const UxViewInfo &info)
{
    return Visitor<CreateFunctor, COMPONENT_TYPE_LIST> {}.Visit(info);
}

bool ComponentFactory::CheckUxComponent(const UxViewInfo &info)
{
    return Visitor<CheckFunctor, COMPONENT_TYPE_LIST> {}.Visit(info);
}
} // namespace Updater
