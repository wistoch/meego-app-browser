// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_MOZC_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_MOZC_CONFIG_VIEW_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class MozcCombobox;
class MozcComboboxModel;

// A dialog box for showing Mozc (Japanese input method) preferences.
class LanguageMozcConfigView : public views::Combobox::Listener,
                               public views::DialogDelegate,
                               public OptionsPageView {
 public:
  explicit LanguageMozcConfigView(Profile* profile);
  virtual ~LanguageMozcConfigView();

  // views::Combobox::Listener overrides.
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // views::DialogDelegate overrides.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }
  virtual std::wstring GetWindowTitle() const;

  // views::View overrides.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // OptionsPageView overrides.
  virtual void InitControlLayout();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Updates the mozc keyboard combobox.
  void NotifyPrefChanged();

  views::View* contents_;

  struct MozcPrefAndAssociatedCombobox {
    StringPrefMember multiple_choice_pref;
    MozcComboboxModel* combobox_model;
    MozcCombobox* combobox;
  } prefs_and_comboboxes_[kNumMozcMultipleChoicePrefs];

  DISALLOW_COPY_AND_ASSIGN(LanguageMozcConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_MOZC_CONFIG_VIEW_H_
