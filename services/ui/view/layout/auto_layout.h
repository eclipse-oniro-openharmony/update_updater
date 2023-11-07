/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef AUTO_LAYOUT_H
#define AUTO_LAYOUT_H
#include "layout_interface.h"
namespace Updater {
class AutoLayout {
    DISALLOW_COPY_MOVE(AutoLayout);
public:
    void RegisterHelper(std::unique_ptr<LayoutInterface> ptr);
    static AutoLayout &GetInstance();
    void SetJsonLocation(JsonNode &root);
    void Init();
private:
    AutoLayout() = default;
    ~AutoLayout() = default;
    std::unique_ptr<LayoutInterface> helper_ {};
};
}
#endif