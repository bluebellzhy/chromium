// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "webkit/tools/test_shell/webview_host.h"

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webview.h"

/*static*/
WebViewHost* WebViewHost::Create(GtkWidget* parent_window,
                                 WebViewDelegate* delegate,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  // TODO(erg):
  // - Set "host->view_"
  // - Call "host->webwidget_->Resize"
  host->webwidget_ = WebView::Create(delegate, prefs);

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}
