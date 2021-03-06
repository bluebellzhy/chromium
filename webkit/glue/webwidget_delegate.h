// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWIDGET_DELEGATE_H__
#define WEBKIT_GLUE_WEBWIDGET_DELEGATE_H__

#include "base/gfx/native_widget_types.h"
#include "webkit/glue/window_open_disposition.h"

namespace gfx {
  class Point;
  class Rect;
}

class WebWidget;
class WebCursor;
struct WebPluginGeometry;

class WebWidgetDelegate {
 public:
  // Returns the view in which the WebWidget is embedded.
  virtual gfx::ViewHandle GetContainingWindow(WebWidget* webwidget) = 0;

  // Called when a region of the WebWidget needs to be re-painted.
  virtual void DidInvalidateRect(WebWidget* webwidget, const gfx::Rect& rect) = 0;

  // Called when a region of the WebWidget, given by clip_rect, should be
  // scrolled by the specified dx and dy amounts.
  virtual void DidScrollRect(WebWidget* webwidget, int dx, int dy,
                             const gfx::Rect& clip_rect) = 0;

  // This method is called to instruct the window containing the WebWidget to
  // show itself as the topmost window.  This method is only used after a
  // successful call to CreateWebWidget.  |disposition| indicates how this new
  // window should be displayed, but generally only means something for WebViews.
  virtual void Show(WebWidget* webwidget, WindowOpenDisposition disposition) = 0;

  // This method is called to instruct the window containing the WebWidget to
  // close.  Note: This method should just be the trigger that causes the
  // WebWidget to eventually close.  It should not actually be destroyed until
  // after this call returns.
  virtual void CloseWidgetSoon(WebWidget* webwidget) = 0;

  // This method is called to focus the window containing the WebWidget so
  // that it receives keyboard events.
  virtual void Focus(WebWidget* webwidget) = 0;

  // This method is called to unfocus the window containing the WebWidget so that
  // it no longer receives keyboard events.
  virtual void Blur(WebWidget* webwidget) = 0;

  virtual void SetCursor(WebWidget* webwidget, 
                         const WebCursor& cursor) = 0;
  // Returns the rectangle of the WebWidget in screen coordinates.
  virtual void GetWindowRect(WebWidget* webwidget, gfx::Rect* rect) = 0;

  // This method is called to re-position the WebWidget on the screen.  The given
  // rect is in screen coordinates.  The implementation may choose to ignore
  // this call or modify the given rect.  This method may be called before Show
  // has been called.
  // TODO(darin): this is more of a request; does this need to take effect
  // synchronously?
  virtual void SetWindowRect(WebWidget* webwidget, const gfx::Rect& rect) = 0;

  // Returns the rectangle of the window in which this WebWidget is embeded in.
  virtual void GetRootWindowRect(WebWidget* webwidget, gfx::Rect* rect) = 0;

  // Keeps track of the necessary window move for a plugin window that resulted
  // from a scroll operation.  That way, all plugin windows can be moved at the
  // same time as each other and the page.
  virtual void DidMove(WebWidget* webwidget, const WebPluginGeometry& move) = 0;

  // Suppress input events to other windows, and do not return until the widget
  // is closed.  This is used to support |window.showModalDialog|.
  virtual void RunModal(WebWidget* webwidget) = 0;

  // Owners depend on the delegates living as long as they do, so we ref them.
  virtual void AddRef() = 0;
  virtual void Release() = 0;

  // Returns true if the widget is in a background tab.
  virtual bool IsHidden() = 0;

  WebWidgetDelegate() { }
  virtual ~WebWidgetDelegate() { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebWidgetDelegate);
};

#endif  // #ifndef WEBKIT_GLUE_WEBWIDGET_DELEGATE_H__
