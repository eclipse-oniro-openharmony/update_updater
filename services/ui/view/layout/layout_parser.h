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

#ifndef LAYOUT_PARSER_H
#define LAYOUT_PARSER_H

#include "macros.h"
#include "view_api.h"

namespace Updater {
class LayoutParser {
    DISALLOW_COPY_MOVE(LayoutParser);
public:
    static LayoutParser &GetInstance();
    bool LoadLayout(const std::vector<std::string> &layoutFiles, std::vector<UxPageInfo> &vec) const;
    bool LoadLayout(const std::string &layoutFile, UxPageInfo &pageInfo) const;
private:
    LayoutParser();
    ~LayoutParser();
    class Impl;
    std::unique_ptr<Impl> pImpl_ {};
};
} // namespace Updater

#endif // LAYOUT_PARSER_H
