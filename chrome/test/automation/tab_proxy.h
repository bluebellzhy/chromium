// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_TAB_PROXY_H_
#define CHROME_TEST_AUTOMATION_TAB_PROXY_H_

#include <wtypes.h>
#include <string>
#include <vector>

#include "chrome/browser/download/save_package.h"
#include "chrome/browser/security_style.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/test/automation/automation_handle_tracker.h"

class ConstrainedWindowProxy;
class GURL;
class Value;

typedef enum FindInPageDirection { BACK = 0, FWD = 1 };
typedef enum FindInPageCase { IGNORE_CASE = 0, CASE_SENSITIVE = 1 };

class TabProxy : public AutomationResourceProxy {
 public:
  TabProxy(AutomationMessageSender* sender,
           AutomationHandleTracker* tracker,
           int handle)
    : AutomationResourceProxy(tracker, sender, handle) {}

  virtual ~TabProxy() {}

  // Gets the current url of the tab.
  bool GetCurrentURL(GURL* url) const;

  // Gets the title of the tab.
  bool GetTabTitle(std::wstring* title) const;

  // Gets the number of constrained window for this tab.
  bool GetConstrainedWindowCount(int* count) const;

  // Gets the proxy object for constrained window within this tab. Ownership
  // for the returned object is transfered to the caller. Returns NULL on
  // failure.
  ConstrainedWindowProxy* GetConstrainedWindow(int window_index) const;

  // Executes a javascript in a frame's context whose xpath is provided as the
  // first parameter and extract the values from the resulting json string.
  // Examples:
  // jscript = "window.domAutomationController.send('string');"
  // will result in value = "string"
  // jscript = "window.domAutomationController.send(24);"
  // will result in value = 24
  bool ExecuteAndExtractString(const std::wstring& frame_xpath,
                               const std::wstring& jscript,
                               std::wstring* value);
  bool ExecuteAndExtractBool(const std::wstring& frame_xpath,
                             const std::wstring& jscript,
                             bool* value);
  bool ExecuteAndExtractInt(const std::wstring& frame_xpath,
                            const std::wstring& jscript,
                            int* value);
  bool ExecuteAndExtractValue(const std::wstring& frame_xpath,
                              const std::wstring& jscript,
                              Value** value);

  // Navigates to a url. This method accepts the same kinds of URL input that
  // can be passed to Chrome on the command line. This is a synchronous call and
  // hence blocks until the navigation completes.
  // Returns a status from AutomationMsg_NavigationResponseValues.
  int NavigateToURL(const GURL& url);

  // Navigates to a url. This is same as NavigateToURL with a timeout option.
  // The function returns until the navigation completes or timeout (in
  // milliseconds) occurs. If return after timeout, is_timeout is set to true.
  int NavigateToURLWithTimeout(const GURL& url, uint32 timeout_ms,
                               bool* is_timeout);

  // Navigates to a url in an externally hosted tab.
  // This method accepts the same kinds of URL input that
  // can be passed to Chrome on the command line. This is a synchronous call and
  // hence blocks until the navigation completes.
  // Returns a status from AutomationMsg_NavigationResponseValues.
  int NavigateInExternalTab(const GURL& url);

  // Navigates to a url. This is an asynchronous version of NavigateToURL.
  // The function returns immediately after sending the LoadURL notification
  // to the browser.
  // TODO(vibhor): Add a callback if needed in future.
  // TODO(mpcomplete): If the navigation results in an auth challenge, the
  // TabProxy we attach won't know about it.  See bug 666730.
  bool NavigateToURLAsync(const GURL& url);

  // Replaces a vector contents with the redirect chain out of the given URL.
  // Returns true on success. Failure may be due to being unable to send the
  // message, parse the response, or a failure of the history system in the
  // browser.
  bool GetRedirectsFrom(const GURL& source_url, std::vector<GURL>* redirects);

  // Equivalent to hitting the Back button. This is a synchronous call and
  // hence blocks until the navigation completes.
  int GoBack();

  // Equivalent to hitting the Forward button. This is a synchronous call and
  // hence blocks until the navigation completes.
  // Returns a status from AutomationMsg_NavigationResponseValues.
  int GoForward();

  // Equivalent to hitting the Reload button. This is a synchronous call and
  // hence blocks until the navigation completes.
  int Reload();

  // Closes the tab. This is synchronous, but does NOT block until the tab has
  // closed, rather it blocks until the browser has initiated the close. Use
  // Close(true) if you need to block until tab completely closes.
  //
  // Note that this proxy is invalid after this call.
  bool Close();

  // Variant of close that allows you to specify whether you want to block
  // until the tab has completely closed (wait_until_closed == true) or block
  // until the browser has initiated the close (wait_until_closed = false).
  //
  // When a tab is closed the browser does additional work via invoke later
  // and may wait for messages from the renderer. Supplying a value of true to
  // this method waits until all processing is done. Be careful with this,
  // when closing the last tab it is possible for the browser to shutdown BEFORE
  // the tab has completely closed. In other words, this may NOT be sent for
  // the last tab.
  bool Close(bool wait_until_closed);

  // Gets the HWND that corresponds to the content area of this tab.
  // Returns true if the call was successful.
  // Returns a status from AutomationMsg_NavigationResponseValues.
  bool GetHWND(HWND* hwnd) const;

  // Gets the process ID that corresponds to the content area of this tab.
  // Returns true if the call was successful.  If the specified tab has no
  // separate process for rendering its content, the return value is true but
  // the process_id is 0.
  bool GetProcessID(int* process_id) const;

  // Supply or cancel authentication to a login prompt.  These are synchronous
  // calls and hence block until the load finishes (or another login prompt
  // appears, in the case of invalid login info).
  bool SetAuth(const std::wstring& username, const std::wstring& password);
  bool CancelAuth();

  // Checks if this tab has a login prompt waiting for auth.  This will be
  // true if a navigation results in a login prompt, and if an attempted login
  // fails.
  // Note that this is only valid if you've done a navigation on this same
  // object; different TabProxy objects can refer to the same Tab.  Calls
  // that can set this are NavigateToURL, GoBack, and GoForward.
  // TODO(mpcomplete): we have no way of knowing if auth is needed after either
  // NavigateToURLAsync, or after appending a tab with an URL that triggers
  // auth.
  bool NeedsAuth() const;

  // Fills |*is_visible| with whether the tab's download shelf is currently
  // visible. The return value indicates success. On failure, |*is_visible| is
  // unchanged.
  bool IsShelfVisible(bool* is_visible);

  // Opens the FindInPage box. Note: If you just want to search within a tab
  // you don't need to call this function, just use FindInPage(...) directly.
  bool OpenFindInPage();

  // Returns whether the Find window is fully visible If animating, |is_visible|
  // will be false. Returns false on failure.
  bool IsFindWindowFullyVisible(bool* is_visible);

  // Get the x, y coordinates for the Find window. If animating, |x| and |y|
  // will be -1, -1. Returns false on failure.
  bool GetFindWindowLocation(int* x, int* y);

  // Starts a search within the current tab. The parameter |search_string|
  // specifies what string to search for, |forward| specifies whether to search
  // in forward direction, and |match_case| specifies case sensitivity
  // (true=case sensitive). |find_next| specifies whether this is a new search
  // or a continuation of the old one. A return value of -1 indicates failure.
  int FindInPage(const std::wstring& search_string, FindInPageDirection forward,
                 FindInPageCase match_case, bool find_next);

  bool GetCookies(const GURL& url, std::string* cookies);
  bool GetCookieByName(const GURL& url,
                       const std::string& name,
                       std::string* cookies);
  bool SetCookie(const GURL& url, const std::string& value);

  // Sends a InspectElement message for the current tab. |x| and |y| are the
  // coordinates that we want to simulate that the user is trying to inspect.
  int InspectElement(int x, int y);

  // Block the thread until the constrained(child) window count changes.
  // First parameter is the original child window count
  // The second parameter is updated with the number of new child windows.
  // The third parameter specifies the timeout length for the wait loop.
  // Returns false if the count does not change.
  bool WaitForChildWindowCountToChange(int count, int* new_count,
      int wait_timeout);

  bool GetDownloadDirectory(std::wstring* download_directory);

  // Shows an interstitial page.  Blocks until the interstitial page
  // has been loaded. Return false if a failure happens.3
  bool ShowInterstitialPage(const std::string& html_text);

  // Hides the currently shown interstitial page. Blocks until the interstitial
  // page has been hidden. Return false if a failure happens.
  bool HideInterstitialPage();

  // This sets the keyboard accelerators to be used by an externally
  // hosted tab. This call is not valid on a regular tab hosted within
  // Chrome.
  bool SetAccelerators(HACCEL accel_table, int accel_table_entry_count);

  // The container of an externally hosted tab calls this to reflect any
  // accelerator keys that it did not process. This gives the tab a chance
  // to handle the keys
  bool ProcessUnhandledAccelerator(const MSG& msg);

  // Waits for the tab to finish being restored. Returns true on success.
  // timeout_ms gives the max amount of time to wait for restore to complete.
  bool WaitForTabToBeRestored(uint32 timeout_ms);

  // Retrieves the different security states for the current tab.
  bool GetSecurityState(SecurityStyle* security_style,
                        int* ssl_cert_status,
                        int* mixed_content_state);

  // Returns the type of the page currently showing (normal, interstitial,
  // error).
  bool GetPageType(NavigationEntry::PageType* page_type);

  // Simulates the user action on the SSL blocking page.  if |proceed| is true,
  // this is equivalent to clicking the 'Proceed' button, if false to 'Take me
  // out of there' button.
  bool TakeActionOnSSLBlockingPage(bool proceed);

  // Prints the current page without user intervention.
  bool PrintNow();

  // Save the current web page. |file_name| is the HTML file name, and
  // |dir_path| is the directory for saving resource files. |type| indicates
  // which type we're saving as: HTML only or the complete web page.
  bool SavePage(const std::wstring& file_name,
                const std::wstring& dir_path,
                SavePackage::SavePackageType type);

  // Posts a message to the external tab.
  void HandleMessageFromExternalHost(AutomationHandle handle,
                                     const std::string& target,
                                     const std::string& message);

  // Retrieves the number of SSL related info-bars currently showing in |count|.
  bool GetSSLInfoBarCount(int* count);

  // Causes a click on the link of the info-bar at |info_bar_index|.  If
  // |wait_for_navigation| is true, this call does not return until a navigation
  // has occured.
  bool ClickSSLInfoBarLink(int info_bar_index, bool wait_for_navigation);

  // Retrieves the time at which the last navigation occured.  This is intended
  // to be used with WaitForNavigation (see below).
  bool GetLastNavigationTime(int64* last_navigation_time);

  // Waits for a new navigation if none as occurred since |last_navigation_time|
  // The purpose of this function is for operations that causes asynchronous
  // navigation to happen.
  // It is supposed to be used as follow:
  // int64 last_nav_time;
  // tab_proxy->GetLastNavigationTime(&last_nav_time);
  // tab_proxy->SomeOperationThatTriggersAnAsynchronousNavigation();
  // tab_proxy->WaitForNavigation(last_nav_time);
  bool WaitForNavigation(int64 last_navigation_time);

 private:
  DISALLOW_COPY_AND_ASSIGN(TabProxy);
};

#endif  // CHROME_TEST_AUTOMATION_TAB_PROXY_H_

