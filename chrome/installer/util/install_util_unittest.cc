// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Unit tests for InstallUtil class.

#include <windows.h>

#include "chrome/installer/util/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class InstallUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Currently no setup required.
  }

  virtual void TearDown() {
    // Currently no tear down required.
  }
};
}  // namespace

TEST_F(InstallUtilTest, GetNewGoogleUpdateApKeyTest) {
  installer_util::InstallStatus s = installer_util::FIRST_INSTALL_SUCCESS;
  installer_util::InstallStatus f = installer_util::INSTALL_FAILED;

  // Incremental Installer that worked.
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, s, L""), L"");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, s, L"1.1"), L"1.1");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, s, L"1.1-dev"),
            L"1.1-dev");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, s, L"-full"), L"");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, s, L"1.1-full"),
            L"1.1");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Incremental Installer that failed.
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, f, L""), L"-full");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, f, L"1.1"), L"1.1-full");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, f, L"1.1-dev"),
            L"1.1-dev-full");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, f, L"-full"), L"-full");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, f, L"1.1-full"),
            L"1.1-full");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(true, f, L"1.1-dev-full"),
            L"1.1-dev-full");

  // Full Installer that worked.
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, s, L""), L"");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, s, L"1.1"), L"1.1");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, s, L"1.1-dev"),
            L"1.1-dev");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, s, L"-full"), L"");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, s, L"1.1-full"),
            L"1.1");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Full Installer that failed.
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, f, L""), L"");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, f, L"1.1"), L"1.1");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, f, L"1.1-dev"),
            L"1.1-dev");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, f, L"-full"), L"");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, f, L"1.1-full"),
            L"1.1");
  EXPECT_EQ(InstallUtil::GetNewGoogleUpdateApKey(false, f, L"1.1-dev-full"),
            L"1.1-dev");
}
