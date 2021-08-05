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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

enum EXIT_CODES {
    EXIT_INVALID_ARGS = EXIT_SUCCESS + 1,
    EXIT_READ_PACKAGE_ERROR = 2,
    EXIT_FOUND_SCRIPT_ERROR = 3,
    EXIT_PARSE_SCRIPT_ERROR = 4,
    EXIT_EXEC_SCRIPT_ERROR = 5,
};

int main(int argc, char **argv)
{
    constexpr int lessArgIndex = 2;
    constexpr int withRetry = 3;
    constexpr int decimal = 10;
    if (argc < lessArgIndex) {
        std::cout << "Invalid arguments\n";
        return EXIT_INVALID_ARGS;
    }

    bool retry = false;
    int pipeFd = static_cast<int>(std::strtol(argv[1], nullptr, decimal));
    if (argc >= withRetry && strcmp(argv[withRetry - 1], "retry") == 0) {
        retry = true;
    }
    FILE *pipeWrite = fdopen(pipeFd, "w");
    if (pipeWrite == nullptr) {
        std::cout << "Failed to fdopen\n";
        return EXIT_INVALID_ARGS;
    }

    setlinebuf(pipeWrite);
    fprintf(pipeWrite, "ui_log: This is ui output\n");
    fprintf(pipeWrite, "write_log: Write logs\n");
    if (retry) {
        fprintf(pipeWrite, "retry_update\n");
    }
    fprintf(pipeWrite, "show_progress: 1 2\n");
    fprintf(pipeWrite, "show_progress: 1\n");
    fprintf(pipeWrite, "nonexist: 1 2\n");
    fclose(pipeWrite);
    return 0;
}

