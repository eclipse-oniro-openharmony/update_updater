/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
/*
############
This file is used to support compatibility between platforms, differences between old and new projects and
compilation platforms

defined __MUSL__ Has migrated to the latest version of harmony project

defined HARMONY_PROJECT
With openharmony toolchains suport. If not defined, it should be [device]buildroot or [PC]msys64(...)/ubuntu-apt(...)
envirments
############
*/
#include "system_depend.h"
#include "parameter.h"

namespace Hdc {
namespace SystemDepend {
    bool SetDevItem(const char *key, const char *value)
    {
        bool ret = true;
        ret = SetParameter(key, value) == 0;
        return ret;
    }

    bool GetDevItem(const char *key, string &out, const char *preDefine)
    {
        char tmpStringBuf[BUF_SIZE_MEDIUM] = "";
        auto res = GetParameter(key, preDefine, tmpStringBuf, BUF_SIZE_MEDIUM);
        if (res <= 0) {
            return false;
        }
        out = tmpStringBuf;
        return true;
    }

    bool RebootDevice(const string &cmd)
    {
        const string rebootProperty = "sys.powerctl";
        string propertyVal;
        string reason = cmd;
        if (reason.size() == 0) {
            propertyVal = "reboot";
        } else {
            propertyVal = Base::StringFormat("reboot,%s", reason.c_str());
        }
        return SetDevItem(rebootProperty.c_str(), propertyVal.c_str());
    }
}
}  // namespace Hdc
