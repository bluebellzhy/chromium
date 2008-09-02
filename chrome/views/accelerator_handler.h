// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_ACCELERATOR_HANDLER_H__
#define CHROME_VIEWS_ACCELERATOR_HANDLER_H__

#include "base/message_loop.h"

namespace ChromeViews {

// This class delegates WM_KEYDOWN and WM_SYSKEYDOWN messages to
// the associated FocusManager class for the window that is receiving
// these messages for accelerator processing. The BrowserProcess object
// holds a singleton instance of this class which can be used by other
// custom message loop dispatcher objects to implement default accelerator
// handling.
class AcceleratorHandler : public MessageLoopForUI::Dispatcher {
 public:
   AcceleratorHandler();
  // Dispatcher method. This returns true if an accelerator was
  // processed by the focus manager
  virtual bool Dispatch(const MSG& msg);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(AcceleratorHandler);
};
}
#endif  // CHROME_VIEWS_ACCELERATOR_HANDLER_H__

