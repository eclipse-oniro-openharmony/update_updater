/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "log/log.h"
#include <chrono>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <vector>
#include "securec.h"

namespace Updater {
static std::ofstream g_updaterLog;
static std::ofstream g_updaterStage;
static std::ofstream g_errorCode;
static std::string g_logTag;
static int g_logLevel = INFO;
#ifndef DIFF_PATCH_SDK
static OHOS::HiviewDFX::HiLogLabel g_logLabel = {LOG_CORE, 0XD002E01, "UPDATER"};
#endif

void InitUpdaterLogger(const std::string &tag, const std::string &logFile, const std::string &stageFile,
    const std::string &errorCodeFile)
{
    g_logTag = tag;
#ifndef DIFF_PATCH_SDK
    g_logLabel.tag = g_logTag.c_str();
#endif
    g_updaterLog.open(logFile.c_str(), std::ios::app | std::ios::out);
    g_updaterStage.open(stageFile.c_str(), std::ios::app | std::ios::out);
    g_errorCode.open(errorCodeFile.c_str(), std::ios::app | std::ios::out);
}

UpdaterLogger::~UpdaterLogger()
{
    std::string str = oss_.str();
    if (g_logLevel > level_) {
        std::cout << std::endl << std::flush;
        return;
    }
#ifndef DIFF_PATCH_SDK
    switch (level_) {
        case static_cast<int>(INFO):
            OHOS::HiviewDFX::HiLog::Info(g_logLabel, "%{public}s", str.c_str());
            break;
        case static_cast<int>(ERROR):
            OHOS::HiviewDFX::HiLog::Error(g_logLabel, "%{public}s", str.c_str());
            break;
        case static_cast<int>(DEBUG):
            OHOS::HiviewDFX::HiLog::Debug(g_logLabel, "%{public}s", str.c_str());
            break;
        case static_cast<int>(WARNING):
            OHOS::HiviewDFX::HiLog::Warn(g_logLabel, "%{public}s", str.c_str());
            break;
        default:
            break;
    }
#endif
    oss_.str("");
    oss_ << std::endl << std::flush;
    if (g_updaterLog.is_open()) {
        g_updaterLog << realTime_ <<  "  " << "[" << logLevelMap_[level_] << "]" <<
            g_logTag << " " << str << std::endl << std::flush;
    }
}

StageLogger::~StageLogger()
{
    if (g_updaterStage.is_open()) {
        g_updaterStage << std::endl << std::flush;
    } else {
        std::cout << std::endl << std::flush;
    }
}

void SetLogLevel(int level)
{
    g_logLevel = level;
}

std::ostream& UpdaterLogger::OutputUpdaterLog(const std::string &path, int line)
{
    auto sysTime = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(sysTime);
    struct tm *localTime = std::localtime(&currentTime);
    if (localTime != nullptr) {
        std::strftime(realTime_, sizeof(realTime_), "%Y-%m-%d %H:%M:%S", localTime);
    }
    if (g_logLevel <= level_) {
        return oss_ << path << " " << line << " : ";
    }
    return std::cout;
}

std::ostream& StageLogger::OutputUpdaterStage()
{
    std::unordered_map<int, std::string> updaterStageMap = {
        { UPDATE_STAGE_BEGIN, "BEGIN" },
        { UPDATE_STAGE_SUCCESS, "SUCCESS" },
        { UPDATE_STAGE_FAIL, "FAIL" },
        { UPDATE_STAGE_OUT, "OUT" }
    };
    char realTime[MAX_TIME_SIZE] = {0};
    auto sysTime = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(sysTime);
    struct tm *localTime = std::localtime(&currentTime);
    if (localTime != nullptr) {
        std::strftime(realTime, sizeof(realTime), "%Y-%m-%d %H:%M:%S", localTime);
    }

    if (g_updaterLog.is_open()) {
        if (stage_ == UPDATE_STAGE_OUT) {
            return g_updaterStage << realTime << "  " << g_logTag << " ";
        }
        if (auto it = updaterStageMap.find(stage_); it != updaterStageMap.end()) {
            return g_updaterStage << realTime << "  " << g_logTag << " status is : " <<
                it->second << ", stage is ";
        }
    }
    return std::cout;
}

void Logger(int level, const char* fileName, int32_t line, const char* format, ...)
{
    static std::vector<char> buff(1024); // 1024 : max length of buff
    va_list list;
    va_start(list, format);
    int size = vsnprintf_s(reinterpret_cast<char*>(buff.data()), buff.capacity(), buff.capacity(), format, list);
    va_end(list);
    if (size < EOK) {
        UpdaterLogger(level).OutputUpdaterLog(fileName, line) << "vsnprintf_s failed";
        return;
    }
    std::string str(buff.data(), size);
    UpdaterLogger(level).OutputUpdaterLog(fileName, line) << str;
}

std::ostream& ErrorCode::OutputErrorCode(const std::string &path, int line, UpdaterErrorCode code)
{
    char realTime[MAX_TIME_SIZE] = {0};
    auto sysTime = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(sysTime);
    struct tm *localTime = std::localtime(&currentTime);
    if (localTime != nullptr) {
        std::strftime(realTime, sizeof(realTime), "%Y-%m-%d %H:%M:%S", localTime);
    }
    if (g_errorCode.is_open()) {
        return g_errorCode << realTime <<  "  " << path << " " << line << " , error code is : " << code << std::endl;
    }
    return std::cout;
}
} // Updater
