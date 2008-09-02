// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"

namespace file_util {

const wchar_t kPathSeparator = L'/';

bool GetTempDir(std::wstring* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp)
    *path = UTF8ToWide(tmp);
  else
    *path = L"/tmp";
  return true;
}

bool CopyFile(const std::wstring& from_path, const std::wstring& to_path) {
  // TODO(erikkay): implement
  NOTIMPLEMENTED();
  return false;
}

}  // namespace file_util
