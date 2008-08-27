// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#include "chromium_strings.h"
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

ChromeViews::View* ExternalProtocolDialog::GetContentsView() {
  return message_box_view_;
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

  ChromeViews::Window::CreateChromeWindow(root_hwnd, gfx::Rect(), this)->Show();
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

