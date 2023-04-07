/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "script_manager_impl.h"
#include <cstring>
#include <dlfcn.h>
#include "dump.h"
#include "pkg_manager.h"
#include "script_instructionhelper.h"
#include "script_interpreter.h"
#include "script_utils.h"
#include "thread_pool.h"
#include "updater_const.h"
#include "utils.h"

using namespace Hpackage;

namespace Uscript {
constexpr const char *LOAD_SCRIPT_NAME = "loadScript.us";
constexpr const char *REGISTER_CMD_SCRIPT_NAME = "registerCmd.us";

static ScriptManagerImpl* g_scriptManager = nullptr;
ScriptManager* ScriptManager::GetScriptManager(UScriptEnv *env, const Hpackage::HashDataVerifier *verifier)
{
    if (env == nullptr || verifier == nullptr) {
        USCRIPT_LOGE("Env or verifier is null");
        return nullptr;
    }
    if (g_scriptManager != nullptr) {
        return g_scriptManager;
    }
    g_scriptManager = new (std::nothrow) ScriptManagerImpl(env, verifier);
    if (g_scriptManager == nullptr) {
        USCRIPT_LOGE("Create g_scriptManager failed");
        return nullptr;
    }
    if (g_scriptManager->Init() != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("g_scriptManager init failed");
        return nullptr;
    }
    return g_scriptManager;
}

void ScriptManager::ReleaseScriptManager()
{
    if (g_scriptManager != nullptr) {
        delete g_scriptManager;
    }
    g_scriptManager = nullptr;
}

ScriptManagerImpl::~ScriptManagerImpl()
{
    if (threadPool_) {
        ThreadPool::Destroy();
        threadPool_ = nullptr;
    }
    for (int i = 0; i < MAX_PRIORITY; i++) {
        scriptFiles_[i].clear();
    }
    auto iter1 = scriptInstructions_.begin();
    while (iter1 != scriptInstructions_.end()) {
        UScriptInstructionPtr inst = (*iter1).second;
        delete inst;
        iter1 = scriptInstructions_.erase(iter1);
    }
    scriptInstructions_.clear();
    ScriptInstructionHelper::ReleaseBasicInstructionHelper();
}

int32_t ScriptManagerImpl::Init()
{
    if (scriptEnv_ == nullptr) {
        USCRIPT_LOGE("Env null");
        return USCRIPT_INVALID_PARAM;
    }

    threadPool_ = ThreadPool::CreateThreadPool(MAX_PRIORITY);
    if (threadPool_ == nullptr) {
        USCRIPT_LOGE("Failed to create thread pool");
        return USCRIPT_INVALID_PARAM;
    }

    // Register system reserved instructions
    ScriptInstructionHelper* helper = ScriptInstructionHelper::GetBasicInstructionHelper(this);
    if (helper == nullptr) {
        USCRIPT_LOGE("Failed to get helper");
        return USCRIPT_INVALID_PARAM;
    }
    helper->RegisterInstructions();

    // Register customized instructions
    RegisterInstruction(*helper);

    PkgManager::PkgManagerPtr manager = scriptEnv_->GetPkgManager();
    if (manager == nullptr) {
        USCRIPT_LOGE("Failed to get pkg manager");
        return USCRIPT_INVALID_PARAM;
    }

    // Register other instructions from scripts
    int32_t ret = USCRIPT_SUCCESS;
    const FileInfo *info = manager->GetFileInfo(REGISTER_CMD_SCRIPT_NAME);
    if (info != nullptr) {
        ret = ExtractAndExecuteScript(manager, REGISTER_CMD_SCRIPT_NAME);
    }
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to extract and execute script ");
        return ret;
    }

    // Collect scripts
    ret = ExtractAndExecuteScript(manager, LOAD_SCRIPT_NAME);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to extract and execute script ");
        return ret;
    }
    return USCRIPT_SUCCESS;
}

int32_t ScriptManagerImpl::RegisterInstruction(ScriptInstructionHelper &helper)
{
    Uscript::UScriptInstructionFactoryPtr factory = scriptEnv_->GetInstructionFactory();
    if (factory == nullptr) {
        USCRIPT_LOGE("None factory");
        return USCRIPT_SUCCESS;
    }

    for (auto instrName : scriptEnv_->GetInstructionNames()) {
        // Create instructions and register it.
        UScriptInstructionPtr instr = nullptr;
        int32_t ret = factory->CreateInstructionInstance(instr, instrName);
        if (ret != USCRIPT_SUCCESS) {
            USCRIPT_LOGE("Failed to create instruction for %s", instrName.c_str());
            return ret;
        }
        helper.AddInstruction(instrName, instr);
        if (ret != USCRIPT_SUCCESS) {
            USCRIPT_LOGE("Failed to add instruction for %s", instrName.c_str());
            return ret;
        }
    }
    return USCRIPT_SUCCESS;
}

int32_t ScriptManagerImpl::ExtractAndExecuteScript(PkgManager::PkgManagerPtr manager,
    const std::string &scriptName)
{
    UPDATER_INIT_RECORD;
    PkgManager::StreamPtr outStream = nullptr;
    const std::string path = Updater::Utils::IsUpdaterMode() ? "" : Updater::UPDATER_PATH;
    int32_t ret = manager->CreatePkgStream(outStream, path + "/" + scriptName, 0, PkgStream::PkgStreamType_Write);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to create script stream %s", scriptName.c_str());
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    ret = manager->ExtractFile(scriptName, outStream);
    if (ret != USCRIPT_SUCCESS) {
        manager->ClosePkgStream(outStream);
        USCRIPT_LOGE("Failed to extract script stream %s", scriptName.c_str());
        UPDATER_LAST_WORD(ret);
        return ret;
    }
    if (scriptVerifier_ == nullptr || !scriptVerifier_->VerifyHashData(scriptName, outStream)) {
        manager->ClosePkgStream(outStream);
        USCRIPT_LOGE("verify script %s by hash signed data failed", scriptName.c_str());
        UPDATER_LAST_WORD(ret);
        return USCRIPT_INVALID_SCRIPT;
    }
    ret = ScriptInterpreter::ExecuteScript(this, outStream);
    manager->ClosePkgStream(outStream);
    if (ret != USCRIPT_SUCCESS) {
        USCRIPT_LOGE("Failed to ExecuteScript %s", scriptName.c_str());
        return ret;
    }
    return ret;
}

int32_t ScriptManagerImpl::ExecuteScript(int32_t priority)
{
    if (priority >= MAX_PRIORITY || priority < 0) {
        USCRIPT_LOGE("ExecuteScript priority not support %d", priority);
        UPDATER_LAST_WORD(USCRIPT_INVALID_PRIORITY, priority);
        return USCRIPT_INVALID_PRIORITY;
    }
    PkgManager::PkgManagerPtr manager = scriptEnv_->GetPkgManager();
    if (manager == nullptr) {
        USCRIPT_LOGE("Failed to get pkg manager");
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_INVALID_PARAM;
    }
    if (scriptFiles_[priority].size() == 0) {
        return USCRIPT_SUCCESS;
    }

    // Execute scripts
    int32_t threadNumber = threadPool_->GetThreadNumber();
    Task task;
    int32_t ret = USCRIPT_SUCCESS;
    int32_t retCode = USCRIPT_SUCCESS;
    task.workSize = threadNumber;
    task.processor = [&](int iter) {
        for (size_t i = static_cast<size_t>(iter); i < scriptFiles_[priority].size();
            i += static_cast<size_t>(threadNumber)) {
            ret = ExtractAndExecuteScript(manager, scriptFiles_[priority][i]);
            if (ret != USCRIPT_SUCCESS) {
                USCRIPT_LOGE("Failed to execute script %s", scriptFiles_[priority][i].c_str());
                retCode = ret;
            }
        }
    };
    ThreadPool::AddTask(std::move(task));
    return retCode;
}

int32_t ScriptManagerImpl::AddInstruction(const std::string &instrName, const UScriptInstructionPtr instruction)
{
    USCRIPT_LOGI("AddInstruction instrName: %s ", instrName.c_str());
    if (scriptInstructions_.find(instrName) != scriptInstructions_.end()) {
        USCRIPT_LOGW("Instruction: %s exist", instrName.c_str());
        // New instruction has the same name
        // with already registered instruction,
        // just override it.
        delete scriptInstructions_[instrName];
    }
    scriptInstructions_[instrName] = instruction;
    return USCRIPT_SUCCESS;
}

int32_t ScriptManagerImpl::AddScript(const std::string &scriptName, int32_t priority)
{
    UPDATER_INIT_RECORD;
    if (priority < 0 || priority >= MAX_PRIORITY) {
        USCRIPT_LOGE("Invalid priority %d", priority);
        UPDATER_LAST_WORD(USCRIPT_INVALID_PRIORITY);
        return USCRIPT_INVALID_PRIORITY;
    }

    PkgManager::PkgManagerPtr manager = scriptEnv_->GetPkgManager();
    if (manager == nullptr) {
        USCRIPT_LOGE("Failed to get pkg manager");
        UPDATER_LAST_WORD(USCRIPT_INVALID_PARAM);
        return USCRIPT_INVALID_PARAM;
    }

    if (manager->GetFileInfo(scriptName) == nullptr) {
        USCRIPT_LOGE("Failed to access script %s", scriptName.c_str());
        UPDATER_LAST_WORD(USCRIPT_INVALID_SCRIPT);
        return USCRIPT_INVALID_SCRIPT;
    }
    scriptFiles_[priority].push_back(scriptName);
    return USCRIPT_SUCCESS;
}

UScriptInstruction* ScriptManagerImpl::FindInstruction(const std::string &instrName)
{
    if (scriptInstructions_.find(instrName) == scriptInstructions_.end()) {
        return nullptr;
    }
    return scriptInstructions_[instrName];
}

UScriptEnv* ScriptManagerImpl::GetScriptEnv(const std::string &instrName) const
{
    return scriptEnv_;
}
} // namespace Uscript
