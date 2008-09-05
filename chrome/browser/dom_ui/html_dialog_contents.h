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

#ifndef CHROME_BROWSER_DOM_UI_HTML_DIALOG_CONTENTS_H__
#define CHROME_BROWSER_DOM_UI_HTML_DIALOG_CONTENTS_H__

#include "chrome/browser/dom_ui/dom_ui_host.h"
#include "chrome/views/window_delegate.h"

// Implement this class to receive notifications.
class HtmlDialogContentsDelegate : public ChromeViews::WindowDelegate {
 public:
   // Get the HTML file path for the content to load in the dialog.
   virtual GURL GetDialogContentURL() const = 0;
   // Get the size of the dialog.
   virtual void GetDialogSize(CSize* size) const = 0;
   // Gets the JSON string input to use when showing the dialog.
   virtual std::string GetDialogArgs() const = 0;
   // A callback to notify the delegate that the dialog closed.
   virtual void OnDialogClosed(const std::string& json_retval) = 0;

  protected:
   ~HtmlDialogContentsDelegate() {}
};

////////////////////////////////////////////////////////////////////////////////
//
// HtmlDialogContents is a simple wrapper around DOMUIHost and is used to
// display file URL contents inside a modal HTML dialog.
//
////////////////////////////////////////////////////////////////////////////////
class HtmlDialogContents : public DOMUIHost {
 public:
  struct HtmlDialogParams {
    // The URL for the content that will be loaded in the dialog.
    GURL url;
    // Width of the dialog.
    int width;
    // Height of the dialog.
    int height;
    // The JSON input to pass to the dialog when showing it.
    std::string json_input;
  };

  HtmlDialogContents(Profile* profile,
                     SiteInstance* instance,
                     RenderViewHostFactory* rvf);
  virtual ~HtmlDialogContents();

  // Initialize the HtmlDialogContents with the given delegate.  Must be called
  // after the RenderViewHost is created.
  void Init(HtmlDialogContentsDelegate* d);

  // Overridden from DOMUIHost:
  virtual void AttachMessageHandlers();

  // Returns true of this URL should be handled by the HtmlDialogContents.
  static bool IsHtmlDialogUrl(const GURL& url);

 private:
  // JS message handlers:
  void OnDialogClosed(const Value* content);

  // The delegate that knows how to display the dialog and receives the response
  // back from the dialog.
  HtmlDialogContentsDelegate* delegate_;

  DISALLOW_EVIL_CONSTRUCTORS(HtmlDialogContents);
};

#endif  // CHROME_BROWSER_DOM_UI_HTML_DIALOG_CONTENTS_H__
