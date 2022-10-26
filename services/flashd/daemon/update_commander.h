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

#ifndef FLASHD_UPDATE_COMMANDER_H
#define FLASHD_UPDATE_COMMANDER_H

#include "commander.h"

namespace Flashd {
class UpdateCommander : public Commander {
public:
    explicit UpdateCommander(callbackFun callback) : Commander(callback) {};
    ~UpdateCommander() override;
    void DoCommand(const uint8_t *payload, int payloadSize) override;
    void PostCommand() override;
    void DoCommand([[maybe_unused]] const std::string &cmmParam, [[maybe_unused]] size_t fileSize) override;

private:
    bool DoUpdate(const uint8_t *payload, int payloadSize);
    bool ExecUpdate() const;
    void SaveLog() const;
    std::string filePath_ = "";
    int fd_ = -1;
};
} // namespace Flashd
#endif