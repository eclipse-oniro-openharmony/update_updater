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
#ifndef UPDATE_PROCESSOR_H
#define UPDATE_PROCESSOR_H

#include <cstdio>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include "applypatch/data_writer.h"
#include "pkg_manager.h"
#include "script_instruction.h"
#include "script_manager.h"

using Uscript::UScriptEnv;
using Uscript::UScriptInstructionFactory;
using Uscript::UScriptInstructionFactoryPtr;
using Uscript::UScriptInstructionPtr;

namespace Updater {
enum EXIT_CODES {
    EXIT_INVALID_ARGS = EXIT_SUCCESS + 1,
    EXIT_READ_PACKAGE_ERROR = 2,
    EXIT_FOUND_SCRIPT_ERROR = 3,
    EXIT_PARSE_SCRIPT_ERROR = 4,
    EXIT_EXEC_SCRIPT_ERROR = 5,
};

int ProcessUpdater(bool retry, int pipeFd, const std::string &packagePath, const std::string &keyPath);
void GetPartitionPathFromName(const std::string &partitionName, std::string &partitionPath);

#endif /* UPDATE_PROCESSOR_H */
