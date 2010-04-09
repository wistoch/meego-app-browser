// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
#define CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_

#include "chrome/browser/js_modal_dialog.h"

#include <string>

#include "app/message_box_flags.h"
#include "chrome/browser/jsmessage_box_client.h"
#include "chrome/browser/views/modal_dialog_delegate.h"

class MessageBoxView;
class JavaScriptMessageBoxClient;

class JavaScriptMessageBoxDialog : public ModalDialogDelegate {
 public:
  JavaScriptMessageBoxDialog(JavaScriptAppModalDialog* parent,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox);

  virtual ~JavaScriptMessageBoxDialog();

  // ModalDialogDelegate overrides.
  virtual gfx::NativeWindow GetDialogRootWindow();

  // views::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual void DeleteDelegate();
  virtual bool Cancel();
  virtual bool Accept();
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;

  // views::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();

 private:
  JavaScriptMessageBoxClient* client() {
    return parent_->client();
  }

  // A pointer to the AppModalDialog that owns us.
  JavaScriptAppModalDialog* parent_;

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptMessageBoxDialog);
};

#endif  // CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
