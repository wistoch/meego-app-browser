// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
#pragma once

#include <string>

#include "app/message_box_flags.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Button;
class Label;
class RadioButton;
class Textfield;
}  // namespace views

namespace chromeos {

// A dialog box that is shown when password change was detected.
// User is presented with an option to sync all settings or
// enter old password and sync only delta.
class PasswordChangedView : public views::View,
                            public views::DialogDelegate,
                            public views::ButtonListener,
                            public views::Textfield::Controller {
 public:
  // Delegate class to get notifications from the view.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // User provided |old_password|, decrypt homedir and sync only delta.
    virtual void RecoverEncryptedData(const std::string& old_password) = 0;

    // Ignores password change and forces full sync.
    virtual void ResyncEncryptedData() = 0;
  };

  explicit PasswordChangedView(Delegate* delegate);
  virtual ~PasswordChangedView() {}

  // views::DialogDelegate overrides:
  virtual bool Accept();
  virtual int GetDialogButtons() const;

  // views::WindowDelegate overrides:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::View overrides:
  virtual std::wstring GetWindowTitle() const;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // views::Textfield::Controller overrides:
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

  // Selects delta sync radio button.
  void SelectDeltaSyncOption();

 protected:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // Called when dialog is accepted.
  bool ExitDialog();

  // Initialize view layout.
  void Init();

  // Screen controls.
  views::Label* title_label_;
  views::Label* description_label_;
  views::RadioButton* full_sync_radio_;
  views::RadioButton* delta_sync_radio_;
  views::Textfield* old_password_field_;

  // Notifications receiver.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PasswordChangedView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
