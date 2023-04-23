/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <string>
#include "components/ui_view.h"

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

class ComponentInterface {
public:
    virtual ~ComponentInterface() = default;
    virtual const char *GetComponentType() = 0;
    virtual OHOS::UIView *GetOhosView() = 0;
};

// crtp util to ensure type conversion safety and make component code less repetitive
template<class Component>
class ComponentCommon : public ComponentInterface {
public:
    virtual ~ComponentCommon() = default;
    virtual const char *GetComponentType()
    {
        static_assert(Component::COMPONENT_TYPE != nullptr, "you must not assign a nullptr to COMPONNET_TYPE");
        return Component::COMPONENT_TYPE;
    }
    Component *GetComponent(void)
    {
        return static_cast<Component *>(this);
    }
    OHOS::UIView *GetOhosView()
    {
        return static_cast<OHOS::UIView *>(static_cast<Component *>(this));
    }
    void SetViewCommonInfo(const UxViewCommonInfo &common)
    {
        viewId_ = common.id;
        auto child = static_cast<Component *>(this);
        child->SetPosition(common.x, common.y, common.w, common.h);
        child->SetVisible(common.visible);
        child->SetViewId(viewId_.c_str());
    }
protected:
    std::string viewId_ {};
};
}


#endif