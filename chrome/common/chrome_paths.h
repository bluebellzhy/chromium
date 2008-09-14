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

#ifndef CHROME_COMMON_CHROME_PATHS_H__
#define CHROME_COMMON_CHROME_PATHS_H__

// This file declares path keys for the chrome module.  These can be used with
// the PathService to access various special directories and files.

namespace chrome {

enum {
  PATH_START = 1000,

  DIR_APP = PATH_START,  // directory where dlls and data reside
  DIR_LOGS,              // directory where logs should be written
  DIR_USER_DATA,         // directory where user data can be written
  DIR_CRASH_DUMPS,       // directory where crash dumps are written
  DIR_USER_DESKTOP,      // directory that correspond to the desktop
  DIR_RESOURCES,         // directory where application resources are stored
  DIR_INSPECTOR,         // directory where web inspector is located
  DIR_THEMES,            // directory where theme dll files are stored
  DIR_LOCALES,           // directory where locale resources are stored
  DIR_APP_DICTIONARIES,  // directory where the global dictionaries are
  DIR_USER_DOCUMENTS,    // directory for a user's "My Documents"
  FILE_RESOURCE_MODULE,  // full path and filename of the module that contains
                         // embedded resources (version, strings, images, etc.)
  FILE_LOCAL_STATE,      // path and filename to the file in which machine/
                         // installation-specific state is saved
  FILE_RECORDED_SCRIPT,  // full path to the script.log file that contains
                         // recorded browser events for playback.
  FILE_GEARS_PLUGIN,     // full path to the gears.dll plugin file.

  // Valid only in development environment; TODO(darin): move these
  DIR_TEST_DATA,         // directory where unit test data resides
  DIR_TEST_TOOLS,        // directory where unit test tools reside
  FILE_TEST_SERVER,      // simple HTTP server for testing the network stack
  FILE_PYTHON_RUNTIME,   // Python runtime, used for running the test server

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_H__
