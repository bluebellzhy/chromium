// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_
#define CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_

#include "chrome/common/ipc_message.h"
#include "dom_ui_bindings.h"

// ExternalHostBindings is the class backing the "externalHost" object
// accessible from Javascript
//
// We expose one function, for sending a message to the external host:
//  ForwardMessageToExternalHost(String receiver, String message);
class ExternalHostBindings : public DOMBoundBrowserObject {
 public:
  ExternalHostBindings() { BindMethods(); }
  virtual ~ExternalHostBindings() {};

  // DOMBoundBrowserObject implementation.
  virtual void BindMethods();

  // The ForwardMessageToExternalHost() function provided to Javascript.
  void ForwardMessageToExternalHost(const CppArgumentList& args,
                                    CppVariant* result);
 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalHostBindings);
};

#endif  // CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_

