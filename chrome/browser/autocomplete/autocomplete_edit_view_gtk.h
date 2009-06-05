// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/owned_widget_gtk.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompletePopupPositioner;
class AutocompletePopupViewGtk;
class CommandUpdater;
class Profile;
class TabContents;

class AutocompleteEditViewGtk : public AutocompleteEditView {
 public:
  AutocompleteEditViewGtk(AutocompleteEditController* controller,
                          ToolbarModel* toolbar_model,
                          Profile* profile,
                          CommandUpdater* command_updater,
                          AutocompletePopupPositioner* popup_positioner);
  ~AutocompleteEditViewGtk();

  // Initialize, create the underlying widgets, etc.
  void Init();

  GtkWidget* widget() { return alignment_.get(); }

  // Grab keyboard input focus, putting focus on the location widget.
  void SetFocus();

  // Implement the AutocompleteEditView interface.
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

  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection);
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length);
  virtual void OnRevertTemporaryText();
  virtual void OnBeforePossibleChange();
  virtual bool OnAfterPossibleChange();

 private:
  // Modeled like the Windows CHARRANGE.  Represent a pair of cursor position
  // offsets.  Since GtkTextIters are invalid after the buffer is changed, we
  // work in character offsets (not bytes).
  struct CharRange {
    CharRange() : cp_min(0), cp_max(0) { }
    CharRange(int n, int x) : cp_min(n), cp_max(x) { }

    // Work in integers to match the gint GTK APIs.
    int cp_min;  // For a selection: Represents the start.
    int cp_max;  // For a selection: Represents the end (insert position).
  };

  // TODO(deanm): Would be nice to insulate the thunkers better, etc.
  static void HandleBeginUserActionThunk(GtkTextBuffer* unused, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleBeginUserAction();
  }
  void HandleBeginUserAction();

  static void HandleEndUserActionThunk(GtkTextBuffer* unused, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleEndUserAction();
  }
  void HandleEndUserAction();

  static gboolean HandleKeyPressThunk(GtkWidget* widget,
                                      GdkEventKey* event,
                                      gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleKeyPress(
        widget, event);
  }
  gboolean HandleKeyPress(GtkWidget* widget, GdkEventKey* event);

  static gboolean HandleKeyReleaseThunk(GtkWidget* widget,
                                        GdkEventKey* event,
                                        gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleKeyRelease(
        widget, event);
  }
  gboolean HandleKeyRelease(GtkWidget* widget, GdkEventKey* event);

  static gboolean HandleViewButtonPressThunk(GtkWidget* view,
                                             GdkEventButton* event,
                                             gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewButtonPress(event);
  }
  gboolean HandleViewButtonPress(GdkEventButton* event);

  static gboolean HandleViewFocusInThunk(GtkWidget* view,
                                          GdkEventFocus* event,
                                          gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewFocusIn();
  }
  gboolean HandleViewFocusIn();

  static gboolean HandleViewFocusOutThunk(GtkWidget* view,
                                          GdkEventFocus* event,
                                          gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewFocusOut();
  }
  gboolean HandleViewFocusOut();

  static void HandleViewMoveCursorThunk(GtkWidget* view,
                                        GtkMovementStep step,
                                        gint count,
                                        gboolean extend_selection,
                                        gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewMoveCursor(step, count, extend_selection);
  }
  void HandleViewMoveCursor(GtkMovementStep step,
                            gint count,
                            gboolean extendion_selection);

  static void HandleViewSizeRequestThunk(GtkWidget* view,
                                         GtkRequisition* req,
                                         gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewSizeRequest(req);
  }
  void HandleViewSizeRequest(GtkRequisition* req);

  // Get the character indices of the current selection.  This honors
  // direction, cp_max is the insertion point, and cp_min is the bound.
  CharRange GetSelection();

  // Translate from character positions to iterators for the current buffer.
  void ItersFromCharRange(const CharRange& range,
                          GtkTextIter* iter_min,
                          GtkTextIter* iter_max);

  // Return the number of characers in the current buffer.
  int GetTextLength();

  // Try to parse the current text as a URL and colorize the components.
  void EmphasizeURLComponents();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // The widget we expose, used for vertically centering the real text edit,
  // since the height will change based on the font / font size, etc.
  OwnedWidgetGtk alignment_;

  // The actual text entry which will be owned by the alignment_.
  GtkWidget* text_view_;

  GtkTextTagTable* tag_table_;
  GtkTextBuffer* text_buffer_;
  GtkTextTag* base_tag_;
  GtkTextTag* secure_scheme_tag_;
  GtkTextTag* insecure_scheme_tag_;
  GtkTextTag* black_text_tag_;

  scoped_ptr<AutocompleteEditModel> model_;
  scoped_ptr<AutocompletePopupViewGtk> popup_view_;
  AutocompleteEditController* controller_;
  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  // TODO(deanm).
  bool popup_window_mode_;

  ToolbarModel::SecurityLevel scheme_security_level_;

  // Tracking state before and after a possible change.
  std::wstring text_before_change_;
  CharRange sel_before_change_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewGtk);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
