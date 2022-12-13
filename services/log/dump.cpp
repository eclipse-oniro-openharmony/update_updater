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
#include <sys/stat.h>
#include <unordered_map>
#include <vector>
#include "log/log.h"
#include "securec.h"

namespace Updater {
thread_local std::stack<std::string> g_stageStack;
std::string DumpHelper::pkgPath_;

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

void DumpHelper::WriteDumpResult(const std::string &result)
{
    std::string updaterPath = "/data/updater";
    if (access(updaterPath, 0) != 0) {
        if (MkdirRecursive(updaterPath, 0755) != 0) { // 0755: -rwxr-xr-x
            LOG(ERROR) << "MkdirRecursive error!";
            return;
        }
    }
    LOG(INFO) << "WriteDumpResult: " << result;
    const std::string resultPath = updaterPath + "/updater_result";
    std::string writeBuffer {};
    std::ifstream fin {resultPath};
    if (!fin.is_open()) {
        LOG(ERROR) << "open file error" << resultPath;
        return;
    }
    std::string buf;
    while (std::getline(fin, buf)) {
        if (buf.find(GetPackage()) == std::string::npos) {
            writeBuffer += buf + "\n";
            continue;
        }
        writeBuffer += buf + " : " + result + "\n";
    }
    if (writeBuffer != "") {
        writeBuffer.pop_back();
    }
    std::ofstream fout {resultPath};
    if (fout.is_open()) {
        LOG(ERROR) << "open file error" << resultPath;
        return;
    }
    fout << writeBuffer;

    (void)chown(resultPath.c_str(), 0, 6666); // 6666: GROUP_UPDATE_AUTHORITY
    (void)chmod(resultPath.c_str(), 0640); // 0640: -rw-r-----
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
