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

#include "webkit/glue/plugins/test/plugin_test.h"
#include "webkit/glue/plugins/test/npapi_constants.h"

namespace NPAPIClient {

PluginTest::PluginTest(NPP id, NPNetscapeFuncs *host_functions) {
  id_ = id;
  id_->pdata = this;
  host_functions_ = host_functions;
}

NPError PluginTest::New(uint16 mode, int16 argc, const char* argn[],
                        const char* argv[], NPSavedData* saved) {
  test_name_ = this->GetArgValue("name", argc, argn, argv);
  test_id_ = this->GetArgValue("id", argc, argn, argv);
  return NPERR_NO_ERROR;
}

NPError PluginTest::SetWindow(NPWindow* pNPWindow) {
  return NPERR_NO_ERROR;
}

// It's a shame I have to implement URLEncode.  But, using webkit's
// or using chrome's means a ball of string of dlls and dependencies that
// is very very long.  After spending far too much time on it,
// I'll just encode it myself.  Too bad Microsoft doesn't implement
// this in a reusable way either.  Both webkit and chrome will
// end up using libicu, which is a string of dependencies we don't
// want.

inline BYTE toHex(const BYTE &x) {
    return x > 9 ? x + 55: x + 48;
}

std::string URLEncode(const std::string &sIn) {
  std::string sOut;

  const size_t length = sIn.length();
  for (size_t idx = 0; idx < length;) {
    const char ch = sIn.at(idx);
    if (isalnum(ch)) {
      sOut.append(1, ch);
    } else if (isspace(ch) && ((ch != '\n') && (ch != '\r'))) {
      sOut.append(1, '+');
    } else {
      sOut.append(1, '%');
      sOut.append(1, toHex(ch>>4));
      sOut.append(1, toHex(ch%16));
    }
    idx++;
  }
  return sOut;
}

void PluginTest::SignalTestCompleted() {
  // To signal test completion, we expect a couple of
  // javascript functions to be defined in the webpage
  // which hosts this plugin:
  //    onSuccess(test_name, test_id)
  //    onFailure(test_name, test_id, error_message)
  std::string script_result;
  std::string script_url;
  if (Succeeded()) {
    script_url.append("onSuccess(\"");
    script_url.append(test_name_);
    script_url.append("\",\"");
    script_url.append(test_id_);
    script_url.append("\");");
  } else {
    script_url.append("onFailure(\"");
    script_url.append(test_name_);
    script_url.append("\",\"");
    script_url.append(test_id_);
    script_url.append("\",\"");
    script_url.append(test_status_);
    script_url.append("\");");
  }
  script_url = URLEncode(script_url);
  script_result.append("javascript:");
  script_result.append(script_url);
  host_functions_->geturl(id_, script_result.c_str(), "_self");
}

const char *PluginTest::GetArgValue(const char *name, const int16 argc,
                                    const char *argn[], const char *argv[]) {
  for (int idx = 0; idx < argc; idx++)
    if (_stricmp(argn[idx], name) == 0)
      return argv[idx];
  return NULL;
}

void PluginTest::SetError(const std::string &msg) {
  test_status_.append(msg);
}

NPError PluginTest::NewStream(NPMIMEType type, NPStream* stream,
                              NPBool seekable, uint16* stype) {
  // There is no default action here.
  return NPERR_NO_ERROR;
}

int32 PluginTest::WriteReady(NPStream *stream) {
  // Take data in small chunks
  return 4096;
}

int32 PluginTest::Write(NPStream *stream, int32 offset, int32 len,
                           void *buffer) {
  // Pretend that we took all the data.
  return len;
}

NPError PluginTest::DestroyStream(NPStream *stream, NPError reason) {
  // There is no default action.
  return NPERR_NO_ERROR;
}

void PluginTest::StreamAsFile(NPStream* stream, const char* fname) {
  // There is no default action.
}

void PluginTest::URLNotify(const char* url, NPReason reason, void* data) {
  // There is no default action
}

int16 PluginTest::HandleEvent(void* event) {
  // There is no default action
  return 0;
}

} // namespace NPAPIClient
