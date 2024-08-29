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
#ifndef UPDATE_LOG_H__
#define UPDATE_LOG_H__

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include "error_code.h"

namespace Updater {
#ifdef __WIN32
#undef ERROR
#endif

constexpr size_t MIN_UPDATE_SPACE = 50 * 1024 * 1024;
constexpr int MAX_TIME_SIZE = 20;
#define UPDATER_LOG_FILE_NAME   (Updater::GetLogFileName())
#define LOG(level) UpdaterLogger(level).OutputUpdaterLog((UPDATER_LOG_FILE_NAME), (__LINE__))
#define STAGE(stage) StageLogger(stage).OutputUpdaterStage()
#define ERROR_CODE(code) ErrorCode(code).OutputErrorCode((UPDATER_LOG_FILE_NAME), (__LINE__), (code))

enum {
    DEBUG = 3,
    INFO = 4,
    WARNING = 5,
    ERROR = 6,
    FATAL = 7,
};

enum {
    UPDATE_STAGE_BEGIN,
    UPDATE_STAGE_SUCCESS,
    UPDATE_STAGE_FAIL,
    UPDATE_STAGE_OUT,
};

const char *GetLogFileName();

void SetLogLevel(int level);

void InitUpdaterLogger(const std::string &tag, const std::string &logFile, const std::string &stageFile,
    const std::string &errorCodeFile);

extern "C" void Logger(int level, const char* fileName, int32_t line, const char* format, ...);

extern "C" void UpdaterHiLogger(int level, const char* fileName, int32_t line, const char* format, ...);

class UpdaterLogger {
public:
    UpdaterLogger(int level) : level_(level) {}

    ~UpdaterLogger();

    std::ostream& OutputUpdaterLog(const std::string &path, int line);
private:
    int level_;
    std::stringstream oss_;
    char realTime_[MAX_TIME_SIZE] = {0};
    static inline std::unordered_map<int, std::string> logLevelMap_ = {
        { DEBUG, "D" },
        { INFO, "I" },
        { WARNING, "W" },
        { ERROR, "E" },
        { FATAL, "F" }
    };
};

class StageLogger {
public:
    StageLogger(int stage) : stage_(stage) {}

    ~StageLogger();

    std::ostream& OutputUpdaterStage();
private:
    int stage_;
};

class ErrorCode {
public:
    ErrorCode(enum UpdaterErrorCode level) {}

    ~ErrorCode() {}

    std::ostream& OutputErrorCode(const std::string &path, int line, UpdaterErrorCode code);
};
} // Updater
#endif /* UPDATE_LOG_H__ */
