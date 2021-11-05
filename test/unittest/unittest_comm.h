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

#ifndef UNITTEST_COMM
#define UNITTEST_COMM

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "package/package.h"

const std::string TEST_PATH_FROM = "/data/updater/src/";
const std::string TEST_PATH_TO = "/data/updater/dst/";

inline std::string GetTestPrivateKeyName(int type = 0)
{
    std::string name = TEST_PATH_FROM;
    if (type != PKG_DIGEST_TYPE_SHA384) {
        name += "rsa_private_key2048.pem";
    } else {
        name += "rsa_private_key384.pem";
    }
    return name;
}

inline std::string GetTestCertName(int type = 0)
{
    std::string name = TEST_PATH_FROM;
    if (type != PKG_DIGEST_TYPE_SHA384) {
        name += "signing_cert.crt";
    } else {
        name += "signing_cert384.crt";
    }
    return name;
}
#endif // UNITTEST_COMM
