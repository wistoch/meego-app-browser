// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/table_model.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view2.h"
#include "views/controls/table/table_view_observer.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class InputMethodButton;
class InputMethodCheckbox;
class PreferredLanguageTableModel;
// A dialog box for showing a password textfield.
class LanguageConfigView : public TableModel,
                           public views::ButtonListener,
                           public views::DialogDelegate,
                           public views::TableViewObserver,
                           public views::View {
 public:
  LanguageConfigView();
  virtual ~LanguageConfigView();

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // views::DialogDelegate overrides.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }
  virtual std::wstring GetWindowTitle() const;

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // views::TableViewObserver overrides:
  virtual void OnSelectionChanged();

  // TableModel overrides:
  // To workaround crbug.com/38266, implement TreeModel as part of
  // LanguageConfigView class, rather than a separate class.
  // TODO(satorux): Implement TableModel as a separate class once the bug
  // is fixed.
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);
  virtual int RowCount();

  // Invoked when a language is added from the add button.
  void OnAddLanguage(const std::string& language_code);

 private:
  // Initializes the view.
  void Init();

  // Initializes the preferred language table codes based on the active
  // input languages.
  void InitPreferredLanguageCodes();

  // Initializes the input method config view.
  void InitInputMethodConfigViewMap();

  // Creates the contents on the left, including the language table.
  views::View* CreateContentsOnLeft();

  // Creates the per-language config view.
  views::View* CreatePerLanguageConfigView(const std::string& language_code);

  // Deactivates the input languages for the given language code.
  void DeactivateInputLanguagesFor(const std::string& language_code);

  // Creates the input method config view based on the given |language|.
  // Returns NULL if the config view is not found.
  views::DialogDelegate* CreateInputMethodConfigureView(
      const InputLanguage& language);

  // The language library interface.
  LanguageLibrary* language_library_;
  // The codes of the preferred languages.
  std::vector<std::string> preferred_language_codes_;
  // The map of the input language id to a pointer to the function for
  // creating the input method configuration dialog.
  typedef views::DialogDelegate* (*CreateDialogDelegateFunction)();
  typedef std::map<std::string,
                   CreateDialogDelegateFunction> InputMethodConfigViewMap;
  InputMethodConfigViewMap input_method_config_view_map_;

  // The buttons for configuring input methods for a language.
  std::set<InputMethodButton*> input_method_buttons_;
  // The checkboxes for activating input methods for a language.
  std::set<InputMethodCheckbox*> input_method_checkboxes_;

  views::View* root_container_;
  views::View* right_container_;
  views::NativeButton* add_language_button_;
  views::NativeButton* remove_language_button_;
  views::TableView2* preferred_language_table_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
