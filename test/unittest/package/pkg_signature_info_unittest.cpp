 /*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include "log.h"
#include "pkcs7_signed_data.h"

using namespace testing::ext;
using namespace Hpackage;
using namespace Updater;

namespace UpdaterUt {
class PkgSignatureTest : public ::testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    void SetUp() override;
    void TearDown() override;

private:
};

void PkgSignatureTest::SetUpTestSuite()
{
}

void PkgSignatureTest::TearDownTestSuite()
{
}

void PkgSignatureTest::SetUp()
{
}

void PkgSignatureTest::TearDown()
{
}

HWTEST_F(PkgSignatureTest, TestDefaultPkcs7VerifyHelper, TestSize.Level1)
{
    std::vector<uint8_t> digestBlock{};  // input
    SignatureInfo signatureInfo{};       // output
    std::vector<uint8_t> digest{};       // output

    EXPECT_EQ(Pkcs7SignedData::GetInstance().GetDigest(digestBlock, signatureInfo, digest), -1);
    EXPECT_TRUE(signatureInfo.overall.buffer.empty());
    EXPECT_EQ(signatureInfo.overall.length, 0);
    EXPECT_TRUE(signatureInfo.hashResult.buffer.empty());
    EXPECT_EQ(signatureInfo.hashResult.length, 0);
    EXPECT_EQ(signatureInfo.nid, 0);
}
}  // namespace UpdaterUt
