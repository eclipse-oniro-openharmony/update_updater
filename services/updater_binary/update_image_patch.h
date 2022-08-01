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

#ifndef UPDATER_FILE_PATCH_H
#define UPDATER_FILE_PATCH_H

#include "pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"

namespace Updater {
class USInstrImagePatch : public Uscript::UScriptInstruction {
public:
    struct ImagePatchPara {
        std::string partName {};
        std::string devPath {};
        std::string srcSize {};
        std::string srcHash {};
        std::string destSize {};
        std::string destHash {};
        std::string patchFile {};
    };
    USInstrImagePatch() {}
    virtual ~USInstrImagePatch() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
private:
    int32_t GetParam(Uscript::UScriptContext &context, ImagePatchPara &para);
    std::string GetPatchFile(Uscript::UScriptEnv &env, const ImagePatchPara &para);
    int32_t ExecuteImagePatch(Uscript::UScriptEnv &env, Uscript::UScriptContext &context);
    std::string GetSourceFile(const ImagePatchPara &para);
    int32_t ApplyPatch(const ImagePatchPara &para, const std::string &patchFile);
    std::string GetFileHash(const std::string &file);
};

class USInstrImageShaCheck : public Uscript::UScriptInstruction {
public:
    struct CheckPara {
        std::string partName {};
        std::string devPath {};
        std::string srcSize {};
        std::string srcHash {};
        std::string destSize {};
        std::string destHash {};
    };
    USInstrImageShaCheck() {}
    virtual ~USInstrImageShaCheck() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;
private:
    int32_t ExecuteShaCheck(Uscript::UScriptEnv &env, Uscript::UScriptContext &context);
    int32_t GetParam(Uscript::UScriptContext &context, CheckPara &para);
    int32_t CheckHash(const CheckPara &para);
};
}
#endif // UPDATER_FILE_PATCH_H
