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

#include "blockset_unittest.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <vector>
#include "applypatch/block_set.h"
#include "applypatch/command.h"
#include "log/log.h"

using namespace testing::ext;
using namespace UpdaterUt;
using namespace Updater;
using namespace std;

namespace UpdaterUt {
void BlockSetUnitTest::SetUp(void)
{
    cout << "SetUpTestCase" << endl;
}

void BlockSetUnitTest::TearDown(void)
{
    cout << "TearDownTestCase" << endl;
}

HWTEST_F(BlockSetUnitTest, blockset_test_001, TestSize.Level1)
{
    cout << "Blockset ut start";
    BlockSet block(std::vector<BlockPair> {BlockPair{0, 1}});
    cout << "Blockset ut init end";
    size_t countOfRanges = block.CountOfRanges();
    cout << "Blockset ranges: " << countOfRanges;
    auto itBegin = block.Begin();
    auto itEnd = block.End();
    auto itCBegin = block.CBegin();
    auto itCEnd = block.CEnd();
    auto itCrBegin = block.CrBegin();
    auto itCrEnd = block.CrEnd();
    if (itBegin != itEnd)
    cout << "Right iterator";
    if (itCBegin != itCEnd)
    cout << "Right iterator";
    if (itCrBegin != itCrEnd)
    cout << "Right iterator";
    std::vector<uint8_t> buffer;
    buffer.resize(H_BLOCK_SIZE);
    std::fill(buffer.begin(), buffer.end(), 0);
    string sha256 = "fdfasdf";
    auto ret = block.VerifySha256(buffer, block.TotalBlockSize(), sha256);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(BlockSetUnitTest, blockset_test_002, TestSize.Level1)
{
    cout << "Blockset ut two blocks overlap";
    BlockSet block(std::vector<BlockPair> {BlockPair{0, 1}});
    BlockSet block2(std::vector<BlockPair> {BlockPair{0, 1}});
    BlockSet block3(std::vector<BlockPair> {BlockPair{2, 3}});
    bool ret = BlockSet::IsTwoBlocksOverlap(block, block2);
    EXPECT_EQ(ret, true);
    ret = BlockSet::IsTwoBlocksOverlap(block, block3);
    EXPECT_EQ(ret, false);
}

HWTEST_F(BlockSetUnitTest, blockset_test_003, TestSize.Level1)
{
    cout << "Blockset ut two blocks overlap";
    std::vector<uint8_t> buffer;
    buffer.resize(H_BLOCK_SIZE);
    BlockSet blk(std::vector<BlockPair> {BlockPair{0, 1}});
    std::fill(buffer.begin(), buffer.end(), 0);
    std::string filename = "/tmp/ut_blockset";
    int fd = open(filename.c_str(), O_RDWR);
    if (fd < 0) {
        printf("Open file failed");
        return;
    }
    blk.WriteDataToBlock(fd, buffer);
    close(fd);
}

HWTEST_F(BlockSetUnitTest, blockset_test_004, TestSize.Level1)
{
    cout << "Blockset ut two blocks overlap";
    std::vector<uint8_t> srcBuffer;
    srcBuffer.resize(H_BLOCK_SIZE);
    std::vector<uint8_t> tgtBuffer;
    tgtBuffer.resize(H_BLOCK_SIZE);
    BlockSet blk(std::vector<BlockPair> {BlockPair{0, 1}});
    std::fill(srcBuffer.begin(), srcBuffer.end(), 0);
    std::fill(tgtBuffer.begin(), tgtBuffer.end(), 0);
    BlockSet::MoveBlock(srcBuffer, blk, tgtBuffer);
}

HWTEST_F(BlockSetUnitTest, blockset_test_005, TestSize.Level1)
{
    std::string hashValue = "5aa246ebe8e817740f12cc0f6e536c5ea22e5db177563a1caea5a86614275546";
    std::string blockInfo = "2,20755,21031 276 2,20306,20582";
    std::string cmdLine = std::string("move ") + hashValue + " " + blockInfo;
    int fd = open("/data/updater/updater/blocksetTest.txt", O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd < 0) {
        printf("Open file failed");
        return;
    }
    Command *cmd = new Command();
    cmd->Init(cmdLine);
    cmd->SetFileDescriptor(fd);
    BlockSet targetBlock;
    size_t tgtBlockSize = H_BLOCK_SIZE;
    std::vector<uint8_t> buffer(tgtBlockSize);
    bool isImgDiff = true;
    int ret = targetBlock.WriteDiffToBlock(const_cast<const Command &>(*cmd), buffer, tgtBlockSize, isImgDiff);
    EXPECT_EQ(ret, -1);
    isImgDiff = false;
    ret = targetBlock.WriteDiffToBlock(const_cast<const Command &>(*cmd), buffer, tgtBlockSize, isImgDiff);
    EXPECT_EQ(ret, -1);
    close(fd);
    delete cmd;
}

HWTEST_F(BlockSetUnitTest, blockset_test_006, TestSize.Level1)
{
    int fd = open("/data/updater/updater/blocksetTest.txt", O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd < 0) {
        printf("Open file failed");
        return;
    }
    std::vector<uint8_t> buffer;
    buffer.resize(H_BLOCK_SIZE);
    BlockSet myBlock;
    int ret = myBlock.ReadDataFromBlock(fd, buffer);
    EXPECT_EQ(ret, 0);
    BlockSet myBlock2({std::vector<BlockPair>{}});
    BlockSet myBlock3({std::vector<BlockPair>{BlockPair{0, 1}}});
    BlockSet myBlock4({std::vector<BlockPair>{BlockPair{-1, 0}}});
    ret = myBlock3.ReadDataFromBlock(fd, buffer);
    EXPECT_EQ(ret, -1);
    ret = myBlock3.WriteDataToBlock(fd, buffer);
    EXPECT_EQ(ret, 4096);
    ret = myBlock4.ReadDataFromBlock(fd, buffer);
    EXPECT_EQ(ret, 0);
    close(fd);
}
}
