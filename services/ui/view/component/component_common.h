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

#ifndef COMPONENT_COMMON_H
#define COMPONENT_COMMON_H

#include <memory>
#include <string>
#include <type_traits>
#include "components/ui_view.h"
#include "json_visitor.h"

namespace Updater {
struct UxViewCommonInfo {
    int x;
    int y;
    int w;
    int h;
    std::string id;
    std::string type;
    bool visible;
};

struct Serializable {
    virtual ~Serializable() = default;

    virtual bool DeserializeFromJson(const JsonNode &componentNode, const JsonNode &defaultNode) = 0;
    virtual const std::string &GetStructKey() const = 0;
};

struct UxViewSpecificInfo : public Serializable {
    virtual const std::string &GetType() const = 0;
    virtual bool IsValid() const = 0;
};

struct UxViewInfo {
    UxViewCommonInfo commonInfo {};
    // Each derived view component has its own specific data item definitions.
    std::unique_ptr<UxViewSpecificInfo> specificInfo;
};

template <typename TComponent>
struct SpecificInfoWrapper : public UxViewSpecificInfo {
    SpecificInfoWrapper() = default;
    DISALLOW_COPY_MOVE(SpecificInfoWrapper);

    using TSpecificInfo = typename TComponent::SpecificInfoType;

    TSpecificInfo data;

    bool DeserializeFromJson(const JsonNode &componentNode, const JsonNode &defaultNode) override
    {
        return Visit<SETVAL>(componentNode, defaultNode, data);
    }

    const std::string &GetStructKey() const override
    {
        static std::string key {Traits<TSpecificInfo>::STRUCT_KEY};
        return key;
    }

    const std::string &GetType() const override
    {
        static std::string type {TComponent::COMPONENT_TYPE};
        return type;
    }

    bool IsValid() const override
    {
        return TComponent::IsValid(data);
    }
};

class ComponentInterface {
public:
    virtual ~ComponentInterface() = default;
    virtual const char *GetComponentType() = 0;
    virtual OHOS::UIView *GetOhosView() = 0;
    virtual void SetViewCommonInfo(const UxViewCommonInfo &common) = 0;
};

template <typename T>
struct IsUpdaterComponent {
    inline static constexpr bool value = std::is_base_of_v<ComponentInterface, T>;
};

template <typename T>
using EnableIfIsUpdaterComponent = std::enable_if_t<IsUpdaterComponent<T>::value>;

template <typename T>
using EnableIfNotUpdaterComponent = std::enable_if_t<!IsUpdaterComponent<T>::value>;

}  // namespace Updater

#endif
