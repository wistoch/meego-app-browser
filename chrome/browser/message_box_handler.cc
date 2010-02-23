// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/message_box_handler.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "build/build_config.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/js_modal_dialog.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace {

const size_t kMaxReasonableTextLength = 2048;

// In some platforms, the underlying processing of humongous strings takes too
// long and thus make the UI thread unresponsive.
std::wstring MakeTextSafe(const std::wstring& text) {
  if (text.size() > kMaxReasonableTextLength)
    return text.substr(0, kMaxReasonableTextLength) + L"\x2026";
  return text;
}

}  // namespace

void RunJavascriptMessageBox(JavaScriptMessageBoxClient* client,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
  std::wstring title = client->GetMessageBoxTitle(frame_url,
      (dialog_flags == MessageBoxFlags::kIsJavascriptAlert));
  Singleton<AppModalDialogQueue>()->AddDialog(
      new JavaScriptAppModalDialog(client, title, dialog_flags,
          MakeTextSafe(message_text), default_prompt_text,
          display_suppress_checkbox, false, reply_msg));
}

void RunBeforeUnloadDialog(TabContents* tab_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
  std::wstring full_message =
      message_text + L"\n\n" +
      l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);
  Singleton<AppModalDialogQueue>()->AddDialog(new JavaScriptAppModalDialog(
      tab_contents, l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE),
      MessageBoxFlags::kIsJavascriptConfirm, MakeTextSafe(message_text),
      std::wstring(), false, true, reply_msg));
}

void RunCookiePrompt(TabContents* tab_contents,
                     HostContentSettingsMap* host_content_settings_map,
                     const GURL& origin,
                     const std::string& cookie_line,
                     CookiePromptModalDialogDelegate* delegate) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  origin, cookie_line, delegate));
#else
  // Linux: http://crbug.com/35178
  NOTIMPLEMENTED();
  delegate->BlockSiteData();
#endif
}

void RunLocalStoragePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  origin, key, value, delegate));
#else
  // Linux: http://crbug.com/35178
  NOTIMPLEMENTED();
  delegate->BlockSiteData();
#endif
}

void RunDatabasePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& database_name,
    CookiePromptModalDialogDelegate* delegate) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  Singleton<AppModalDialogQueue>()->AddDialog(
      new CookiePromptModalDialog(tab_contents, host_content_settings_map,
                                  origin, database_name, delegate));
#else
  // Linux: http://crbug.com/35178
  NOTIMPLEMENTED();
  delegate->BlockSiteData();
#endif
}
