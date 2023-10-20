/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "applypatch/update_progress.h"
#include <thread>
#include <mutex>
namespace Updater {
static float g_totalProgress = 0.0f;
static std::mutex g_totalProgressLock;
static bool g_progressExitFlag = false; 

void SetUpdateProgress(float step)
{
    g_totalProgressLock.lock();
    g_totalProgress += step;
    g_totalProgressLock.unlock();
}

float GetUpdateProress()
{
    g_totalProgressLock.lock();
    float progress = g_totalProgress;
    g_totalProgressLock.unlock();
    return progress;
}

void setProgressExitFlag(bool exitFlag)
{
    g_progressExitFlag = exitFlag;
}

static void *OtaUpdateProgressThread(Uscript::UScriptEnv *env)
{
    float totalProgress = 0.0f;
    float curProgress = 0.0f;
    while (totalProgress <= 1.0f) {
        totalProgress = GetUpdateProress();
        if (curProgress < totalProgress && env != nullptr) {
            env->PostMessage("set_progress", std::to_string(totalProgress));
            curProgress = totalProgress;
        }
        if (g_progressExitFlag == true) {
            break;
        }
    }
    return nullptr;
}

int CreateProgressThread(Uscript::UScriptEnv *env)
{
    std::thread progressThread(OtaUpdateProgressThread, env);
    progressThread.detach();
    return 0;
}
}