// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for GoogleChromeDistribution class.

#include <windows.h>

#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class GoogleChromeDistributionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Currently no setup required.
  }

  virtual void TearDown() {
    // Currently no tear down required.
  }

  // Creates "ap" key with the value given as parameter. Also adds work
  // items to work_item_list given so that they can be rollbed back later.
  bool CreateApKey(WorkItemList* work_item_list, std::wstring value) {
    HKEY reg_root = HKEY_CURRENT_USER;
    std::wstring reg_key = GetApKeyPath();
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    work_item_list->AddSetRegValueWorkItem(reg_root, reg_key,
        google_update::kRegApFieldName, value.c_str(), true);
    if (!work_item_list->Do()) {
      work_item_list->Rollback();
      return false;
    }
    return true;
  }

  // Returns the key path of "ap" key Google\Update\ClientState\<chrome-guid>
  std::wstring GetApKeyPath() {
    std::wstring reg_key(google_update::kRegPathClientState);
    reg_key.append(L"\\");
    reg_key.append(google_update::kChromeGuid);
    return reg_key;
  }

  // Utility method to read "ap" key value
  std::wstring ReadApKeyValue() {
    RegKey key;
    std::wstring ap_key_value;
    std::wstring reg_key = GetApKeyPath();
    if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) && 
        key.ReadValue(google_update::kRegApFieldName, &ap_key_value)) {
      return ap_key_value;
    }
    return std::wstring();
  }
};
}  // namespace

#if defined(GOOGLE_CHROME_BUILD)
TEST_F(GoogleChromeDistributionTest, GetNewGoogleUpdateApKeyTest) {
  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetDistribution());
  installer_util::InstallStatus s = installer_util::FIRST_INSTALL_SUCCESS;
  installer_util::InstallStatus f = installer_util::INSTALL_FAILED;

  // Incremental Installer that worked.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Incremental Installer that failed.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L""), L"-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1"), L"1.1-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-dev"),
            L"1.1-dev-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"-full"), L"-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-full"), L"1.1-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-dev-full"),
            L"1.1-dev-full");

  // Full Installer that worked.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Full Installer that failed.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-dev-full"),
            L"1.1-dev");
}

TEST_F(GoogleChromeDistributionTest, UpdateDiffInstallStatusTest) {
  // Get Google Chrome distribution
  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetDistribution());

  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  // Test incremental install failure
  if (!CreateApKey(work_item_list.get(), L""))
    GTEST_FATAL_FAILURE("Failed to create ap key.");
  dist->UpdateDiffInstallStatus(false, true, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test incremental install success
  if (!CreateApKey(work_item_list.get(), L""))
    GTEST_FATAL_FAILURE("Failed to create ap key.");
  dist->UpdateDiffInstallStatus(false, true,
                                installer_util::FIRST_INSTALL_SUCCESS);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install failure
  if (!CreateApKey(work_item_list.get(), L"-full"))
    GTEST_FATAL_FAILURE("Failed to create ap key.");
  dist->UpdateDiffInstallStatus(false, false, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install success
  if (!CreateApKey(work_item_list.get(), L"-full"))
    GTEST_FATAL_FAILURE("Failed to create ap key.");
  dist->UpdateDiffInstallStatus(false, false,
                                installer_util::FIRST_INSTALL_SUCCESS);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test the case of when "ap" key doesnt exist at all
  std::wstring ap_key_value = ReadApKeyValue();
  std::wstring reg_key = GetApKeyPath();
  HKEY reg_root = HKEY_CURRENT_USER;
  bool ap_key_deleted = false;
  RegKey key;
  if (!key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS)){
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    if (!work_item_list->Do())
      GTEST_FATAL_FAILURE("Failed to create ClientState key.");
  } else if (key.DeleteValue(google_update::kRegApFieldName)) {
    ap_key_deleted = true;
  }
  // try differential installer
  dist->UpdateDiffInstallStatus(false, true, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  // try full installer now
  dist->UpdateDiffInstallStatus(false, false, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  // Now cleanup to leave the system in unchanged state.
  // - Diff installer creates an ap key if it didnt exist, so delete this ap key
  // - If we created any reg key path for ap, roll it back
  // - Finally restore the original value of ap key.
  key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS);
  key.DeleteValue(google_update::kRegApFieldName);
  work_item_list->Rollback();
  if (ap_key_deleted) {
    work_item_list.reset(WorkItem::CreateWorkItemList());
    if (!CreateApKey(work_item_list.get(), ap_key_value))
      GTEST_FATAL_FAILURE("Failed to restore ap key.");
  }

}
#endif

