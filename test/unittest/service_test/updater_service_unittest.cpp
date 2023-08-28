/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <sys/ioctl.h>
#include "log/log.h"
#include "securec.h"
#include "updater/updater_const.h"
#include "updater/updater.h"
#include "fs_manager/mount.h"
#include "misc_info/misc_info.h"
#include "updater_main.h"
#include "updater_ui_stub.h"
#include "utils.h"

using namespace Updater;
using namespace std;
using namespace testing::ext;

namespace {
constexpr uint32_t MAX_ARG_SIZE = 24;
class UpdaterUtilUnitTest : public testing::Test {
public:
    UpdaterUtilUnitTest()
    {
        InitUpdaterLogger("UPDATER", TMP_LOG, TMP_STAGE_LOG, TMP_ERROR_CODE_PATH);
    }
    ~UpdaterUtilUnitTest() {}

    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
    void TestBody() {}
};

HWTEST_F(UpdaterUtilUnitTest, DeleteUpdaterPath, TestSize.Level1)
{
    std::string path = "/data/test/test/test";
    bool ret = DeleteUpdaterPath(path);
    EXPECT_EQ(ret, true);

    path = "/data/test";
    ret = DeleteUpdaterPath(path);
    EXPECT_EQ(ret, true);
}

HWTEST_F(UpdaterUtilUnitTest, ClearMisc, TestSize.Level1)
{
    bool ret = ClearMisc();
    EXPECT_EQ(ret, true);
}

HWTEST_F(UpdaterUtilUnitTest, IsSDCardExist, TestSize.Level1)
{
    std::string sdcardStr = "";
    bool ret = IsSDCardExist(sdcardStr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(UpdaterUtilUnitTest, IsFlashd, TestSize.Level1)
{
    EXPECT_EQ(IsFlashd({"boot_updater", "", "boot_flash"}), true);
    EXPECT_EQ(IsFlashd({"boot_updater", "", ""}), false);
}

HWTEST_F(UpdaterUtilUnitTest, IsUpdater, TestSize.Level1)
{
    EXPECT_EQ(IsUpdater({"boot_updater", "", ""}), true);
    EXPECT_EQ(IsUpdater({"boot_updater", "", "boot_flash"}), false);
    EXPECT_EQ(IsUpdater({"boot_updater", "", "xxx"}), true);
}

HWTEST_F(UpdaterUtilUnitTest, SelectMode, TestSize.Level1)
{
    // clear already registered mode
    GetBootModes().clear();

    auto dummyEntry = [] (int argc, char **argv) -> int { return 0; };
    // register modes
    RegisterMode({ IsFlashd, "FLASHD", "", dummyEntry });
    RegisterMode({ IsUpdater, "UPDATER", "", dummyEntry });

    // test select mode
    auto mode = SelectMode({"boot_updater", "", ""});
    ASSERT_NE(mode, std::nullopt);
    EXPECT_EQ(mode->modeName, "UPDATER");

    mode = SelectMode({"boot_updater", "", "boot_flash"});
    ASSERT_NE(mode, std::nullopt);
    EXPECT_EQ(mode->modeName, "FLASHD");

    mode = SelectMode({"invalid_command", "", ""});
    EXPECT_EQ(mode, std::nullopt);
}

HWTEST_F(UpdaterUtilUnitTest, PostUpdater, TestSize.Level1)
{
    PostUpdater(true);
    LoadSpecificFstab("/data/updater/updater/etc/fstab.ut.updater");
    int ret = access(TMP_LOG, 0);
    EXPECT_EQ(ret, 0);
    PostUpdater(true);
    ret = access(UPDATER_LOG, 0);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UpdaterUtilUnitTest, ParseParams, TestSize.Level1)
{
    UpdateMessage boot {};
    std::string commandMsg = "";
    std::string updateMsg = "";
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()), 0);
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    std::string str = "./UpdaterMain";
    EXPECT_EQ(strncpy_s(argv[0], MAX_ARG_SIZE, str.c_str(), str.size()), 0);
    int argc = 1;
    std::vector<std::string> args = Utils::ParseParams(argc, argv);
    std::string res = "";
    for (auto s : args) {
        res += s;
    }
    EXPECT_EQ("./UpdaterMain", res);

    commandMsg = "boot_updater";
    updateMsg = "--update_package=updater_full.zip";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()), 0);
    bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);

    args = Utils::ParseParams(argc, argv);
    res = "";
    for (auto s : args) {
        res += s;
    }
    EXPECT_EQ("./UpdaterMain--update_package=updater_full.zip", res);
}

HWTEST_F(UpdaterUtilUnitTest, UpdaterMain, TestSize.Level1)
{
    UpdateMessage boot {};
    if (access("/data/updater/", 0)) {
        int ret = mkdir("/data/updater/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        ASSERT_EQ(ret, 0);
    }
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);
    const std::string commandMsg = "boot_updater";
    const std::string updateMsg = "--update_package=/data/updater/updater/updater_full.zip";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()), 0);
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    EXPECT_EQ(strncpy_s(argv[0], MAX_ARG_SIZE, "./UpdaterMain", MAX_ARG_SIZE), 0);
    int argc = 1;

    int ret = UpdaterMain(argc, argv);
    EXPECT_EQ(ret, 0);
    delete argv[0];
    delete []argv;
}

HWTEST_F(UpdaterUtilUnitTest, UpdaterFromSdcardTest, TestSize.Level1)
{
    UpdateMessage boot {};
    if (access("/data/updater/", 0)) {
        int ret = mkdir("/data/updater/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        ASSERT_EQ(ret, 0);
    }
    const std::string commandFile = "/data/updater/command";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(commandFile.c_str(), "wb"), fclose);
    EXPECT_NE(fp, nullptr);
    const std::string commandMsg = "boot_updater";
    const std::string updateMsg = "--sdcard_update";
    EXPECT_EQ(strncpy_s(boot.command, sizeof(boot.command) - 1, commandMsg.c_str(), commandMsg.size()), 0);
    EXPECT_EQ(strncpy_s(boot.update, sizeof(boot.update) - 1, updateMsg.c_str(), updateMsg.size()), 0);
    bool bRet = WriteUpdaterMessage(commandFile, boot);
    EXPECT_EQ(bRet, true);
    char **argv = new char*[1];
    argv[0] = new char[MAX_ARG_SIZE];
    EXPECT_EQ(strncpy_s(argv[0], MAX_ARG_SIZE, "./UpdaterMain", MAX_ARG_SIZE), 0);
    int argc = 1;
    EXPECT_EQ(UpdaterMain(argc, argv), 0);
    delete argv[0];
    delete []argv;
}

HWTEST_F(UpdaterUtilUnitTest, DoInstallUpdaterPackageTest, TestSize.Level1)
{
    UpdaterParams upParams;
    upParams.callbackProgress = nullptr;
    std::vector<std::string> output;
    EXPECT_EQ(DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE), UPDATE_CORRUPT);
    upParams.callbackProgress = [] (float value) {};
    EXPECT_EQ(DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE), UPDATE_CORRUPT);
    upParams.retryCount = 0;
    EXPECT_EQ(DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE), UPDATE_CORRUPT);
    upParams.retryCount = 1;
    EXPECT_EQ(DoInstallUpdaterPackage(nullptr, upParams, HOTA_UPDATE), UPDATE_CORRUPT);
}

HWTEST_F(UpdaterUtilUnitTest, updater_ExtractUpdaterBinary, TestSize.Level1)
{
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    std::string path = "xxx";
    int32_t ret = ExtractUpdaterBinary(pkgManager, path, UPDATER_BINARY);
    EXPECT_EQ(ret, 1);
    path = "/data/updater/updater/updater_full.zip";
    ret = ExtractUpdaterBinary(pkgManager, path, UPDATER_BINARY);
    Hpackage::PkgManager::ReleasePackageInstance(pkgManager);
    EXPECT_EQ(ret, 1);
}

HWTEST_F(UpdaterUtilUnitTest, updater_IsSpaceCapacitySufficient, TestSize.Level1)
{
    std::vector<std::string> packagePath;
    UpdaterStatus status = IsSpaceCapacitySufficient(packagePath);
    EXPECT_EQ(status, UPDATE_ERROR);
    packagePath.push_back("/data/updater/updater/updater_full.zip");
    status = IsSpaceCapacitySufficient(packagePath);
    EXPECT_EQ(status, UPDATE_SUCCESS);
    packagePath.push_back("xxx");
    ProgressSmoothHandler(0, 0);
    ProgressSmoothHandler(-1, 0);
    ProgressSmoothHandler(0, 1);
    status = IsSpaceCapacitySufficient(packagePath);
    EXPECT_EQ(status, UPDATE_ERROR);
}

HWTEST_F(UpdaterUtilUnitTest, updater_HandleChildOutput, TestSize.Level1)
{
    std::string buf = "xxx";
    bool retryUpdate = false;
    UpdaterParams upParams;
    HandleChildOutput(buf, 0, retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, false);
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, false);
    buf = "write_log:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, false);
    buf = "retry_update:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, true);
    buf = "ui_log:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, true);
    buf = "show_progress:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, true);
    buf = "show_progress:xxx:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, true);
    buf = "set_progress:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, true);
    buf = "xxx:xxx";
    HandleChildOutput(buf, buf.size(), retryUpdate, upParams);
    EXPECT_EQ(retryUpdate, true);
}

HWTEST_F(UpdaterUtilUnitTest, InstallUpdaterPackageTest, TestSize.Level1)
{
    UpdaterParams upParams;
    upParams.retryCount = 0;
    upParams.callbackProgress = [] (float value) { UPDATER_UI_INSTANCE.ShowProgress(value); };
    upParams.updatePackage.push_back("/data/updater/updater/updater_full.zip");
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    EXPECT_EQ(InstallUpdaterPackage(upParams, pkgManager), UPDATE_ERROR);
    upParams.updateMode = SDCARD_UPDATE;
    upParams.retryCount = 1;
    EXPECT_EQ(InstallUpdaterPackage(upParams, pkgManager), UPDATE_ERROR);
}

HWTEST_F(UpdaterUtilUnitTest, DoUpdatePackagesTest, TestSize.Level1)
{
    UpdaterParams upParams;
    EXPECT_EQ(DoUpdatePackages(upParams), UPDATE_ERROR);
    upParams.updatePackage.push_back("/data/updater/updater/updater_full.zip");
    EXPECT_EQ(DoUpdatePackages(upParams), UPDATE_CORRUPT);
    upParams.callbackProgress = [] (float value) { UPDATER_UI_INSTANCE.ShowProgress(value); };
    upParams.installTime.push_back(std::chrono::duration<double>(0));
    EXPECT_EQ(DoUpdatePackages(upParams), UPDATE_ERROR);
}

HWTEST_F(UpdaterUtilUnitTest, StartUpdaterEntryTest, TestSize.Level1)
{
    UpdaterParams upParams;
    upParams.factoryWipeData = true;
    EXPECT_EQ(DoUpdatePackages(upParams), UPDATE_ERROR);
    upParams.factoryWipeData = false;
    upParams.userWipeData = true;
    EXPECT_EQ(DoUpdatePackages(upParams), UPDATE_ERROR);
    upParams.userWipeData = false;
    EXPECT_EQ(DoUpdatePackages(upParams), UPDATE_ERROR);
}

HWTEST_F(UpdaterUtilUnitTest, StartUpdaterProcTest, TestSize.Level1)
{
    Hpackage::PkgManager::PkgManagerPtr pkgManager = Hpackage::PkgManager::CreatePackageInstance();
    UpdaterParams upParams;
    int maxTemperature = 0;
    EXPECT_EQ(StartUpdaterProc(nullptr, upParams, maxTemperature), UPDATE_CORRUPT);
    EXPECT_EQ(StartUpdaterProc(pkgManager, upParams, maxTemperature), UPDATE_ERROR);
}
}