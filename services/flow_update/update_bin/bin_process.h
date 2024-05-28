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
#ifndef UPDATE_BIN_PROCESS_H
#define UPDATE_BIN_PROCESS_H

#include <string>
#include "macros.h"
#include "component_processor.h"
#include "package/pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"

namespace Updater {
class UScriptInstructionBinFlowWrite : public Uscript::UScriptInstruction {
public:
    UScriptInstructionBinFlowWrite() {}
    virtual ~UScriptInstructionBinFlowWrite() {}
    int32_t Execute(Uscript::UScriptEnv &env, Uscript::UScriptContext &context) override;

protected:
    virtual int32_t ProcessBinFile(Uscript::UScriptEnv &env, Uscript::UScriptContext &context,
        Hpackage::PkgManager::StreamPtr stream);
    int32_t ComponentProcess(Uscript::UScriptEnv &env, Hpackage::PkgManager::StreamPtr stream,
        const std::string &name, const Hpackage::FileInfo &fileInfo);
    bool isStopRun_ = false;

private:
    int32_t ExtractBinFile(Uscript::UScriptEnv &env, Uscript::UScriptContext &context,
        Hpackage::PkgManager::StreamPtr stream);
    int32_t UnCompressDataProducer(const Hpackage::PkgBuffer &buffer, size_t size, size_t start,
        bool isFinish, const void* context);
    size_t GetFileLength();
    bool ReadFromBinFile(Hpackage::PkgBuffer &buffer, size_t start, size_t &readLen);
    bool CheckEsDeviceUpdate(const Hpackage::FileInfo &fileInfo);
    bool IsMatchedCsEsIamge(const Hpackage::FileInfo &fileInfo);
    float fullUpdateProportion_ = 1.0f;
    size_t stashDataSize_ = 0;
    Hpackage::PkgBuffer stashBuffer_ {};
};
} // namespace Updater
#endif // UPDATE_BIN_PROCESS_H