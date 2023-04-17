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

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include "box_progress_adapter.h"
#include "img_view_adapter.h"
#include "label_btn_adapter.h"
#include "text_label_adapter.h"
#ifdef COMPONENT_EXT_INCLUDE
#include COMPONENT_EXT_INCLUDE
#endif

/*
 * all supported component types listed here, you
 * should add a template argument when add a new
 * component type
 */
#ifndef COMPONENT_EXT_TYPE_LIST
#define COMPONENT_TYPE_LIST BoxProgressAdapter, ImgViewAdapter, TextLabelAdapter, LabelBtnAdapter
#else
#define COMPONENT_TYPE_LIST BoxProgressAdapter, ImgViewAdapter, TextLabelAdapter, LabelBtnAdapter \
    COMPONENT_EXT_TYPE_LIST
#endif

namespace Updater {
template<typename ... Components>
struct SpecificInfo {
    using Type = std::variant<typename Components::SpecificInfoType...>;
};

class ComponentFactory final {
public:
    static std::unique_ptr<OHOS::UIView> CreateUxComponent(const UxViewInfo &info);
    static bool CheckUxComponent(const UxViewInfo &info);
};

using SpecificInfoFunc = std::function<SpecificInfo<COMPONENT_TYPE_LIST>::Type()>;

template<typename ... Components>
const auto &GetSpecificInfoMap()
{
    const static std::unordered_map<std::string, SpecificInfoFunc> specificInfoMap {
        { Components::COMPONENT_TYPE, [] () { return typename Components::SpecificInfoType {}; }}...
    };
    return specificInfoMap;
}

struct UxViewCommonInfo {
    int x;
    int y;
    int w;
    int h;
    std::string id;
    std::string type;
    bool visible;
};

struct UxViewInfo {
    UxViewCommonInfo commonInfo {};
    SpecificInfo<COMPONENT_TYPE_LIST>::Type specificInfo {};
};
} // namespace Updater
#endif