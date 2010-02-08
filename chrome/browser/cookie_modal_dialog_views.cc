// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"

#include "base/logging.h"
#include "chrome/browser/views/cookie_prompt_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "views/window/window.h"


// TODO(zelidrag): Make this work on Linux (views).

int CookiePromptModalDialog::GetDialogButtons() {
#if defined(OS_WIN)
  return dialog_->GetDialogButtons();
#else
  return 0;
#endif
}

void CookiePromptModalDialog::AcceptWindow() {
#if defined(OS_WIN)
  views::DialogClientView* client_view =
      dialog_->window()->GetClientView()->AsDialogClientView();
  client_view->AcceptWindow();
#endif
}

void CookiePromptModalDialog::CancelWindow() {
#if defined(OS_WIN)
  views::DialogClientView* client_view =
      dialog_->window()->GetClientView()->AsDialogClientView();
  client_view->CancelWindow();
#endif
}


NativeDialog CookiePromptModalDialog::CreateNativeDialog() {
#if defined(OS_WIN)
  if (cookie_ui_) {
    return new CookiePromptView(this,
                                 tab_contents_->GetMessageBoxRootWindow(),
                                 tab_contents_->profile(),
                                 host_, cookie_line_, delegate_);
  }
  return new CookiePromptView(this,
                               tab_contents_->GetMessageBoxRootWindow(),
                               tab_contents_->profile(),
                               storage_info_, delegate_);
#else
  return NULL;
#endif
}
