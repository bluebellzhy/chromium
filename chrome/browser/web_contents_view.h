// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_CONTENTS_VIEW_H_
#define CHROME_BROWSER_WEB_CONTENTS_VIEW_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"

class InfoBarView;
class RenderViewHost;
class RenderWidgetHostHWND;
class WebContents;
struct WebDropData;

// The WebContentsView is an interface that is implemented by the platform-
// dependent web contents views. The WebContents uses this interface to talk to
// them.
class WebContentsView {
 public:
  virtual ~WebContentsView() {}

  virtual void CreateView(HWND parent_hwnd,
                          const gfx::Rect& initial_bounds) = 0;

  // Sets up the View that holds the rendered web page, receives messages for
  // it and contains page plugins.
  virtual RenderWidgetHostHWND* CreatePageView(
      RenderViewHost* render_view_host) = 0;

  // Returns the HWND that contains the contents of the tab.
  // TODO(brettw) this should not be necessary in this cross-platform interface.
  virtual HWND GetContainerHWND() const = 0;

  // Returns the HWND with the main content of the tab (i.e. the main render
  // view host, though there may be many popups in the tab as children of the
  // container HWND).
  // TODO(brettw) this should not be necessary in this cross-platform interface.
  virtual HWND GetContentHWND() const = 0;

  // Computes the rectangle for the native widget that contains the contents of
  // the tab relative to its parent.
  virtual void GetContainerBounds(gfx::Rect *out) const = 0;

  // Helper function for GetContainerBounds. Most callers just want to know the
  // size, and this makes it more clear.
  gfx::Size GetContainerSize() const {
    gfx::Rect rc;
    GetContainerBounds(&rc);
    return gfx::Size(rc.width(), rc.height());
  }

  // The user started dragging content of the specified type within the tab.
  // Contextual information about the dragged content is supplied by drop_data.
  virtual void StartDragging(const WebDropData& drop_data) = 0;

  // Enumerate and 'un-parent' any plugin windows that are children of us.
  virtual void DetachPluginWindows() = 0;

  // Set/get whether or not the info bar is visible. See also the ChromeFrame
  // method InfoBarVisibilityChanged and TabContents::IsInfoBarVisible.
  virtual void SetInfoBarVisible(bool visible) = 0;
  virtual bool IsInfoBarVisible() const = 0;

  // Create the InfoBarView and returns it if none has been created.
  // Just returns existing InfoBarView if it is already created.
  virtual InfoBarView* GetInfoBarView() = 0;

  // The page wants to update the mouse cursor during a drag & drop operation.
  // |is_drop_target| is true if the mouse is over a valid drop target.
  virtual void UpdateDragCursor(bool is_drop_target) = 0;

 protected:
  WebContentsView() {}  // Abstract interface.

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsView);
};

#endif  // CHROME_BROWSER_WEB_CONTENTS_VIEW_H_
