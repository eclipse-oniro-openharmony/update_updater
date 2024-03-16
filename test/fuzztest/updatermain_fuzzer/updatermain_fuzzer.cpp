/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "updatermain_fuzzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include "log/log.h"
#include "updater_main.h"
#include "misc_info/misc_info.h"
#include "updater/updater_const.h"
#include "securec.h"
#include "utils.h"
#include "updater/updater.h"
#include "updater_ui_stub.h"

using namespace Updater;
using namespace std;
constexpr uint32_t MAX_ARG_SIZE = 10;

static void ParseParamsFuzzTest()
{
    UpdateMessage boot {};
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    if (fp == nullptr) {
        return;
    }
    const std::string commandMsg = "boot_updater";
    if (strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()) != 0) {
        return;
    }
    if (strncpy_s(boot.update, sizeof(boot.update), "", sizeof(boot.update)) != 0) {
        return;
    }
    WriteUpdaterMessage(commandFile, boot);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    if (strncpy_s(argv[0], MAX_ARG_SIZE, "./main", MAX_ARG_SIZE) != 0) {
        return;
    }
    int argc = 1;
    Utils::ParseParams(argc, argv);
    PostUpdater(true);
    delete argv[0];
    delete []argv;
}

static void MianUpdaterFuzzTest()
{
    int argsSize = 24;
    UpdateMessage boot {};
    if (access("/data/updater/", 0)) {
        int ret = mkdir("/data/updater/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        if (ret != 0) {
            return;
        }
    }
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    if (fp == nullptr) {
        return;
    }

    const std::string commandMsg = "boot_updater";
    const std::string updateMsg = "--update_package=/data/updater/updater/updater_full.zip";
    if (strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()) != 0) {
        return;
    }
    if (strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()) != 0) {
        return;
    }
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    if (!bRet) {
        return;
    }
    char **argv = new char* [1];
    argv[0] = new char[argsSize];
    if (strncpy_s(argv[0], argsSize, "./UpdaterMain", argsSize) != 0) {
        return;
    }
    int argc = 1;
 
    int ret = UpdaterMain(argc, argv);
    if (!ret) {
        return;
    }
    delete argv[0];
    delete []argv;
}

static void SdCardUpdateFuzzTest()
{
    int argsSize = 24;
    UpdateMessage boot {};
    if (access("/data/updater/", 0)) {
        int ret = mkdir("/data/updater/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        if (ret != 0) {
            return;
        }
    }
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    if (fp == nullptr) {
        return;
    }
    const std::string commandMsg = "boot_updater";
    const std::string updateMsg = "--sdcard_update";
    if (strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()) != 0) {
        return;
    }
    if (strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()) != 0) {
        return;
    }
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    if (!bRet) {
        return;
    }
    char **argv = new char* [1];
    argv[0] = new char[argsSize];
    if (strncpy_s(argv[0], argsSize, "./UpdaterMain", argsSize) != 0) {
        return;
    }
    int argc = 1;
    if (UpdaterMain(argc, argv) != 0) {
        return;
    }
    delete argv[0];
    delete []argv;
}

static void InstallUpdaterPackageFuzzTest()
{
    UpdaterParams upParams;
    upParams.retryCount = 0;
    upParams.callbackProgress = [] (float value) { UPDATER_UI_INSTANCE.ShowProgress(value); };
    upParams.updatePackage.push_back("/data/updater/updater/updater_full.zip");
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    if (InstallUpdaterPackage(upParams, pkgManager) != UPDATE_ERROR) {
        return;
    }
}

static void DoUpdatePackagesFuzzTest()
{
    UpdaterParams upParams;
    if (DoUpdatePackages(upParams) != UPDATE_CORRUPT) {
        return;
    }
    upParams.updatePackage.push_back("/data/updater/updater/updater_full.zip");
    if (DoUpdatePackages(upParams) != UPDATE_CORRUPT) {
        return;
    }
}

static void StartUpdaterEntryFuzzTest()
{
    UpdaterParams upParams;
    upParams.factoryResetMode = "factory_wipe_data";
    if (DoUpdatePackages(upParams) != UPDATE_CORRUPT) {
        return;
    }
    upParams.factoryResetMode = "user_wipe_data";
    if (DoUpdatePackages(upParams) != UPDATE_CORRUPT) {
        return;
    }
    upParams.factoryResetMode = "menu_wipe_data";
    if (DoUpdatePackages(upParams) != UPDATE_CORRUPT) {
        return;
    }
    upParams.factoryResetMode = "";
    if (DoUpdatePackages(upParams) != UPDATE_CORRUPT) {
        return;
    }
}

static void StartUpdaterProcFuzzTest()
{
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    UpdaterParams upParams;
    int maxTemperature = 0;
    if (StartUpdaterProc(nullptr, upParams, maxTemperature) != UPDATE_CORRUPT) {
        return;
    }
    if (StartUpdaterProc(pkgManager, upParams, maxTemperature) != UPDATE_ERROR) {
        return;
    }
}

static void DoInstallUpdaterPackageFuzzTest()
{
    UpdaterParams upParams;
    upParams.callbackProgress = nullptr;
    std::vector<std::string> output;
    if (DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE) != UPDATE_CORRUPT) {
        return;
    }
    upParams.callbackProgress = [] (float value) {};
    if (DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE) != UPDATE_CORRUPT) {
        return;
    }
    upParams.retryCount = 0;
    if (DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE) != UPDATE_CORRUPT) {
        return;
    }
    upParams.retryCount = 1;
    if (DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE) != UPDATE_CORRUPT) {
        return;
    }
}

static void ExtractUpdaterBinaryFuzzTest()
{
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    std::string path = "xxx";
    int32_t ret = ExtractUpdaterBinary(pkgManager, path, UPDATER_BINARY);
    if (ret != 1) {
        return;
    }
    path = "/data/updater/updater/updater_full.zip";
    ret = ExtractUpdaterBinary(pkgManager, path, UPDATER_BINARY);
    Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
    if (ret != 1) {
        return;
    }
}

static void IsSpaceCapacitySufficientFuzzTest()
{
    UpdaterParams upParams {};
    UpdaterStatus status = IsSpaceCapacitySufficient(upParams);
    if (status != UPDATE_ERROR) {
        return;
    }
    upParams.updatePackage.push_back("/data/updater/updater/updater_full.zip");
    status = IsSpaceCapacitySufficient(upParams);
    if (status != UPDATE_SUCCESS) {
        return;
    }
}

namespace OHOS {
    void FuzzUpdater(const uint8_t* data, size_t size)
    {
        ParseParamsFuzzTest();
        MianUpdaterFuzzTest();
        SdCardUpdateFuzzTest();
        InstallUpdaterPackageFuzzTest();
        DoUpdatePackagesFuzzTest();
        StartUpdaterEntryFuzzTest();
        StartUpdaterProcFuzzTest();
        DoInstallUpdaterPackageFuzzTest();
        ExtractUpdaterBinaryFuzzTest();
        IsSpaceCapacitySufficientFuzzTest();
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzUpdater(data, size);
    return 0;
}

