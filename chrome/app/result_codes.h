// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_RESULT_CODES_H__
#define CHROME_APP_RESULT_CODES_H__

// This file consolidates all the return codes for the browser and renderer
// process. The return code is the value that:
// a) is returned by main() or winmain(), or
// b) specified in the call for ExitProcess() or TerminateProcess(), or
// c) the exception value that causes a process to terminate.
//
// It is advisable to not use negative numbers because the Windows API returns
// it as an unsigned long and the exception values have high numbers. For
// example EXCEPTION_ACCESS_VIOLATION value is 0xC0000005.

class ResultCodes {
 public:
  enum ExitCode {
    NORMAL_EXIT = 0,            // Normal termination. Keep it *always* zero.
    INVALID_CMDLINE_URL,        // An invalid command line url was given.
    SBOX_INIT_FAILED,           // The sandbox could not be initialized.
    GOOGLE_UPDATE_INIT_FAILED,  // The Google Update client stub init failed.
    GOOGLE_UPDATE_LAUNCH_FAILED,// Google Update could not launch chrome DLL.
    BAD_PROCESS_TYPE,           // The process is of an unknown type.
    MISSING_PATH,               // An critical chrome path is missing.
    MISSING_DATA,               // A critical chrome file is missing.
    SHELL_INTEGRATION_FAILED,   // Failed to make Chrome default browser.
    UNINSTALL_DELETE_FILE_ERROR,// Error while deleting shortcuts.
    UNINSTALL_CHROME_ALIVE,     // Uninstall detected another chrome instance.
    UNINSTALL_NO_SURVEY,        // Do not launch survey after uninstall.
    UNINSTALL_USER_CANCEL,      // The user changed her mind.
    UNSUPPORTED_PARAM,          // Command line parameter is not supported.
    KILLED_BAD_MESSAGE,         // A bad message caused the process termination.
    IMPORTER_CANCEL,            // The user canceled the browser import.
    HUNG,                       // Browser was hung and killed.
    IMPORTER_HUNG,              // Browser import hung and was killed.
    EXIT_LAST_CODE              // Last return code (keep it last).
  };
};

#endif  // CHROME_APP_RESULT_CODES_H__

