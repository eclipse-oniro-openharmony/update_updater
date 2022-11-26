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

#include "applypatch_unittest.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "applypatch/data_writer.h"
#include "fs_manager/mount.h"
#include "log/log.h"
#include "securec.h"
#include "unittest_comm.h"
#include "utils.h"

namespace UpdaterUt {
using namespace testing::ext;
using namespace Updater;
using namespace std;
using namespace testing;
constexpr unsigned int BUFFER_LEN = 12;

void ApplyPatchUnitTest::SetUp()
{
    unsigned long mountFlag = MS_REMOUNT;
    std::string tmpPath = "/tmp";
    // mount rootfs to read-write.
    std::string rootSource = "/dev/root";
    if (mount(rootSource.c_str(), "/", "ext4", mountFlag, nullptr) != 0) {
        std::cout << "Cannot re-mount rootfs\n";
    }
    auto ret = mkdir(tmpPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (ret != 0) {
        std::cout << "Cannot create \"/tmp\" directory: " << errno << "\n";
    }

    // Load specific fstab for testing.
    LoadSpecificFstab("/data/updater/applypatch/etc/fstab.ut.updater");
}

void ApplyPatchUnitTest::TearDown()
{
    std::string partitionName = "/rawwriter";
    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    unlink(devPath.c_str());
}

HWTEST_F(ApplyPatchUnitTest, updater_RawWriter, TestSize.Level1)
{
    WriteMode mode = WRITE_RAW;
    uint8_t *addr = nullptr;
    uint8_t buf[BUFFER_LEN + 1] = {0};

    std::string partitionName = "/rawwriter";
    std::string filePath = "/data/updater/ut/datawriter/rawwrite";
    std::unique_ptr<DataWriter> writer = DataWriter::CreateDataWriter(mode, filePath);
    EXPECT_NE(writer, nullptr);
    bool ret = writer->Write(addr, 0, nullptr);
    EXPECT_FALSE(ret);

    addr = buf;
    ret = writer->Write(addr, 0, nullptr);
    EXPECT_FALSE(ret);

    ret = writer->Write(buf, BUFFER_LEN, nullptr);
    EXPECT_FALSE(ret);

    int mRet = memcpy_s(buf, BUFFER_LEN, "hello, world", BUFFER_LEN);
    EXPECT_EQ(mRet, 0);
    auto devPath = GetBlockDeviceByMountPoint(partitionName);
    const std::string devDir = "/data/updater/ut/datawriter";
    Updater::Utils::MkdirRecursive(devDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    close(open(devPath.c_str(), O_CREAT | O_WRONLY | O_EXCL, 0664));
    ret = writer->Write(buf, BUFFER_LEN, nullptr);
    EXPECT_TRUE(ret);

    int fd  = open(devPath.c_str(), O_RDONLY);
    EXPECT_GE(fd, 0);

    uint8_t buffer[BUFFER_LEN + 1] = {0};
    size_t n = read(fd, buffer, BUFFER_LEN);
    EXPECT_EQ(n, BUFFER_LEN);

    auto result = memcmp(buf, buffer, BUFFER_LEN);
    EXPECT_EQ(result, 0);
    DataWriter::ReleaseDataWriter(writer);
}

HWTEST_F(ApplyPatchUnitTest, updater_CreateDataWriter, TestSize.Level1)
{
    std::vector<WriteMode> modes = { WRITE_RAW, WRITE_DECRYPT };
    std::unique_ptr<DataWriter> writer = nullptr;
    for (auto mode : modes) {
        if (mode == WRITE_DECRYPT) {
            EXPECT_EQ(writer, nullptr);
            continue;
        }
        writer = DataWriter::CreateDataWriter(mode, "", DataWriter::GetUpdaterEnv());
        EXPECT_NE(writer, nullptr);
        DataWriter::ReleaseDataWriter(writer);
        writer = nullptr;
    }
}
} // namespace updater_ut
