/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>

#include "component_common.h"
#include "log/log.h"

namespace Updater {

// Generic factory for create UxView and UxInfo
class ComponentFactory final {
    using TViewPtr = std::unique_ptr<ComponentInterface>;
    using TViewCreator = std::function<TViewPtr(const UxViewInfo &)>;
    using TSpecificInfoCreator = std::function<std::unique_ptr<UxViewSpecificInfo>()>;

    ComponentFactory() = default;
    DISALLOW_COPY_MOVE(ComponentFactory);

    struct RegistryInfo {
        std::string componentType;
        TViewCreator createView;
        TSpecificInfoCreator createInfo;

        RegistryInfo(
            const std::string &componentTypeName, TViewCreator &&createViewFunc, TSpecificInfoCreator &&createInfoFunc)
            : componentType(componentTypeName), createView(createViewFunc), createInfo(createInfoFunc)
        {}
    };

public:
    template <typename TComponent>
    static bool Register(const std::string &componentType)
    {
        auto &factory = Instance();
        factory.RegisterImpl<TComponent>(componentType);
        return true;
    }

    static std::unique_ptr<ComponentInterface> CreateView(const std::string &componentType, const UxViewInfo &info)
    {
        auto &factory = Instance();
        std::lock_guard<std::mutex> lock(factory.mutex_);

        auto itr = factory.registry_.find(componentType);
        if (itr == factory.registry_.end()) {
            LOG(ERROR) << "CreateView failed, componentType: " << componentType;
            return nullptr;
        }
        if (itr->second == nullptr) {
            LOG(ERROR) << "CreateView failed, no factory, componentType:" << componentType;
            return nullptr;
        }

        return itr->second->createView(info);
    }

    static std::unique_ptr<UxViewSpecificInfo> CreateSpecificInfo(const std::string &componentType)
    {
        auto &factory = Instance();
        std::lock_guard<std::mutex> lock(factory.mutex_);

        auto itr = factory.registry_.find(componentType);
        if (itr == factory.registry_.end()) {
            LOG(ERROR) << "CreateSpecificInfo failed, componentType: " << componentType;
            return nullptr;
        }
        if (itr->second == nullptr) {
            LOG(ERROR) << "CreateSpecificInfo failed, no factory, componentType: " << componentType;
            return nullptr;
        }

        return itr->second->createInfo();
    }

private:
    static ComponentFactory &Instance()
    {
        static ComponentFactory factory;
        return factory;
    }

    template <typename TComponent>
    void RegisterImpl(const std::string &componentType)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (registry_.count(componentType) > 0) {
            LOG(WARNING) << "component register again: " << componentType;
            return;
        }

        auto item = std::make_unique<RegistryInfo>(
            componentType,
            // Create concrete UxView component
            [](const UxViewInfo &info) { return std::make_unique<TComponent>(info); },
            // Create concrete UxView's info
            []() { return std::make_unique<SpecificInfoWrapper<TComponent>>(); });

        registry_.emplace(componentType, std::move(item));
        LOG(INFO) << "component register: " << componentType;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<RegistryInfo>> registry_;
};

// crtp util to ensure type conversion safety and make component code less repetitive
template <typename TComponent, typename TSpecificInfo>
class ComponentCommon : public ComponentInterface {
public:
    using SpecificInfoType = TSpecificInfo;

    static bool RegisterHook()
    {
        return ComponentFactory::Register<TComponent>(TComponent::COMPONENT_TYPE);
    }

    ~ComponentCommon() override = default;
    const char *GetComponentType() override
    {
        static_assert(TComponent::COMPONENT_TYPE != nullptr, "you must not assign a nullptr to COMPONNET_TYPE");
        return TComponent::COMPONENT_TYPE;
    }
    OHOS::UIView *GetOhosView() override
    {
        return static_cast<TComponent *>(this);
    }
    void SetViewCommonInfo(const UxViewCommonInfo &common) override
    {
        viewId_ = common.id;
        auto child = static_cast<TComponent *>(this);
        child->SetPosition(common.x, common.y, common.w, common.h);
        child->SetVisible(common.visible);
        child->SetViewId(viewId_.c_str());
    }

protected:
    const TSpecificInfo &AsSpecific(const std::unique_ptr<UxViewSpecificInfo> &specificPtr)
    {
        const static SpecificInfoWrapper<TComponent> empty {};

        if (specificPtr == nullptr) {
            LOG(ERROR) << TComponent::COMPONENT_TYPE << " specific convert failed, src is nullptr";
            return empty.data;
        }
        if (empty.GetType() != specificPtr->GetType()) {
            LOG(ERROR) << TComponent::COMPONENT_TYPE << " specific convert failed, src COMPONENT_TYPE is "
                       << specificPtr->GetType();
            return empty.data;
        }
        if (empty.GetStructKey() != specificPtr->GetStructKey()) {
            LOG(ERROR) << TComponent::COMPONENT_TYPE << " specific convert failed, src STRUCT_KEY is "
                       << specificPtr->GetStructKey();
            return empty.data;
        }

        return static_cast<SpecificInfoWrapper<TComponent> *>(specificPtr.get())->data;
    }

protected:
    std::string viewId_{};

private:
    friend TComponent;
    ComponentCommon() = default;
};

}  // namespace Updater
#endif
