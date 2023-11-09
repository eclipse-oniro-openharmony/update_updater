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
#include "thread_pool.h"
#include <cstring>
#include "script_utils.h"
#include <unistd.h>

namespace Uscript {
static thread_local float g_scriptProportion = 1.0f;
static ThreadPool* g_threadPool = nullptr;
static std::mutex g_initMutex;

void SetScriptProportion(float proportion)
{
    g_scriptProportion = proportion;
}

float GetScriptProportion()
{
    return g_scriptProportion;
}

ThreadPool* ThreadPool::CreateThreadPool(int number)
{
    std::lock_guard<std::mutex> lock(g_initMutex);
    if (g_threadPool != nullptr) {
        return g_threadPool;
    }
    g_threadPool = new ThreadPool();
    g_threadPool->Init(number);
    return g_threadPool;
}

void ThreadPool::Destroy()
{
    if (g_threadPool == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_initMutex);
    delete g_threadPool;
    g_threadPool = nullptr;
}

void ThreadPool::Init(int32_t numberThread)
{
    threadNumber_ = numberThread;
    taskQueue_.resize(THREAD_POOL_MAX_TASKS);
    for (size_t t = 0; t < taskQueue_.size(); ++t) {
        taskQueue_[t].available = true;
        for (int32_t i = 0; i < threadNumber_; ++i) {
            taskQueue_[t].subTaskFlag.emplace_back(new std::atomic_bool { false });
        }
    }
    // Create workers
    for (int32_t threadIndex = 0; threadIndex < threadNumber_; ++threadIndex) {
        workers_.emplace_back(std::thread(ThreadPool::ThreadExecute, this, threadIndex));
    }
}

void ThreadPool::ThreadRun(int32_t threadIndex)
{
    USCRIPT_LOGI("Create new thread successfully, tid: %d", gettid());
    while (!stop_) {
        for (int32_t k = 0; k < THREAD_POOL_MAX_TASKS; ++k) {
            if (*taskQueue_[k].subTaskFlag[threadIndex]) {
                taskQueue_[k].task.processor(threadIndex);
                *taskQueue_[k].subTaskFlag[threadIndex] = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    for (auto& worker : workers_) {
        worker.join();
    }
    for (auto& task : taskQueue_) {
        for (auto c : task.subTaskFlag) {
            delete c;
        }
    }
}

void ThreadPool::AddTask(Task &&task)
{
    if (g_threadPool != nullptr) {
        g_threadPool->AddNewTask(std::move(task));
    }
}

void ThreadPool::AddNewTask(Task &&task)
{
    int32_t index = AcquireWorkIndex();
    if (index < 0) {
        USCRIPT_LOGI("ThreadPool::AddNewTask Failed");
        return;
    }

    RunTask(std::move(task), index);
    // Works done. make this task available
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_[index].available = true;
}

int32_t ThreadPool::AcquireWorkIndex()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    for (int32_t i = 0; i < THREAD_POOL_MAX_TASKS; ++i) {
        if (taskQueue_[i].available) {
            taskQueue_[i].available = false;
            return i;
        }
    }
    return -1;
}

void ThreadPool::RunTask(Task &&task, int32_t index)
{
    int32_t workSize = task.workSize;
    taskQueue_[index].task = std::move(task);
    // Mark each task should be executed
    int32_t num = workSize > threadNumber_ ? threadNumber_ : workSize;
    for (int32_t i = 0; i < num; ++i) {
        *taskQueue_[index].subTaskFlag[i] = true;
    }

    bool complete = true;
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms
        complete = true;
        // 检查是否所有子任务执行结束
        for (int32_t i = 0; i < num; ++i) {
            if (*taskQueue_[index].subTaskFlag[i]) {
                complete = false;
                break;
            }
        }
    } while (!complete);
}
} // namespace Uscript
