// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CLEAR_BROWSING_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_CLEAR_BROWSING_DATA_H_
#pragma once

#include "chrome/browser/browsing_data_remover.h"
#include "ui/base/models/combobox_model.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Label;
class Throbber;
class Window;
}

class Profile;
class MessageLoop;

////////////////////////////////////////////////////////////////////////////////
//
// The ClearBrowsingData class is responsible for drawing the UI controls of the
// dialog that allows the user to select what to delete (history, downloads,
// etc).
//
////////////////////////////////////////////////////////////////////////////////
class ClearBrowsingDataView : public views::View,
                              public views::DialogDelegate,
                              public views::ButtonListener,
                              public ui::ComboboxModel,
                              public views::Combobox::Listener,
                              public BrowsingDataRemover::Observer,
                              public views::LinkController {
 public:
  explicit ClearBrowsingDataView(Profile* profile);
  virtual ~ClearBrowsingDataView(void);

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate:
  virtual int GetDefaultDialogButton() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();
  virtual views::ClientView* CreateClientView(views::Window* window);
  virtual views::View* GetExtraView();
  virtual bool GetSizeExtraViewHeightToButtons();
  virtual views::View* GetInitiallyFocusedView();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender, int prev_index,
                           int new_index);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overriden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const std::wstring& text, bool checked);

  // Sets the controls on the UI to be enabled/disabled depending on whether we
  // have a delete operation in progress or not.
  void UpdateControlEnabledState();

  // Starts the process of deleting the browsing data depending on what the
  // user selected.
  void OnDelete();

  // Callback from BrowsingDataRemover. Closes the dialog.
  virtual void OnBrowsingDataRemoverDone();

  // UI elements we add to the parent view.
  views::View* throbber_view_;
  views::Throbber* throbber_;
  views::Label* status_label_;
  // Other UI elements.
  views::Label* delete_all_label_;
  views::Checkbox* del_history_checkbox_;
  views::Checkbox* del_downloads_checkbox_;
  views::Checkbox* del_cache_checkbox_;
  views::Checkbox* del_cookies_checkbox_;
  views::Checkbox* del_passwords_checkbox_;
  views::Checkbox* del_form_data_checkbox_;
  views::Label* time_period_label_;
  views::Combobox* time_period_combobox_;

  // Used to signal enabled/disabled state for controls in the UI.
  bool delete_in_progress_;

  Profile* profile_;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CLEAR_BROWSING_DATA_H_
