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

#include "log/dump.h"
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include "log/log.h"
#include "securec.h"

namespace Updater {
thread_local std::stack<std::string> g_stageStack;

extern "C" __attribute__((constructor)) void RegisterDump(void)
{
    Dump::GetInstance().RegisterDump("DumpHelperLog", std::make_unique<DumpHelperLog>());
}

DumpStageHelper::DumpStageHelper(const std::string &stage)
{
    g_stageStack.push(stage);
}

DumpStageHelper::~DumpStageHelper()
{
    if (!g_stageStack.empty()) {
        g_stageStack.pop();
    }
}

std::stack<std::string> &DumpStageHelper::GetDumpStack()
{
    return g_stageStack;
}
Dump &Dump::GetInstance()
{
    static Dump dump;
    return dump;
}

Dump::~Dump()
{
}
} // Updater
