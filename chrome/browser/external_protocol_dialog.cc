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

#include "chrome/browser/external_protocol_dialog.h"

#include "base/registry.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, public:

// static
void ExternalProtocolDialog::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  TabContents* tab_contents = tab_util::GetTabContentsByID(
      render_process_host_id, routing_id);
  ExternalProtocolDialog* handler =
      new ExternalProtocolDialog(tab_contents, url);
}

ExternalProtocolDialog::~ExternalProtocolDialog() {
}

//////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, ChromeViews::DialogDelegate implementation:

int ExternalProtocolDialog::GetDialogButtons() const {
  return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
}

int ExternalProtocolDialog::GetDefaultDialogButton() const {
  return DIALOGBUTTON_CANCEL;
}

std::wstring ExternalProtocolDialog::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT);

  // Set the button to have a default name.
  return L"";
}

std::wstring ExternalProtocolDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_TITLE);
}

void ExternalProtocolDialog::WindowClosing() {
  delete this;
}

bool ExternalProtocolDialog::Accept() {
  MessageLoop* io_loop = g_browser_process->io_thread()->message_loop();
  if (io_loop == NULL) {
    // Returning true closes the dialog.
    return true;
  }

  // Attempt to launch the application on the IO loop.
  io_loop->PostTask(FROM_HERE,
      NewRunnableFunction(
          &ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck, url_));
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(TabContents* tab_contents,
                                               const GURL& url)
    : tab_contents_(tab_contents),
      url_(url) {
  std::wstring message_text = l10n_util::GetStringF(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToWide(url.scheme() + ":"),
      ASCIIToWide(url.possibly_invalid_spec())) + L"\n\n";

  message_text += l10n_util::GetStringF(
      IDS_EXTERNAL_PROTOCOL_APPLICATION_TO_LAUNCH,
      GetApplicationForProtocol()) + L"\n\n";

  message_text += l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_WARNING);

  message_box_view_ = new MessageBoxView(MessageBoxView::kIsConfirmMessageBox,
                                         message_text,
                                         L"",
                                         kMessageWidth);
  HWND root_hwnd;
  if (tab_contents_) {
    root_hwnd = GetAncestor(tab_contents_->GetContentHWND(), GA_ROOT);
  } else {
    // Dialog is top level if we don't have a tab_contents associated with us.
    root_hwnd = NULL;
  }

  ChromeViews::Window* dialog =
      ChromeViews::Window::CreateChromeWindow(root_hwnd, gfx::Rect(),
                                              message_box_view_, this);

  dialog->Show();
}

std::wstring ExternalProtocolDialog::GetApplicationForProtocol() {
  std::wstring url_spec = ASCIIToWide(url_.possibly_invalid_spec());
  std::wstring cmd_key_path =
      ASCIIToWide(url_.scheme() + "\\shell\\open\\command");
  RegKey cmd_key(HKEY_CLASSES_ROOT, cmd_key_path.c_str(), KEY_READ);
  size_t split_offset = url_spec.find(L':');
  if (split_offset == std::wstring::npos)
    return std::wstring();
  std::wstring parameters = url_spec.substr(split_offset + 1,
                                            url_spec.length() - 1);
  std::wstring application_to_launch;
  if (cmd_key.ReadValue(NULL, &application_to_launch)) {
    ReplaceSubstringsAfterOffset(&application_to_launch, 0, L"%1", parameters);
    return application_to_launch;
  } else {
    return std::wstring();
  }
}
