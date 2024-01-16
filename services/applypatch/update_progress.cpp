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
#include <pthread.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <string>
namespace Updater {
static std::atomic<float> g_totalProgress(0.0f);
static bool g_progressExitFlag = false;
pthread_t thread;
void FillUpdateProgress()
{
    g_totalProgress.store(1.001f); // ensure > 1.0f
}

void SetUpdateProgress(float step)
{
    float totalProgress = g_totalProgress.load();
    totalProgress += step;
    g_totalProgress.store(totalProgress);
}

float GetUpdateProress()
{
    return g_totalProgress.load();
}

void SetProgressExitFlag(bool exitFlag)
{
    g_progressExitFlag = exitFlag;
    pthread_join(thread, nullptr);
}

static void *OtaUpdateProgressThread(void *usEnv)
{
    Uscript::UScriptEnv *env = static_cast<Uscript::UScriptEnv *>(usEnv);
    float totalProgress = 0.0f;
    float curProgress = 0.0f;
    while (true) {
        totalProgress = GetUpdateProress();
        if (totalProgress > 1.0f) {
            g_totalProgress.store(0.0f);
            totalProgress -= 1.0f;
            curProgress = 0.0f;
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 500ms
            continue;
        }
        if (g_progressExitFlag == true) {
            break;
        }
        if (curProgress < totalProgress && env != nullptr) {
            env->PostMessage("set_progress", std::to_string(totalProgress));
            curProgress = totalProgress;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms
    }
    return nullptr;
}

int CreateProgressThread(Uscript::UScriptEnv *env)
{
    std::string content = std::to_string(1.0f) + "," + std::to_string(0.0f); // set g_percentage 100
    env->PostMessage("show_progress", content);
    (void)pthread_create(&thread, nullptr, OtaUpdateProgressThread, env);
    return 0;
}
}