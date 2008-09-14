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

#include <windows.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  wchar_t test_root[] = L"TempTemp";
  wchar_t data_str_1[] = L"data_111";
  wchar_t data_str_2[] = L"data_222";
  DWORD dword1 = 0;
  DWORD dword2 = 1;
  class SetRegValueWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Create a temporary key for testing
      RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
      key.DeleteKey(test_root);
      ASSERT_FALSE(key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
      ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, test_root, KEY_READ));

      // Create a log file
      std::wstring log_file;
      ASSERT_TRUE(file_util::CreateTemporaryFileName(&log_file));
      ASSERT_TRUE(file_util::PathExists(log_file));

      logging::InitLogging(log_file.c_str(),
                           logging::LOG_ONLY_TO_FILE,
                           logging::LOCK_LOG_FILE,
                           logging::DELETE_OLD_LOG_FILE);
      logging::SetMinLogLevel(logging::LOG_INFO);
    }
    virtual void TearDown() {
      logging::CloseLogFile();
      // Clean up the temporary key
      RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
      ASSERT_TRUE(key.DeleteKey(test_root));
    }
  };
};

// Write a new value without overwrite flag. The value should be set.
TEST_F(SetRegValueWorkItemTest, WriteNewNonOverwrite) {
  RegKey key;

  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteNewNonOverwrite");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ));

  std::wstring name_str(L"name_str");
  std::wstring data_str(data_str_1);
  scoped_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_str, data_str, false));

  std::wstring name_dword(L"name_dword");
  scoped_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_dword, dword1, false));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  std::wstring read_out;
  DWORD read_dword;
  EXPECT_TRUE(key.ReadValue(name_str.c_str(), &read_out));
  EXPECT_TRUE(key.ReadValueDW(name_dword.c_str(), &read_dword));
  EXPECT_EQ(read_out, data_str_1);
  EXPECT_EQ(read_dword, dword1);

  work_item1->Rollback();
  work_item2->Rollback();

  // Rollback should delete the value.
  EXPECT_FALSE(key.ValueExists(name_str.c_str()));
  EXPECT_FALSE(key.ValueExists(name_dword.c_str()));
}

// Write a new value with overwrite flag. The value should be set.
TEST_F(SetRegValueWorkItemTest, WriteNewOverwrite) {
  RegKey key;

  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteNewOverwrite");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ));

  std::wstring name_str(L"name_str");
  std::wstring data_str(data_str_1);
  scoped_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_str, data_str, true));

  std::wstring name_dword(L"name_dword");
  scoped_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_dword, dword1, true));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  std::wstring read_out;
  DWORD read_dword;
  EXPECT_TRUE(key.ReadValue(name_str.c_str(), &read_out));
  EXPECT_TRUE(key.ReadValueDW(name_dword.c_str(), &read_dword));
  EXPECT_EQ(read_out, data_str_1);
  EXPECT_EQ(read_dword, dword1);

  work_item1->Rollback();
  work_item2->Rollback();

  // Rollback should delete the value.
  EXPECT_FALSE(key.ValueExists(name_str.c_str()));
  EXPECT_FALSE(key.ValueExists(name_dword.c_str()));
}

// Write to an existing value without overwrite flag. There should be
// no change.
TEST_F(SetRegValueWorkItemTest, WriteExistingNonOverwrite) {
  RegKey key;

  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteExistingNonOverwrite");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, parent_key.c_str(),
                         KEY_READ | KEY_SET_VALUE));

  // First test REG_SZ value.
  // Write data to the value we are going to set.
  std::wstring name(L"name_str");
  ASSERT_TRUE(key.WriteValue(name.c_str(), data_str_1));

  std::wstring data(data_str_2);
  scoped_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name, data, false));
  EXPECT_TRUE(work_item->Do());

  std::wstring read_out;
  EXPECT_TRUE(key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(data_str_1));

  work_item->Rollback();
  EXPECT_TRUE(key.ValueExists(name.c_str()));
  EXPECT_TRUE(key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(read_out, data_str_1);

  // Now test REG_DWORD value.
  // Write data to the value we are going to set.
  name.assign(L"name_dword");
  ASSERT_TRUE(key.WriteValue(name.c_str(), dword1));
  work_item.reset(WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
      parent_key, name, dword2, false));
  EXPECT_TRUE(work_item->Do());

  DWORD read_dword;
  EXPECT_TRUE(key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword1);

  work_item->Rollback();
  EXPECT_TRUE(key.ValueExists(name.c_str()));
  EXPECT_TRUE(key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword1);
}

// Write to an existing value with overwrite flag. The value should be
// overwritten.
TEST_F(SetRegValueWorkItemTest, WriteExistingOverwrite) {
  RegKey key;

  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteExistingOverwrite");
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, parent_key.c_str(),
                         KEY_READ | KEY_SET_VALUE));

  // First test REG_SZ value.
  // Write data to the value we are going to set.
  std::wstring name(L"name_str");
  ASSERT_TRUE(key.WriteValue(name.c_str(), data_str_1));

  std::wstring data(data_str_2);
  scoped_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name, data, true));
  EXPECT_TRUE(work_item->Do());

  std::wstring read_out;
  EXPECT_TRUE(key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(data_str_2));

  work_item->Rollback();
  EXPECT_TRUE(key.ValueExists(name.c_str()));
  EXPECT_TRUE(key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(read_out, data_str_1);

  // Now test REG_DWORD value.
  // Write data to the value we are going to set.
  name.assign(L"name_dword");
  ASSERT_TRUE(key.WriteValue(name.c_str(), dword1));
  work_item.reset(WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
      parent_key, name, dword2, true));
  EXPECT_TRUE(work_item->Do());

  DWORD read_dword;
  EXPECT_TRUE(key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword2);

  work_item->Rollback();
  EXPECT_TRUE(key.ValueExists(name.c_str()));
  EXPECT_TRUE(key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword1);
}

// Write a value to a non-existing key. This should fail.
TEST_F(SetRegValueWorkItemTest, WriteNonExistingKey) {
  RegKey key;

  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteNonExistingKey");

  std::wstring name(L"name");
  std::wstring data(data_str_1);
  scoped_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name, data, false));
  EXPECT_FALSE(work_item->Do());

  work_item.reset(WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
      parent_key, name, dword1, false));
  EXPECT_FALSE(work_item->Do());
}
