// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_MAC_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/cocoa/autocomplete_text_field.h"

class AutocompleteEditController;
class AutocompletePopupViewMac;
class BubblePositioner;
class Clipboard;
class CommandUpdater;
class Profile;
class ToolbarModel;

// Implements AutocompleteEditView on an AutocompleteTextField.

class AutocompleteEditViewMac : public AutocompleteEditView,
                                public AutocompleteTextFieldObserver {
 public:
  AutocompleteEditViewMac(AutocompleteEditController* controller,
                          const BubblePositioner* bubble_positioner,
                          ToolbarModel* toolbar_model,
                          Profile* profile,
                          CommandUpdater* command_updater,
                          AutocompleteTextField* field);
  virtual ~AutocompleteEditViewMac();

  // Implement the AutocompleteEditView interface.
  // TODO(shess): See if this couldn't be simplified to:
  //    virtual AEM* model() const { ... }
  virtual AutocompleteEditModel* model() { return model_.get(); }
  virtual const AutocompleteEditModel* model() const { return model_.get(); }

  virtual void SaveStateToTab(TabContents* tab);
  virtual void Update(const TabContents* tab_for_state_restoring);

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const std::wstring& keyword);

  virtual std::wstring GetText() const;
  virtual void SetUserText(const std::wstring& text) {
    SetUserText(text, text, true);
  }
  virtual void SetUserText(const std::wstring& text,
                           const std::wstring& display_text,
                           bool update_popup);

  virtual void SetWindowTextAndCaretPos(const std::wstring& text,
                                        size_t caret_pos);

  virtual void SetForcedQuery();

  virtual bool IsSelectAll();

  virtual void SelectAll(bool reversed);
  virtual void RevertAll();
  virtual void UpdatePopup();
  virtual void ClosePopup();
  virtual void SetFocus();
  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection);
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length);
  virtual void OnRevertTemporaryText();
  virtual void OnBeforePossibleChange();
  virtual bool OnAfterPossibleChange();
  virtual gfx::NativeView GetNativeView() const;

  // Implement the AutocompleteTextFieldObserver interface.
  virtual void OnControlKeyChanged(bool pressed);
  virtual void OnCopy();
  virtual void OnPaste();
  virtual bool CanPasteAndGo();
  virtual int GetPasteActionStringId();
  virtual void OnPasteAndGo();
  virtual void OnFrameChanged();
  virtual void OnDidResignKey();  // Closes the popup.
  virtual void OnDidBeginEditing();
  virtual void OnDidChange();
  virtual void OnDidEndEditing();
  virtual bool OnDoCommandBySelector(SEL cmd);

  // Helper for LocationBarViewMac.  Selects all in |field_|.
  void FocusLocation();

  // Helper to get appropriate contents from |clipboard|.  Returns
  // empty string if no appropriate data is found on |clipboard|.
  static std::wstring GetClipboardText(Clipboard* clipboard);

 private:
  // Called when the user hits backspace in |field_|.  Checks whether
  // keyword search is being terminated.  Returns true if the
  // backspace should be intercepted (not forwarded on to the standard
  // machinery).
  bool OnBackspacePressed();

  // Returns the field's currently selected range.  Only valid if the
  // field has focus.
  NSRange GetSelectedRange() const;

  // Returns true if |field_| is first-responder in the window.  Used
  // in various DCHECKS to make sure code is running in appropriate
  // situations.
  bool IsFirstResponder() const;

  // If |model_| believes it has focus, grab focus if needed and set
  // the selection to |range|.  Otherwise does nothing.
  void SetSelectedRange(const NSRange range);

  // Update the field with |display_text| and highlight the host and
  // scheme (if it's an URL or URL-fragment).
  void SetText(const std::wstring& display_text);

  // Update the field with |display_text| and set the selection.
  void SetTextAndSelectedRange(const std::wstring& display_text,
                               const NSRange range);

  // Pass the current content of |field_| to SetText(), maintaining
  // any selection.  Named to be consistent with GTK and Windows,
  // though here we cannot really do the in-place operation they do.
  void EmphasizeURLComponents();

  scoped_ptr<AutocompleteEditModel> model_;
  scoped_ptr<AutocompletePopupViewMac> popup_view_;

  AutocompleteEditController* controller_;
  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  AutocompleteTextField* field_;  // owned by tab controller

  // Selection at the point where the user started using the
  // arrows to move around in the popup.
  NSRange saved_temporary_selection_;

  // Tracking state before and after a possible change for reporting
  // to model_.
  NSRange selection_before_change_;
  std::wstring text_before_change_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewMac);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_MAC_H_
