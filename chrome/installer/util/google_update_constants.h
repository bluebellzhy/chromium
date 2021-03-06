// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used with Google Update.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H__
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H__

namespace google_update {

extern const wchar_t kChromeGuid[];

// Strictly speaking Google Update doesn't care about this GUID but it is still
// related to install as it is used by MSI to identify Gears.
extern const wchar_t kGearsUpgradeCode[];

extern const wchar_t kRegPathClients[];
extern const wchar_t kRegPathClientState[];
extern const wchar_t kRegApFieldName[];
extern const wchar_t kRegNameField[];
extern const wchar_t kRegVersionField[];
extern const wchar_t kRegLastCheckedField[];

}  // namespace google_update

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H__

