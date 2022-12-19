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

#ifndef UPDATE_DUMP_H
#define UPDATE_DUMP_H

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <vector>
#include "log.h"
#include "macros.h"

#define UPDATER_LAST_WORD Updater::Dump::GetInstance().DumpInfo
#define UPDATER_INIT_RECORD DumpStageHelper stageHelper(__FUNCTION__)

namespace Updater {
class DumpHelper {
public:
    virtual void RecordDump(const std::string &str) = 0;
    virtual ~DumpHelper() {}
};

class DumpHelperLog : public DumpHelper {
public:
    void RecordDump(const std::string &str) override
    {
        LOG(ERROR) << str;
    }
    ~DumpHelperLog() override {}
};

class Dump {
public:
    DISALLOW_COPY_MOVE(Dump);
    void RegisterDump(const std::string &key, std::unique_ptr<DumpHelper> ptr)
    {
        helpers_.emplace(key, std::move(ptr));
    }
    virtual ~Dump();
    static Dump &GetInstance();
    template<typename ...Args>
    void DumpInfo(Args &&...args)
    {
        std::ostringstream oss;
        std::size_t n {0};
        ((oss << args << (++n != sizeof ...(Args) ? "," : "")), ...);
        std::string str = oss.str();
        for (const auto &[key, value] : helpers_) {
            if (value != nullptr) {
                value->RecordDump(str);
            }
        }
    }

private:
    Dump() {}
    std::map<std::string, std::unique_ptr<DumpHelper>> helpers_;
};

class DumpStageHelper {
public:
    DumpStageHelper(const std::string &stage);
    ~DumpStageHelper();
    static std::stack<std::string> &GetDumpStack();
};
} // namespace Updater
#endif // UPDATE_DUMP_H
