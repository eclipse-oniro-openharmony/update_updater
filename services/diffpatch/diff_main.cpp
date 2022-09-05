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
#include "update_diff.h"
#include "update_patch.h"
#include <getopt.h>

struct DiffParams {
    std::string source;
    std::string destination;
    std::string patch;
    int limit;
    int block;
};

int main(int argc, char *argv[])
{
    DiffParams diffParams {
        "", "", "", 0, 0
    };
    int opt;
    const char *optstring = "s:d:p:l:b:";
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 's':
                diffParams.source = optarg;
                break;
            case 'd':
                diffParams.destination = optarg;
                break;
            case 'p':
                diffParams.patch = optarg;
                break;
            case 'l':
                diffParams.limit = atoi(optarg);
                break;
            case 'b':
                diffParams.block = atoi(optarg);
                break;
            case '?':
                break;
            default:
                break;
        }
    }

    // pack
    if (diffParams.source != "" && diffParams.destination != "" && diffParams.patch != "") {
        if (diffParams.block != 1) {
            UpdatePatch::UpdateDiff::DiffImage(
                diffParams.limit,
                diffParams.source,
                diffParams.destination,
                diffParams.patch);
        } else {
            UpdatePatch::UpdateDiff::DiffBlock(
                diffParams.source,
                diffParams.destination,
                diffParams.patch);
        }
    }
    return 0;
}
