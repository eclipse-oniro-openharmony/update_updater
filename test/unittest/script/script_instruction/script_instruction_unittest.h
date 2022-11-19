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

#ifndef SCRIPT_INSTRUCTION_UNITTEST_H
#define SCRIPT_INSTRUCTION_UNITTEST_H
namespace Uscript {
class UTestScriptEnv : public UScriptEnv {
public:
    explicit UTestScriptEnv(Hpackage::PkgManager::PkgManagerPtr pkgManager) : UScriptEnv(pkgManager)
    {}
    ~UTestScriptEnv() = default;

    void PostMessage(const std::string &cmd, std::string content) {}

    UScriptInstructionFactoryPtr GetInstructionFactory()
    {
        return nullptr;
    }

    const std::vector<std::string> GetInstructionNames() const
    {
        return {};
    }

    bool IsRetry() const
    {
        return isRetry;
    }
    UScriptInstructionFactory *factory_ = nullptr;
private:
    bool isRetry = false;
};

constexpr auto TEST_VALID_LIB_PATH = "/data/updater/lib/libuser_instruction.so";
constexpr auto TEST_INVALID_LIB_PATH = "/data/updater/lib/libuser_instruction_invalid.so";
constexpr auto TEST_NONEXIST_LIB_PATH = "/system/lib/other.so"; // this lib doesn't exist
}
#endif // SCRIPT_INSTRUCTION_UNITTEST_H