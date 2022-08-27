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

#include "diff_patch/diff_patch_interface.h"
#include "update_patch.h"

namespace Updater {
int32_t ApplyPatch(const std::string &patchFile, const std::string &oldfile, const std::string &newFile)
{
    return UpdatePatch::UpdateApplyPatch::ApplyPatch(patchFile, oldfile, newFile);
}
} // namespace Updater