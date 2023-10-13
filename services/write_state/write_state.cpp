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
#include "write_state.h"
#include <string>

namespace Updater {
__attribute__((weak)) UpdaterStatus WriteResult(std::string &path, const std::string &result)
{
    return UPDATE_SUCCESS;
}
__attribute__((weak)) UpdaterStatus PreStartUpdaterEntry(UpdaterParams &upParams, UpdaterStatus status)
{
    return UPDATE_SUCCESS;
}
__attribute__((weak)) UpdaterStatus PostStartUpdaterEntry(UpdaterParams &upParams, UpdaterStatus status)
{
    return UPDATE_SUCCESS;
}
} // Updater
