// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WebCore provides hooks for several kinds of functionality, allowing separate
// classes termed "delegates" to receive notifications (in the form of direct
// function calls) when certain events are about to occur or have just occurred.
// In some cases, the delegate implements the needed functionality; in others,
// the delegate has some control over the behavior but doesn't actually
// implement it.  For example, the UI delegate is responsible for showing a
// dialog box or otherwise handling a JavaScript window.alert() call, via the
// RunJavaScriptAlert() method. On the other hand, the editor delegate doesn't
// actually handle editing functionality, although it could (for example)
// override whether a content-editable node accepts editing focus by returning
// false from ShouldBeginEditing(). (It would also possible for a more
// special-purpose editing delegate to act on the edited node in some way, e.g.
// to highlight modified text in the DidChangeContents() method.)

// WebKit divides the delegated tasks into several different classes, but we
// combine them into a single WebViewDelegate. This single delegate encompasses
// the needed functionality of the WebKit UIDelegate, ContextMenuDelegate,
// PolicyDelegate, FrameLoadDelegate, and EditorDelegate; additional portions
// of ChromeClient and FrameLoaderClient not delegated in the WebKit
// implementation; and some WebView additions.

#ifndef WEBKIT_GLUE_WEBVIEW_DELEGATE_H__
#define WEBKIT_GLUE_WEBVIEW_DELEGATE_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/context_node_types.h"
#include "webkit/glue/webwidget_delegate.h"
#include "webkit/glue/window_open_disposition.h"

namespace gfx {
  class Point;
  class Rect;
}

struct PasswordForm;
struct WebDropData;
struct WebPreferences;
class SkBitmap;
class WebError;
class WebFrame;
class WebHistoryItem;
class WebPluginDelegate;
class WebRequest;
class WebResponse;
class WebView;
class WebWidget;

enum WebNavigationType {
  WebNavigationTypeLinkClicked,
  WebNavigationTypeFormSubmitted,
  WebNavigationTypeBackForward,
  WebNavigationTypeReload,
  WebNavigationTypeFormResubmitted,
  WebNavigationTypeOther
};

enum NavigationGesture {
  NavigationGestureUser,    // User initiated navigation/load. This is not
                            // currently used due to the untrustworthy nature
                            // of userGestureHint (wasRunByUserGesture). See
                            // bug 1051891.
  NavigationGestureAuto,    // Non-user initiated navigation / load. For example
                            // onload or setTimeout triggered document.location
                            // changes, and form.submits. See bug 1046841 for
                            // some cases that should be treated this way but
                            // aren't yet.
  NavigationGestureUnknown, // What we assign when userGestureHint returns true
                            // because we can't trust it.
};


// Interface passed in to the WebViewDelegate to receive notification of the
// result of an open file dialog.
class WebFileChooserCallback {
 public:
  WebFileChooserCallback() {}
  virtual ~WebFileChooserCallback() {}
  virtual void OnFileChoose(const std::wstring& file_name) { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebFileChooserCallback);
};


// Inheritance here is somewhat weird, but since a WebView is a WebWidget,
// it makes sense that a WebViewDelegate is a WebWidgetDelegate.
class WebViewDelegate : virtual public WebWidgetDelegate {
 public:
  // WebView additions -------------------------------------------------------

  // This method is called to create a new WebView.  The WebView should not be
  // made visible until the new WebView's Delegate has its Show method called.
  // The returned WebView pointer is assumed to be owned by the host window,
  // and the caller of CreateWebView should not release the given WebView.
  // user_gesture is true if a user action initiated this call.
  virtual WebView* CreateWebView(WebView* webview, bool user_gesture) {
    return NULL;
  }

  // This method is called to create a new WebWidget to act as a popup
  // (like a drop-down menu).
  virtual WebWidget* CreatePopupWidget(WebView* webview) {
    return NULL;
  }

  // This method is called to create a WebPluginDelegate implementation when a
  // new plugin is instanced.  See webkit_glue::CreateWebPluginDelegateHelper
  // for a default WebPluginDelegate implementation.
  // TODO(port): clsid is very Win- and ActiveX-specific; refactor to be more
  // platform-neutral
  virtual WebPluginDelegate* CreatePluginDelegate(
      WebView* webview,
      const GURL& url,
      const std::string& mime_type,
      const std::string& clsid,
      std::string* actual_mime_type) {
    return NULL;
  }

  // This method is called when default plugin has been correctly created and
  // initialized, and found that the missing plugin is available to install or
  // user has started installation.
  virtual void OnMissingPluginStatus(WebPluginDelegate* delegate, int status) {
  }

  // This method is called to open a URL in the specified manner.
  virtual void OpenURL(WebView* webview, const GURL& url,
                       WindowOpenDisposition disposition) {
  }

  // Notifies how many matches have been found so far, for a given request_id.
  // |final_update| specifies whether this is the last update (all frames have
  // completed scoping).
  virtual void ReportFindInPageMatchCount(int count, int request_id,
                                          bool final_update) {
  }

  // Notifies the browser what tick-mark rect is currently selected. Parameter
  // |request_id| lets the recipient know which request this message belongs to,
  // so that it can choose to ignore the message if it has moved on to other
  // things. |selection_rect| is expected to have coordinates relative to the
  // top left corner of the web page area and represent where on the screen the
  // selection rect is currently located.
  virtual void ReportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const gfx::Rect& selection_rect) {
  }

  // This function is called to retrieve a resource bitmap from the
  // renderer that was cached as a result of the renderer receiving a
  // ViewMsg_Preload_Bitmap message from the browser.
  virtual const SkBitmap* GetPreloadedResourceBitmap(int resource_id) {
    return NULL;
  }

  // Returns whether this WebView was opened by a user gesture.
  virtual bool WasOpenedByUserGesture(WebView* webview) const {
    return true;
  }

  // FrameLoaderClient -------------------------------------------------------

  // Notifies the delegate that a load has begun.
  virtual void DidStartLoading(WebView* webview) {
  }

  // Notifies the delegate that all loads are finished.
  virtual void DidStopLoading(WebView* webview) {
  }

  // The original version of this is WindowScriptObjectAvailable, below. This
  // is a Chrome-specific version that serves the same purpose, but has been
  // renamed since we haven't implemented WebScriptObject.  Our embedding
  // implementation binds native objects to the window via the webframe instead.
  // TODO(pamg): If we do implement WebScriptObject, we may wish to switch to
  // using the original version of this function.
  virtual void WindowObjectCleared(WebFrame* webframe) {
  }

  // PolicyDelegate ----------------------------------------------------------

  // This method is called to notify the delegate, and let it modify a
  // proposed navigation. It will be called before loading starts, and
  // on every redirect.
  //
  // disposition specifies what should normally happen for this
  // navigation (open in current tab, start a new tab, start a new
  // window, etc).  This method can return an altered disposition, and
  // take any additional separate action it wants to.
  //
  // is_redirect is true if this is a redirect rather than user action.
  virtual WindowOpenDisposition DispositionForNavigationAction(
      WebView* webview,
      WebFrame* frame,
      const WebRequest* request,
      WebNavigationType type,
      WindowOpenDisposition disposition,
      bool is_redirect) {
    return disposition;
  }

  // FrameLoadDelegate -------------------------------------------------------

  // Notifies the delegate that the provisional load of a specified frame in a
  // given WebView has started. By the time the provisional load for a frame has
  // started, we know whether or not the current load is due to a client
  // redirect or not, so we pass this information through to allow us to set
  // the referrer properly in those cases. The consumed_client_redirect_src is
  // an empty invalid GURL in other cases.
  virtual void DidStartProvisionalLoadForFrame(
      WebView* webview,
      WebFrame* frame,
      NavigationGesture gesture) {
  }

  // Called when a provisional load is redirected (see GetProvisionalDataSource
  // for more info on provisional loads). This happens when the server sends
  // back any type of redirect HTTP response.
  //
  // The redirect information can be retrieved from the provisional data
  // source's redirect chain, which will be updated prior to this callback.
  // The last element in that vector will be the new URL (which will be the
  // same as the provisional data source's current URL), and the next-to-last
  // element will be the referring URL.
  virtual void DidReceiveProvisionalLoadServerRedirect(WebView* webview,
                                                       WebFrame* frame) {
  }

  //  @method webView:didFailProvisionalLoadWithError:forFrame:
  //  @abstract Notifies the delegate that the provisional load has failed
  //  @param webView The WebView sending the message
  //  @param error The error that occurred
  //  @param frame The frame for which the error occurred
  //  @discussion This method is called after the provisional data source has
  //  failed to load.  The frame will continue to display the contents of the
  //  committed data source if there is one.
  virtual void DidFailProvisionalLoadWithError(WebView* webview,
                                               const WebError& error,
                                               WebFrame* frame) {
  }

  // If the provisional load fails, we try to load a an error page describing
  // the user about the load failure.  |html| is the UTF8 text to display.  If
  // |html| is empty, we will fall back on a local error page.
  virtual void LoadNavigationErrorPage(WebFrame* frame,
                                       const WebRequest* failed_request,
                                       const WebError& error,
                                       const std::string& html,
                                       bool replace) {
  }

  // Notifies the delegate that the load has changed from provisional to
  // committed. This method is called after the provisional data source has
  // become the committed data source.
  //
  // In some cases, a single load may be committed more than once. This
  // happens in the case of multipart/x-mixed-replace, also known as "server
  // push". In this case, a single location change leads to multiple documents
  // that are loaded in sequence. When this happens, a new commit will be sent
  // for each document.
  //
  // The "is_new_navigation" flag will be true when a new session history entry
  // was created for the load.  The frame's GetHistoryState method can be used
  // to get the corresponding session history state.
  virtual void DidCommitLoadForFrame(WebView* webview, WebFrame* frame,
                                     bool is_new_navigation) {
  }

  //
  //  @method webView:didReceiveTitle:forFrame:
  //  @abstract Notifies the delegate that the page title for a frame has been received
  //  @param webView The WebView sending the message
  //  @param title The new page title
  //  @param frame The frame for which the title has been received
  //  @discussion The title may update during loading; clients should be prepared for this.
  //  - (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame;
  virtual void DidReceiveTitle(WebView* webview,
                               const std::wstring& title,
                               WebFrame* frame) {
  }

  //
  //  @method webView:didFinishLoadForFrame:
  //  @abstract Notifies the delegate that the committed load of a frame has completed
  //  @param webView The WebView sending the message
  //  @param frame The frame that finished loading
  //  @discussion This method is called after the committed data source of a frame has successfully loaded
  //  and will only be called when all subresources such as images and stylesheets are done loading.
  //  Plug-In content and JavaScript-requested loads may occur after this method is called.
  //  - (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame;
  virtual void DidFinishLoadForFrame(WebView* webview,
                                     WebFrame* frame) {
  }

  //
  //  @method webView:didFailLoadWithError:forFrame:
  //  @abstract Notifies the delegate that the committed load of a frame has failed
  //  @param webView The WebView sending the message
  //  @param error The error that occurred
  //  @param frame The frame that failed to load
  //  @discussion This method is called after a data source has committed but failed to completely load.
  //  - (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
  virtual void DidFailLoadWithError(WebView* webview,
                                    const WebError& error,
                                    WebFrame* forFrame) {
  }

  // Notifies the delegate of a DOMContentLoaded event.
  // This is called when the html resource has been loaded, but
  // not necessarily all subresources (images, stylesheets). So, this is called
  // before DidFinishLoadForFrame.
  virtual void DidFinishDocumentLoadForFrame(WebView* webview, WebFrame* frame) {
  }

  // This method is called when we load a resource from an in-memory cache.
  // A return value of |false| indicates the load should proceed, but WebCore
  // appears to largely ignore the return value.
  virtual bool DidLoadResourceFromMemoryCache(WebView* webview,
                                              const WebRequest& request,
                                              const WebResponse& response,
                                              WebFrame* frame) {
    return false;
  }

  // This is called after javascript onload handlers have been fired.
  virtual void DidHandleOnloadEventsForFrame(WebView* webview, WebFrame* frame) {
  }

  // This method is called when anchors within a page have been clicked.
  // It is very similar to DidCommitLoadForFrame.
  virtual void DidChangeLocationWithinPageForFrame(WebView* webview,
                                                   WebFrame* frame,
                                                   bool is_new_navigation) {
  }

  // This is called when the favicon for a frame has been received.
  virtual void DidReceiveIconForFrame(WebView* webview, WebFrame* frame) {
  }

  // Notifies the delegate that a frame will start a client-side redirect. When
  // this function is called, the redirect has not yet been started (it may
  // not even be scheduled to happen until some point in the future). When the
  // redirect has been cancelled or has succeeded, DidStopClientRedirect will
  // be called.
  //
  // WebKit considers meta refreshes, and setting document.location (regardless
  // of when called) as client redirects (possibly among others).
  //
  // This function is intended to continue progress feedback while a
  // client-side redirect is pending. Watch out: WebKit seems to call us twice
  // for client redirects, resulting in two calls of this function.
  virtual void WillPerformClientRedirect(WebView* webview,
                                         WebFrame* frame,
                                         const GURL& src_url,
                                         const GURL& dest_url,
                                         unsigned int delay_seconds,
                                         unsigned int fire_date) {
  }

  // Notifies the delegate that a pending client-side redirect has been
  // cancelled (for example, if the frame changes before the timeout) or has
  // completed successfully. A client-side redirect is the result of setting
  // document.location, for example, as opposed to a server side redirect
  // which is the result of HTTP headers (see DidReceiveServerRedirect).
  //
  // On success, this will be called when the provisional load that the client
  // side redirect initiated is committed.
  //
  // See the implementation of FrameLoader::clientRedirectCancelledOrFinished.
  virtual void DidCancelClientRedirect(WebView* webview,
                                       WebFrame* frame) {
  }

  // Notifies the delegate that the load about to be committed for the specified
  // webview and frame was due to a client redirect originating from source URL.
  // The information/notification obtained from this method is relevant until
  // the next provisional load is started, at which point it becomes obsolete.
  virtual void DidCompleteClientRedirect(WebView* webview,
                                         WebFrame* frame,
                                         const GURL& source) {
  }
  //
  //  @method webView:willCloseFrame:
  //  @abstract Notifies the delegate that a frame will be closed
  //  @param webView The WebView sending the message
  //  @param frame The frame that will be closed
  //  @discussion This method is called right before WebKit is done with the frame
  //  and the objects that it contains.
  //  - (void)webView:(WebView *)sender willCloseFrame:(WebFrame *)frame;
  virtual void WillCloseFrame(WebView* webview, WebFrame* frame) {
  }

  // ResourceLoadDelegate ----------------------------------------------------

  // Associates the given identifier with the initial resource request.
  // Resource load callbacks will use the identifier throughout the life of the
  // request.
  virtual void AssignIdentifierToRequest(WebView* webview,
                                         uint32 identifier,
                                         const WebRequest& request) {
  }

  // Notifies the delegate that a request is about to be sent out, giving the
  // delegate the opportunity to modify the request.  Note that request is
  // writable here, and changes to the URL, for example, will change the request
  // to be made.
  virtual void WillSendRequest(WebView* webview,
                               uint32 identifier,
                               WebRequest* request) {
  }

  // Notifies the delegate that a subresource load has succeeded.
  virtual void DidFinishLoading(WebView* webview, uint32 identifier) {
  }

  // Notifies the delegate that a subresource load has failed, and why.
  virtual void DidFailLoadingWithError(WebView* webview,
                                       uint32 identifier,
                                       const WebError& error) {
  }

  // ChromeClient ------------------------------------------------------------

  // Appends a line to the application's error console.  The message contains
  // an error description or other information, the line_no provides a line
  // number (e.g. for a JavaScript error report), and the source_id contains
  // a URL or other description of the source of the message.
  virtual void AddMessageToConsole(WebView* webview,
                                   const std::wstring& message,
                                   unsigned int line_no,
                                   const std::wstring& source_id) {
  }

  // Notification of possible password forms to be filled/submitted by
  // the password manager
  virtual void OnPasswordFormsSeen(WebView* webview,
                                   const std::vector<PasswordForm>& forms) {
  }

  //
  virtual void OnUnloadListenerChanged(WebView* webview, WebFrame* webframe) {
  }

  // UIDelegate --------------------------------------------------------------

  // Asks the browser to show a modal HTML dialog.  The dialog is passed the
  // given arguments as a JSON string, and returns its result as a JSON string
  // through json_retval.
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   std::string* json_retval) {
  }

  // Displays a JavaScript alert panel associated with the given view. Clients
  // should visually indicate that this panel comes from JavaScript. The panel
  // should have a single OK button.
  virtual void RunJavaScriptAlert(WebView* webview,
                                  const std::wstring& message) {
  }

  // Displays a JavaScript confirm panel associated with the given view.
  // Clients should visually indicate that this panel comes
  // from JavaScript. The panel should have two buttons, e.g. "OK" and
  // "Cancel". Returns true if the user hit OK, or false if the user hit Cancel.
  virtual bool RunJavaScriptConfirm(WebView* webview,
                                    const std::wstring& message) {
    return false;
  }

  // Displays a JavaScript text input panel associated with the given view.
  // Clients should visually indicate that this panel comes from JavaScript.
  // The panel should have two buttons, e.g. "OK" and "Cancel", and an area to
  // type text. The default_value should appear as the initial text in the
  // panel when it is shown. If the user hit OK, returns true and fills result
  // with the text in the box.  The value of result is undefined if the user
  // hit Cancel.
  virtual bool RunJavaScriptPrompt(WebView* webview,
                                   const std::wstring& message,
                                   const std::wstring& default_value,
                                   std::wstring* result) {
    return false;
  }

  // Displays a "before unload" confirm panel associated with the given view.
  // The panel should have two buttons, e.g. "OK" and "Cancel", where OK means
  // that the navigation should continue, and Cancel means that the navigation
  // should be cancelled, leaving the user on the current page.  Returns true
  // if the user hit OK, or false if the user hit Cancel.
  virtual bool RunBeforeUnloadConfirm(WebView* webview,
                                      const std::wstring& message) {
    return true;  // OK, continue to navigate away
  }

  // Tells the client that we're hovering over a link with a given URL,
  // if the node is not a link, the URL will be an empty GURL.
  virtual void UpdateTargetURL(WebView* webview,
                               const GURL& url) {
  }

  // Called to display a file chooser prompt.  The prompt should be pre-
  // populated with the given initial_filename string.  The WebViewDelegate
  // will own the WebFileChooserCallback object and is responsible for
  // freeing it.
  virtual void RunFileChooser(const std::wstring& initial_filename,
                              WebFileChooserCallback* file_chooser) {
    delete file_chooser;
  }

  // @abstract Shows a context menu with commands relevant to a specific
  //           element on the current page.
  // @param webview The WebView sending the delegate method.
  // @param type The type of node(s) the context menu is being invoked on
  // @param x The x position of the mouse pointer (relative to the webview)
  // @param y The y position of the mouse pointer (relative to the webview)
  // @param link_url The absolute URL of the link that contains the node the
  // mouse right clicked on
  // @param image_url The absolute URL of the image that the mouse right
  // clicked on
  // @param page_url The URL of the page the mouse right clicked on
  // @param frame_url The URL of the subframe the mouse right clicked on
  // @param selection_text The raw text of the selection that the mouse right
  // clicked on
  // @param misspelled_word The editable (possibily) misspelled word
  // in the Editor on which dictionary lookup for suggestions will be done.
  // @param edit_flags Which edit operations the renderer believes are available
  // @param frame_encoding Which indicates the encoding of current focused
  // sub frame.
  virtual void ShowContextMenu(WebView* webview,
                               ContextNode::Type type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& security_info) {
  }

  // Starts a drag session with the supplied contextual information.
  // webview: The WebView sending the delegate method.
  // drop_data: a WebDropData struct which should contain all the necessary
  // information for dragging data out of the webview.
  virtual void StartDragging(WebView* webview, const WebDropData& drop_data) { }

  // Returns the focus to the client.
  // reverse: Whether the focus should go to the previous (if true) or the next
  // focusable element.
  virtual void TakeFocus(WebView* webview, bool reverse) {
  }

  // Displays JS out-of-memory warning in the infobar
  virtual void JSOutOfMemory() {
  }

  // EditorDelegate ----------------------------------------------------------

  // These methods exist primarily to allow a specialized executable to record
  // edit events for testing purposes.  Most embedders are not expected to
  // override them. In fact, by default these editor delegate methods aren't
  // even called by the EditorClient, for performance reasons. To enable them,
  // call WebView::SetUseEditorDelegate(true) for each WebView.

  virtual bool ShouldBeginEditing(WebView* webview, std::wstring range) {
    return true;
  }

  virtual bool ShouldEndEditing(WebView* webview, std::wstring range) {
    return true;
  }

  virtual bool ShouldInsertNode(WebView* webview,
                                std::wstring node,
                                std::wstring range,
                                std::wstring action) {
    return true;
  }

  virtual bool ShouldInsertText(WebView* webview,
                                std::wstring text,
                                std::wstring range,
                                std::wstring action) {
    return true;
  }

  virtual bool ShouldChangeSelectedRange(WebView* webview,
                                         std::wstring fromRange,
                                         std::wstring toRange,
                                         std::wstring affinity,
                                         bool stillSelecting) {
    return true;
  }

  virtual bool ShouldDeleteRange(WebView* webview, std::wstring range) {
    return true;
  }

  virtual bool ShouldApplyStyle(WebView* webview,
                                std::wstring style,
                                std::wstring range) {
    return true;
  }

  virtual bool SmartInsertDeleteEnabled() {
    return false;
  }
  virtual void DidBeginEditing() { }
  virtual void DidChangeSelection() { }
  virtual void DidChangeContents() { }
  virtual void DidEndEditing() { }

  // Notification that a user metric has occurred.
  virtual void UserMetricsRecordAction(const std::wstring& action) { }
  virtual void UserMetricsRecordComputedAction(const std::wstring& action) {
    UserMetricsRecordAction(action);
  }

  // -------------------------------------------------------------------------

  // Notification that a request to download an image has completed. |errored|
  // indicates if there was a network error. The image is empty if there was
  // a network error, the contents of the page couldn't by converted to an
  // image, or the response from the host was not 200.
  // NOTE: image is empty if the response didn't contain image data.
  virtual void DidDownloadImage(int id,
                                const GURL& image_url,
                                bool errored,
                                const SkBitmap& image) {
  }

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };
  // If providing an alternate error page (like link doctor), returns the URL
  // to fetch instead.  If an invalid url is returned, just fall back on local
  // error pages. |error_name| tells the delegate what type of error page we
  // want (e.g., 404 vs dns errors).
  virtual GURL GetAlternateErrorPageURL(const GURL& failedURL,
                                        ErrorPageType error_type) {
    return GURL();
  }

  // History Related ---------------------------------------------------------

  // Returns the session history entry at a distance |offset| relative to the
  // current entry.  Returns NULL on failure.
  virtual WebHistoryItem* GetHistoryEntryAtOffset(int offset) {
    return NULL;
  }

  // Asynchronously navigates to the history entry at the given offset.
  virtual void GoToEntryAtOffsetAsync(int offset) {
  }

  // Returns how many entries are in the back and forward lists, respectively.
  virtual int GetHistoryBackListCount() {
    return 0;
  }
  virtual int GetHistoryForwardListCount() {
    return 0;
  }

  // Notification that the form state of an element in the document, scroll
  // position, or possibly something else has changed that affects session
  // history (HistoryItem). This function will be called frequently, so the
  // implementor should not perform intensive operations in this notification.
  virtual void OnNavStateChanged(WebView* webview) { }

  // -------------------------------------------------------------------------

  // Tell the delegate the tooltip text for the current mouse position.
  virtual void SetTooltipText(WebView* webview,
                              const std::wstring& tooltip_text) { }

  // Downloading -------------------------------------------------------------

  virtual void DownloadUrl(const GURL& url, const GURL& referrer) { }

  // Editor Client -----------------------------------------------------------

  // Returns true if the word is spelled correctly. The word may begin or end
  // with whitespace or punctuation, so the implementor should be sure to handle
  // these cases.
  //
  // If the word is misspelled (returns false), the given first and last
  // indices (inclusive) will be filled with the offsets of the boundary of the
  // word within the given buffer. The out pointers must be specified. If the
  // word is correctly spelled (returns true), they will be set to (0,0).
  virtual void SpellCheck(const std::wstring& word, int& misspell_location,
                          int& misspell_length) {
    misspell_location = misspell_length = 0;
  }

  // Changes the state of the input method editor.
  virtual void SetInputMethodState(bool enabled) { }

  // Asks the user to print the page or a specific frame. Called in response to
  // a window.print() call.
  virtual void ScriptedPrint(WebFrame* frame) { }

  virtual void WebInspectorOpened(int num_resources) { }

  // Called when the FrameLoader goes into a state in which a new page load
  // will occur.
  virtual void TransitionToCommittedForNewPage() { }

  // Called when an item was added to the history
  virtual void DidAddHistoryItem() { }

  WebViewDelegate() { }
  virtual ~WebViewDelegate() { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebViewDelegate);
};

#endif  // WEBKIT_GLUE_WEBVIEW_DELEGATE_H__

