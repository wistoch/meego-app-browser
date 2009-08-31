// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <algorithm>

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

#if !defined(TOOLKIT_VIEWS)
#include "chrome/browser/gtk/gtk_theme_provider.h"
#endif

namespace {

const char kTextBaseColor[] = "#808080";
const char kSecureSchemeColor[] = "#009614";
const char kInsecureSchemeColor[] = "#c80000";

size_t GetUTF8Offset(const std::wstring& wide_text, size_t wide_text_offset) {
  return WideToUTF8(wide_text.substr(0, wide_text_offset)).size();
}

// Stores GTK+-specific state so it can be restored after switching tabs.
struct ViewState {
  explicit ViewState(const AutocompleteEditViewGtk::CharRange& selection_range)
      : selection_range(selection_range) {
  }

  // Range of selected text.
  AutocompleteEditViewGtk::CharRange selection_range;
};

struct AutocompleteEditState {
  AutocompleteEditState(const AutocompleteEditModel::State& model_state,
                        const ViewState& view_state)
      : model_state(model_state),
        view_state(view_state) {
  }

  const AutocompleteEditModel::State model_state;
  const ViewState view_state;
};

// Returns a lazily initialized property bag accessor for saving our state in a
// TabContents.
PropertyAccessor<AutocompleteEditState>* GetStateAccessor() {
  static PropertyAccessor<AutocompleteEditState> state;
  return &state;
}

}  // namespace

AutocompleteEditViewGtk::AutocompleteEditViewGtk(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater,
    bool popup_window_mode,
    AutocompletePopupPositioner* popup_positioner)
    : text_view_(NULL),
      tag_table_(NULL),
      text_buffer_(NULL),
      faded_text_tag_(NULL),
      secure_scheme_tag_(NULL),
      insecure_scheme_tag_(NULL),
      model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(AutocompletePopupView::CreatePopupView(gfx::Font(), this,
                                                         model_.get(),
                                                         profile,
                                                         popup_positioner)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(popup_window_mode),
      scheme_security_level_(ToolbarModel::NORMAL),
      mark_set_handler_id_(0),
      button_1_pressed_(false),
      text_selected_during_click_(false),
      text_view_focused_before_button_press_(false),
#if !defined(TOOLKIT_VIEWS)
      theme_provider_(GtkThemeProvider::GetFrom(profile)),
#endif
      tab_was_pressed_(false),
      paste_clipboard_requested_(false) {
  model_->set_popup_model(popup_view_->GetModel());
}

AutocompleteEditViewGtk::~AutocompleteEditViewGtk() {
  NotificationService::current()->Notify(
      NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
      Source<AutocompleteEditViewGtk>(this),
      NotificationService::NoDetails());

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
  model_.reset();

  // We own our widget and TextView related objects.
  if (alignment_.get()) {  // Init() has been called.
    alignment_.Destroy();
    g_object_unref(text_buffer_);
    g_object_unref(tag_table_);
    // The tags we created are owned by the tag_table, and should be destroyed
    // along with it.  We don't hold our own reference to them.
  }
}

void AutocompleteEditViewGtk::Init() {
  // The height of the text view is going to change based on the font used.  We
  // don't want to stretch the height, and we want it vertically centered.
  alignment_.Own(gtk_alignment_new(0., 0.5, 1.0, 0.0));

  // The GtkTagTable and GtkTextBuffer are not initially unowned, so we have
  // our own reference when we create them, and we own them.  Adding them to
  // the other objects adds a reference; it doesn't adopt them.
  tag_table_ = gtk_text_tag_table_new();
  text_buffer_ = gtk_text_buffer_new(tag_table_);
  text_view_ = gtk_text_view_new_with_buffer(text_buffer_);
  if (popup_window_mode_)
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_), false);

  // Until we switch to vector graphics, force the font size.
  gtk_util::ForceFontSizePixels(text_view_,
      popup_window_mode_ ?
      browser_defaults::kAutocompleteEditFontPixelSizeInPopup :
      browser_defaults::kAutocompleteEditFontPixelSize);

  // The text view was floating.  It will now be owned by the alignment.
  gtk_container_add(GTK_CONTAINER(alignment_.get()), text_view_);

  // Do not allow inserting tab characters when pressing Tab key, so that when
  // Tab key is pressed, |text_view_| will emit "move-focus" signal, which will
  // be intercepted by our own handler to trigger Tab to search feature when
  // necessary.
  gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view_), FALSE);

  faded_text_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kTextBaseColor, NULL);
  secure_scheme_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kSecureSchemeColor, NULL);
  insecure_scheme_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kInsecureSchemeColor, NULL);
  normal_text_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", "#000000", NULL);

  // NOTE: This code used to connect to "changed", however this was fired too
  // often and during bad times (our own buffer changes?).  It works out much
  // better to listen to end-user-action, which should be fired whenever the
  // user makes some sort of change to the buffer.
  g_signal_connect(text_buffer_, "begin-user-action",
                   G_CALLBACK(&HandleBeginUserActionThunk), this);
  g_signal_connect(text_buffer_, "end-user-action",
                   G_CALLBACK(&HandleEndUserActionThunk), this);
  g_signal_connect(text_buffer_, "insert-text",
                   G_CALLBACK(&HandleInsertTextThunk), this);
  // We connect to key press and release for special handling of a few keys.
  g_signal_connect(text_view_, "key-press-event",
                   G_CALLBACK(&HandleKeyPressThunk), this);
  g_signal_connect(text_view_, "key-release-event",
                   G_CALLBACK(&HandleKeyReleaseThunk), this);
  g_signal_connect(text_view_, "button-press-event",
                   G_CALLBACK(&HandleViewButtonPressThunk), this);
  g_signal_connect(text_view_, "button-release-event",
                   G_CALLBACK(&HandleViewButtonReleaseThunk), this);
  g_signal_connect(text_view_, "focus-in-event",
                   G_CALLBACK(&HandleViewFocusInThunk), this);
  g_signal_connect(text_view_, "focus-out-event",
                   G_CALLBACK(&HandleViewFocusOutThunk), this);
  // NOTE: The GtkTextView documentation asks you not to connect to this
  // signal, but it is very convenient and clean for catching up/down.
  g_signal_connect(text_view_, "move-cursor",
                   G_CALLBACK(&HandleViewMoveCursorThunk), this);
  g_signal_connect(text_view_, "move-focus",
                   G_CALLBACK(&HandleViewMoveFocusThunk), this);
  // Override the size request.  We want to keep the original height request
  // from the widget, since that's font dependent.  We want to ignore the width
  // so we don't force a minimum width based on the text length.
  g_signal_connect(text_view_, "size-request",
                   G_CALLBACK(&HandleViewSizeRequestThunk), this);
  g_signal_connect(text_view_, "populate-popup",
                   G_CALLBACK(&HandlePopulatePopupThunk), this);
  mark_set_handler_id_ = g_signal_connect(
      text_buffer_, "mark-set", G_CALLBACK(&HandleMarkSetThunk), this);
  g_signal_connect(text_view_, "drag-data-received",
                   G_CALLBACK(&HandleDragDataReceivedThunk), this);
  g_signal_connect(text_view_, "backspace",
                   G_CALLBACK(&HandleBackSpaceThunk), this);
  g_signal_connect(text_view_, "copy-clipboard",
                   G_CALLBACK(&HandleCopyClipboardThunk), this);
  g_signal_connect(text_view_, "paste-clipboard",
                   G_CALLBACK(&HandlePasteClipboardThunk), this);

#if !defined(TOOLKIT_VIEWS)
  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
#else
  // Manually invoke SetBaseColor() because TOOLKIT_VIEWS doesn't observe
  // themes.
  SetBaseColor();
#endif

  ViewIDUtil::SetID(widget(), VIEW_ID_AUTOCOMPLETE);
}

void AutocompleteEditViewGtk::SetFocus() {
  gtk_widget_grab_focus(text_view_);
}

void AutocompleteEditViewGtk::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);
  GetStateAccessor()->SetProperty(
      tab->property_bag(),
      AutocompleteEditState(model_->GetStateForTabSwitch(),
                            ViewState(GetSelection())));

  // If any text has been selected, register it as the PRIMARY selection so it
  // can still be pasted via middle-click after the text view is cleared.
  if (!selected_text_.empty()) {
    SavePrimarySelection(selected_text_);
  }
}

void AutocompleteEditViewGtk::Update(const TabContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(toolbar_model_->GetText());

  ToolbarModel::SecurityLevel security_level =
        toolbar_model_->GetSchemeSecurityLevel();
  bool changed_security_level = (security_level != scheme_security_level_);
  scheme_security_level_ = security_level;

  // TODO(deanm): This doesn't exactly match Windows.  There there is a member
  // background_color_.  I think we can get away with just the level though.
  if (changed_security_level) {
    SetBaseColor();
  }

  if (contents) {
    selected_text_.clear();
    RevertAll();
    const AutocompleteEditState* state =
        GetStateAccessor()->GetProperty(contents->property_bag());
    if (state) {
      model_->RestoreState(state->model_state);

      // Move the marks for the cursor and the other end of the selection to
      // the previously-saved offsets.
      GtkTextIter selection_iter, insert_iter;
      ItersFromCharRange(
          state->view_state.selection_range, &selection_iter, &insert_iter);
      // TODO(derat): Restore the selection range instead of just the cursor
      // ("insert") position.  This in itself is trivial to do using
      // gtk_text_buffer_select_range(), but then it also becomes necessary to
      // invalidate hidden tabs' saved ranges when another tab or another app
      // takes the selection, lest we incorrectly regrab a stale selection when
      // a hidden tab is later shown.
      gtk_text_buffer_place_cursor(text_buffer_, &insert_iter);
    }
  } else if (visibly_changed_permanent_text) {
    RevertAll();
    // TODO(deanm): There should be code to restore select all here.
  } else if (changed_security_level) {
    EmphasizeURLComponents();
  }
}

void AutocompleteEditViewGtk::OpenURL(const GURL& url,
                                      WindowOpenDisposition disposition,
                                      PageTransition::Type transition,
                                      const GURL& alternate_nav_url,
                                      size_t selected_line,
                                      const std::wstring& keyword) {
  if (!url.is_valid())
    return;

  model_->SendOpenNotification(selected_line, keyword);

  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state.
  controller_->OnAutocompleteAccept(url, disposition, transition,
                                    alternate_nav_url);
}

std::wstring AutocompleteEditViewGtk::GetText() const {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gchar* utf8 = gtk_text_buffer_get_text(text_buffer_, &start, &end, false);
  std::wstring out(UTF8ToWide(utf8));
  g_free(utf8);
  return out;
}

void AutocompleteEditViewGtk::SetUserText(const std::wstring& text,
                                          const std::wstring& display_text,
                                          bool update_popup) {
  model_->SetUserText(text);
  // TODO(deanm): something about selection / focus change here.
  SetWindowTextAndCaretPos(display_text, display_text.length());
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void AutocompleteEditViewGtk::SetWindowTextAndCaretPos(const std::wstring& text,
                                                       size_t caret_pos) {
  CharRange range(static_cast<int>(caret_pos), static_cast<int>(caret_pos));
  SetTextAndSelectedRange(text, range);
}

void AutocompleteEditViewGtk::SetTextAndSelectedRange(const std::wstring& text,
                                                      const CharRange& range) {
  std::string utf8 = WideToUTF8(text);
  gtk_text_buffer_set_text(text_buffer_, utf8.data(), utf8.length());

  GtkTextIter insert, bound;
  ItersFromCharRange(range, &insert, &bound);
  gtk_text_buffer_select_range(text_buffer_, &insert, &bound);
}

void AutocompleteEditViewGtk::SetForcedQuery() {
  const std::wstring current_text(GetText());
  if (current_text.empty() || (current_text[0] != '?')) {
    SetUserText(L"?");
  } else {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
    gtk_text_buffer_get_iter_at_offset(text_buffer_, &start, 1);
    StartUpdatingHighlightedText();
    gtk_text_buffer_select_range(text_buffer_, &start, &end);
    FinishUpdatingHighlightedText();
  }
}

bool AutocompleteEditViewGtk::IsSelectAll() {
  GtkTextIter sel_start, sel_end;
  gtk_text_buffer_get_selection_bounds(text_buffer_, &sel_start, &sel_end);

  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);

  // Returns true if the |text_buffer_| is empty.
  return gtk_text_iter_equal(&start, &sel_start) &&
      gtk_text_iter_equal(&end, &sel_end);
}

void AutocompleteEditViewGtk::SelectAll(bool reversed) {
  // SelectAll() is invoked as a side effect of other actions (e.g.  switching
  // tabs or hitting Escape) in autocomplete_edit.cc, so we don't update the
  // PRIMARY selection here.
  // TODO(derat): But this is also called by LocationBarView::FocusLocation() --
  // should the X selection be updated when the user hits Ctrl-L?
  SelectAllInternal(reversed, false);
}

void AutocompleteEditViewGtk::RevertAll() {
  ClosePopup();
  model_->Revert();
  TextChanged();
}

void AutocompleteEditViewGtk::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // Don't inline autocomplete when the caret/selection isn't at the end of
  // the text.
  CharRange sel = GetSelection();
  model_->StartAutocomplete(std::max(sel.cp_max, sel.cp_min) < GetTextLength());
}

void AutocompleteEditViewGtk::ClosePopup() {
  popup_view_->GetModel()->StopAutocomplete();
}

void AutocompleteEditViewGtk::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text,
    bool save_original_selection) {
  if (save_original_selection) {
    saved_temporary_selection_ = GetSelection();
    saved_temporary_text_ = GetText();
  }

  StartUpdatingHighlightedText();
  SetWindowTextAndCaretPos(display_text, display_text.length());
  FinishUpdatingHighlightedText();
  TextChanged();
}

bool AutocompleteEditViewGtk::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;

  StartUpdatingHighlightedText();
  SetWindowTextAndCaretPos(display_text, 0);

  // Select the part of the text that was inline autocompleted.
  GtkTextIter bound, insert;
  gtk_text_buffer_get_bounds(text_buffer_, &insert, &bound);
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &insert, user_text_length);
  gtk_text_buffer_select_range(text_buffer_, &insert, &bound);

  FinishUpdatingHighlightedText();
  TextChanged();
  return true;
}

void AutocompleteEditViewGtk::OnRevertTemporaryText() {
  StartUpdatingHighlightedText();
  SetTextAndSelectedRange(saved_temporary_text_, saved_temporary_selection_);
  FinishUpdatingHighlightedText();
  saved_temporary_text_.clear();
  TextChanged();
}

void AutocompleteEditViewGtk::OnBeforePossibleChange() {
  // If this change is caused by a paste clipboard action and all text is
  // selected, then call model_->on_paste_replacing_all() to prevent inline
  // autocomplete.
  if (paste_clipboard_requested_) {
    paste_clipboard_requested_ = false;
    if (IsSelectAll())
      model_->on_paste_replacing_all();
  }

  // Record our state.
  text_before_change_ = GetText();
  sel_before_change_ = GetSelection();
}

// TODO(deanm): This is mostly stolen from Windows, and will need some work.
bool AutocompleteEditViewGtk::OnAfterPossibleChange() {
  CharRange new_sel = GetSelection();
  int length = GetTextLength();
  bool selection_differs = (new_sel.cp_min != sel_before_change_.cp_min) ||
                           (new_sel.cp_max != sel_before_change_.cp_max);
  bool at_end_of_edit = (new_sel.cp_min == length && new_sel.cp_max == length);

  // See if the text or selection have changed since OnBeforePossibleChange().
  std::wstring new_text(GetText());
  bool text_differs = (new_text != text_before_change_);

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.cp_min <= std::min(sel_before_change_.cp_min,
                                 sel_before_change_.cp_max));

  bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_differs, just_deleted_text, at_end_of_edit);

  if (something_changed && text_differs)
    TextChanged();

  return something_changed;
}

gfx::NativeView AutocompleteEditViewGtk::GetNativeView() const {
  return alignment_.get();
}

void AutocompleteEditViewGtk::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);

  SetBaseColor();
}

void AutocompleteEditViewGtk::SetBaseColor() {
#if defined(TOOLKIT_VIEWS)
  bool use_gtk = false;
#else
  bool use_gtk = theme_provider_->UseGtkTheme();
#endif

  // If we're on a secure connection, ignore what the theme wants us to do
  // and use a yellow background.
  if (use_gtk && scheme_security_level_ != ToolbarModel::SECURE) {
    gtk_widget_modify_base(text_view_, GTK_STATE_NORMAL, NULL);

    // Grab the text colors out of the style and set our tags to use them.
    GtkStyle* style = gtk_rc_get_style(text_view_);

    // style may be unrealized at this point, so calculate the halfway point
    // between text[] and base[] manually instead of just using text_aa[].
    GdkColor average_color = gtk_util::AverageColors(
        style->text[GTK_STATE_NORMAL], style->base[GTK_STATE_NORMAL]);

    g_object_set(G_OBJECT(faded_text_tag_), "foreground-gdk",
                 &average_color, NULL);
    g_object_set(G_OBJECT(normal_text_tag_), "foreground-gdk",
                 &style->text[GTK_STATE_NORMAL], NULL);
  } else {
    gtk_widget_modify_base(text_view_, GTK_STATE_NORMAL,
        &LocationBarViewGtk::kBackgroundColorByLevel[scheme_security_level_]);

    g_object_set(G_OBJECT(faded_text_tag_), "foreground", kTextBaseColor, NULL);
    g_object_set(G_OBJECT(normal_text_tag_), "foreground", "#000000", NULL);
  }
}

void AutocompleteEditViewGtk::HandleBeginUserAction() {
  OnBeforePossibleChange();
}

void AutocompleteEditViewGtk::HandleEndUserAction() {
  OnAfterPossibleChange();
}

gboolean AutocompleteEditViewGtk::HandleKeyPress(GtkWidget* widget,
                                                 GdkEventKey* event) {
  // Background of this piece of complicated code:
  // The omnibox supports several special behaviors which may be triggered by
  // certain key events:
  // Tab to search - triggered by Tab key
  // Accept input - triggered by Enter key
  // Revert input - triggered by Escape key
  //
  // Because we use a GtkTextView object |text_view_| for text input, we need
  // send all key events to |text_view_| before handling them, to make sure
  // IME works without any problem. So here, we intercept "key-press-event"
  // signal of |text_view_| object and call its default handler to handle the
  // key event first.
  //
  // Then if the key event is one of Tab, Enter and Escape, we need to trigger
  // the corresponding special behavior if IME did not handle it.
  // For Escape key, if the default signal handler returns FALSE, then we know
  // it's not handled by IME.
  //
  // For Tab key, as "accepts-tab" property of |text_view_| is set to FALSE,
  // if IME did not handle it then "move-focus" signal will be emitted by the
  // default signal handler of |text_view_|. So we can intercept "move-focus"
  // signal of |text_view_| to know if a Tab key press event was handled by IME,
  // and trigger Tab to search behavior when necessary in the signal handler.
  //
  // But for Enter key, if IME did not handle the key event, the default signal
  // handler will delete current selection range and insert '\n' and always
  // return TRUE. We need to prevent |text_view_| from performing this default
  // action if IME did not handle the key event, because we don't want the
  // content of omnibox to be changed before triggering our special behavior.
  // Otherwise our special behavior would not be performed correctly.
  //
  // But there is no way for us to prevent GtkTextView from handling the key
  // event and performing built-in operation. So in order to achieve our goal,
  // "insert-text" signal of |text_buffer_| object is intercepted, and
  // following actions are done in the signal handler:
  // - If there is only one character in inserted text, save it in
  //   char_inserted_.
  // - Filter out all new line and tab characters.
  //
  // So if |char_inserted_| equals '\n' after calling |text_view_|'s
  // default signal handler against an Enter key press event, then we know that
  // the Enter key press event was handled by GtkTextView rather than IME, and
  // can perform the special behavior for Enter key safely.
  //
  // Now the last thing is to prevent the content of omnibox from being changed
  // by GtkTextView when Tab or Enter key is pressed. Because we can't prevent
  // it, we use a backup and restore trick: If Enter is pressed, backup the
  // content of omnibox before sending the key event to |text_view_|, and
  // then restore it afterwards if IME did not handle the event.

  GtkWidgetClass* klass = GTK_WIDGET_GET_CLASS(widget);

  bool enter_pressed = (event->keyval == GDK_Return ||
                        event->keyval == GDK_ISO_Enter ||
                        event->keyval == GDK_KP_Enter);

  gchar* original_text = NULL;

  // Enter key will have special behavior if it's not handled by IME.
  // We need save the original content of |text_buffer_| and restore it when
  // necessary, because GtkTextView might alter the content.
  if (enter_pressed) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
    original_text = gtk_text_buffer_get_text(text_buffer_, &start, &end, FALSE);
    // Reset |char_inserted_|, which may be set in the "insert-text" signal
    // handler, so that we'll know if an Enter key event was handled by IME.
    char_inserted_ = 0;
  }

  // Set |tab_was_pressed_| to true if it's a Tab key press event, so that our
  // handler of "move-focus" signal can trigger Tab to search behavior when
  // necessary.
  tab_was_pressed_ = ((event->keyval == GDK_Tab ||
                       event->keyval == GDK_ISO_Left_Tab ||
                       event->keyval == GDK_KP_Tab) &&
                      !(event->state & GDK_CONTROL_MASK));

  // Reset |paste_clipboard_requested_| to make sure we won't misinterpret this
  // key input action as a paste action.
  paste_clipboard_requested_ = false;

  // Call the default handler, so that IME can work as normal.
  // New line characters will be filtered out by our "insert-text"
  // signal handler attached to |text_buffer_| object.
  gboolean result = klass->key_press_event(widget, event);

  // Set |tab_was_pressed_| to false, to make sure Tab to search behavior can
  // only be triggered by pressing Tab key.
  tab_was_pressed_ = false;

  if (enter_pressed && (char_inserted_ == '\n' || char_inserted_ == '\r')) {
    bool alt_held = (event->state & GDK_MOD1_MASK);
    // Revert the original text in case the text has been changed.
    // Call gtk_text_buffer_{begin|end}_user_action() to make sure |model_| will
    // be updated correctly.
    // Note: SetUserText() does not work here, it'll reset the keyword state.
    DCHECK(original_text);
    gtk_text_buffer_begin_user_action(text_buffer_);
    gtk_text_buffer_set_text(text_buffer_, original_text, -1);
    gtk_text_buffer_end_user_action(text_buffer_);
    model_->AcceptInput(alt_held ? NEW_FOREGROUND_TAB : CURRENT_TAB, false);
    result = TRUE;
  } else if (!result && event->keyval == GDK_Escape &&
             (event->state & gtk_accelerator_get_default_mod_mask()) == 0) {
    // We can handle the Escape key if |text_view_| did not handle it.
    // If it's not handled by us, then we need to propagate it up to the parent
    // widgets, so that Escape accelerator can still work.
    result = model_->OnEscapeKeyPressed();
  } else if (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R) {
    // Omnibox2 can switch its contents while pressing a control key. To switch
    // the contents of omnibox2, we notify the AutocompleteEditModel class when
    // the control-key state is changed.
    model_->OnControlKeyChanged(true);
  }

  if (original_text)
    g_free(original_text);

  // If the key event is not handled by |text_view_| or us, then we need to
  // propagate the key event up to parent widgets by returning FALSE.
  // In this case we need to stop the signal emission explicitly to prevent the
  // default "key-press-event" handler of |text_view_| from being called again.
  if (!result) {
    static guint signal_id =
        g_signal_lookup("key-press-event", GTK_TYPE_WIDGET);
    g_signal_stop_emission(widget, signal_id, 0);
  }

  return result;
}

gboolean AutocompleteEditViewGtk::HandleKeyRelease(GtkWidget* widget,
                                                   GdkEventKey* event) {
  // Omnibox2 can switch its contents while pressing a control key. To switch
  // the contents of omnibox2, we notify the AutocompleteEditModel class when
  // the control-key state is changed.
  if (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R) {
    // Round trip to query the control state after the release.  This allows
    // you to release one control key while still holding another control key.
    GdkDisplay* display = gdk_drawable_get_display(event->window);
    GdkModifierType mod;
    gdk_display_get_pointer(display, NULL, NULL, NULL, &mod);
    if (!(mod & GDK_CONTROL_MASK))
      model_->OnControlKeyChanged(false);
  }

  // Even though we handled the press ourselves, let GtkTextView handle the
  // release.  It shouldn't do anything particularly interesting, but it will
  // handle the IME work for us.
  return FALSE;  // Propagate into GtkTextView.
}

gboolean AutocompleteEditViewGtk::HandleViewButtonPress(GdkEventButton* event) {
  // We don't need to care about double and triple clicks.
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  if (event->button == 1) {
    // When the first button is pressed, track some stuff that will help us
    // determine whether we should select all of the text when the button is
    // released.
    button_1_pressed_ = true;
    text_view_focused_before_button_press_ = GTK_WIDGET_HAS_FOCUS(text_view_);
    text_selected_during_click_ = false;
  } else if (event->button == 2) {
    // GtkTextView pastes PRIMARY selection with middle click.
    // We can't call model_->on_paste_replacing_all() here, because the actual
    // paste clipboard action may not be performed if the clipboard is empty.
    paste_clipboard_requested_ = true;
  }
  return FALSE;
}

gboolean AutocompleteEditViewGtk::HandleViewButtonRelease(
    GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  button_1_pressed_ = false;

  // Call the GtkTextView default handler, ignoring the fact that it will
  // likely have told us to stop propagating.  We want to handle selection.
  GtkWidgetClass* klass = GTK_WIDGET_GET_CLASS(text_view_);
  klass->button_release_event(text_view_, event);

  if (!text_view_focused_before_button_press_ && !text_selected_during_click_) {
    // If this was a focusing click and the user didn't drag to highlight any
    // text, select the full input and update the PRIMARY selection.
    SelectAllInternal(false, true);

    // So we told the buffer where the cursor should be, but make sure to tell
    // the view so it can scroll it to be visible if needed.
    // NOTE: This function doesn't seem to like a count of 0, looking at the
    // code it will skip an important loop.  Use -1 to achieve the same.
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
    gtk_text_view_move_visually(GTK_TEXT_VIEW(text_view_), &start, -1);
  }

  return TRUE;  // Don't continue, we called the default handler already.
}

gboolean AutocompleteEditViewGtk::HandleViewFocusIn() {
  GdkModifierType modifiers;
  gdk_window_get_pointer(text_view_->window, NULL, NULL, &modifiers);
  model_->OnSetFocus((modifiers & GDK_CONTROL_MASK) != 0);
  // TODO(deanm): Some keyword hit business, etc here.

  return FALSE;  // Continue propagation.
}

gboolean AutocompleteEditViewGtk::HandleViewFocusOut() {
  // Close the popup.
  ClosePopup();
  // Tell the model to reset itself.
  model_->OnKillFocus();
  return FALSE;  // Pass the event on to the GtkTextView.
}

void AutocompleteEditViewGtk::HandleViewMoveCursor(
    GtkMovementStep step,
    gint count,
    gboolean extend_selection) {
  // We want the GtkEntry behavior when you move the cursor while you have a
  // selection.  GtkTextView just drops the selection and moves the cursor, but
  // instead we want to move the cursor to the appropiate end of the selection.
  GtkTextIter sstart, send;
  if (step == GTK_MOVEMENT_VISUAL_POSITIONS &&
      !extend_selection &&
      (count == 1 || count == -1) &&
      gtk_text_buffer_get_selection_bounds(text_buffer_, &sstart, &send)) {
    // We have a selection and start / end are in ascending order.
    gtk_text_buffer_place_cursor(text_buffer_, count == 1 ? &send : &sstart);
  } else if (step == GTK_MOVEMENT_PAGES) {  // Page up and down.
    // Multiply by count for the direction (if we move too much that's ok).
    model_->OnUpOrDownKeyPressed(model_->result().size() * count);
  } else if (step == GTK_MOVEMENT_DISPLAY_LINES) {  // Arrow up and down.
    model_->OnUpOrDownKeyPressed(count);
  } else {
    return;  // Propagate into GtkTextView
  }

  // move-cursor doesn't use a signal accumulator on the return value (it
  // just ignores then), so we have to stop the propagation.
  static guint signal_id = g_signal_lookup("move-cursor", GTK_TYPE_TEXT_VIEW);
  g_signal_stop_emission(text_view_, signal_id, 0);
}

void AutocompleteEditViewGtk::HandleViewSizeRequest(GtkRequisition* req) {
  // Don't force a minimum width, but use the font-relative height.  This is a
  // run-first handler, so the default handler was already called.
  req->width = 1;
}

void AutocompleteEditViewGtk::HandlePopulatePopup(GtkMenu* menu) {
  GtkWidget* separator = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_widget_show(separator);

  // Search Engine menu item.
  GtkWidget* search_engine_menuitem = gtk_menu_item_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_EDIT_SEARCH_ENGINES)).c_str());
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), search_engine_menuitem);
  g_signal_connect(search_engine_menuitem, "activate",
                   G_CALLBACK(HandleEditSearchEnginesThunk), this);
  gtk_widget_show(search_engine_menuitem);

  // We need to update the paste and go controller before we know what text
  // to show. We could do this all asynchronously, but it would be elaborate
  // because we'd have to account for multiple menus showing, getting called
  // back after shutdown, and similar issues.
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gchar* text = gtk_clipboard_wait_for_text(x_clipboard);
  std::wstring text_wstr = UTF8ToWide(text);
  g_free(text);
  bool can_paste_and_go = model_->CanPasteAndGo(text_wstr);

  // Paste and Go menu item.
  GtkWidget* paste_go_menuitem = gtk_menu_item_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(model_->is_paste_and_search() ?
              IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO)).c_str());
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_go_menuitem);
  g_signal_connect(paste_go_menuitem, "activate",
                   G_CALLBACK(HandlePasteAndGoThunk), this);
  gtk_widget_set_sensitive(paste_go_menuitem, can_paste_and_go);
  gtk_widget_show(paste_go_menuitem);
}

void AutocompleteEditViewGtk::HandleEditSearchEngines() {
  command_updater_->ExecuteCommand(IDC_EDIT_SEARCH_ENGINES);
}

void AutocompleteEditViewGtk::HandlePasteAndGo() {
  model_->PasteAndGo();
}

void AutocompleteEditViewGtk::HandleMarkSet(GtkTextBuffer* buffer,
                                            GtkTextIter* location,
                                            GtkTextMark* mark) {
  if (!text_buffer_ || buffer != text_buffer_)
    return;

  if (mark != gtk_text_buffer_get_insert(text_buffer_) &&
      mark != gtk_text_buffer_get_selection_bound(text_buffer_)) {
    return;
  }

  // Get the currently-selected text, if there is any.
  std::string new_selected_text;
  GtkTextIter start, end;
  if (gtk_text_buffer_get_selection_bounds(text_buffer_, &start, &end)) {
    gchar* text = gtk_text_iter_get_text(&start, &end);
    size_t text_len = strlen(text);
    if (text_len) {
      new_selected_text = std::string(text, text_len);
    }
    g_free(text);
  }

  // If the user just selected some text with the mouse (or at least while the
  // mouse button was down), make sure that we won't blow their selection away
  // later by selecting all of the text when the button is released.
  if (button_1_pressed_ && !new_selected_text.empty()) {
    text_selected_during_click_ = true;
  }

  // If we had some text selected earlier but it's no longer highlighted, we
  // might need to save it now...
  if (!selected_text_.empty() && new_selected_text.empty()) {
    // ... but only if we currently own the selection.  We want to manually
    // update the selection when the text is unhighlighted because the user
    // clicked in a blank area of the text view, but not when it's unhighlighted
    // because another client or widget took the selection.  (This handler gets
    // called before the default handler, so as long as nobody else took the
    // selection, the text buffer still owns it even if GTK is about to take it
    // away in the default handler.)
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    if (gtk_clipboard_get_owner(clipboard) == G_OBJECT(text_buffer_)) {
      SavePrimarySelection(selected_text_);
    }
  }

  selected_text_ = new_selected_text;
}

// Just use the default behavior for DnD, except if the drop can be a PasteAndGo
// then override.
void AutocompleteEditViewGtk::HandleDragDataReceived(
    GdkDragContext* context, gint x, gint y,
    GtkSelectionData* selection_data, guint target_type, guint time) {
  // Reset |paste_clipboard_requested_| to make sure we won't misinterpret this
  // drop action as a paste action.
  paste_clipboard_requested_ = false;

  // Don't try to PasteAndGo on drops originating from this omnibox. However, do
  // allow default behavior for such drags.
  if (context->source_window == text_view_->window)
    return;

  guchar* text = gtk_selection_data_get_text(selection_data);
  if (!text)
    return;

  std::wstring possible_url = UTF8ToWide(reinterpret_cast<char*>(text));
  g_free(text);
  if (model_->CanPasteAndGo(CollapseWhitespace(possible_url, true))) {
    model_->PasteAndGo();
    gtk_drag_finish(context, TRUE, TRUE, time);

    static guint signal_id =
        g_signal_lookup("drag-data-received", GTK_TYPE_WIDGET);
    g_signal_stop_emission(text_view_, signal_id, 0);
  }
}

void AutocompleteEditViewGtk::HandleInsertText(
    GtkTextBuffer* buffer, GtkTextIter* location, const gchar* text, gint len) {
  std::string filtered_text;
  filtered_text.reserve(len);

  // Filter out new line and tab characters.
  // |text| is guaranteed to be a valid UTF-8 string, so it's safe here to
  // filter byte by byte.
  //
  // If there was only a single character, then it might be generated by a key
  // event. In this case, we save the single character to help our
  // "key-press-event" signal handler distinguish if an Enter key event is
  // handled by IME or not.
  if (len == 1)
    char_inserted_ = text[0];

  for (gint i = 0; i < len; ++i) {
    gchar c = text[i];
    if (c == '\n' || c == '\r' || c == '\t')
      continue;

    filtered_text.push_back(c);
  }

  if (filtered_text.length()) {
    // Call the default handler to insert filtered text.
    GtkTextBufferClass* klass = GTK_TEXT_BUFFER_GET_CLASS(buffer);
    klass->insert_text(buffer, location, filtered_text.data(),
                       static_cast<gint>(filtered_text.length()));
  }

  // Stop propagating the signal emission to prevent the default handler from
  // being called again.
  static guint signal_id = g_signal_lookup("insert-text", GTK_TYPE_TEXT_BUFFER);
  g_signal_stop_emission(buffer, signal_id, 0);
}

void AutocompleteEditViewGtk::HandleBackSpace() {
  // Checks if it's currently in keyword search mode.
  if (model_->is_keyword_hint() || model_->keyword().empty())
    return;  // Propgate into GtkTextView.

  GtkTextIter sel_start, sel_end;
  // Checks if there is some text selected.
  if (gtk_text_buffer_get_selection_bounds(text_buffer_, &sel_start, &sel_end))
    return;  // Propgate into GtkTextView.

  GtkTextIter start;
  gtk_text_buffer_get_start_iter(text_buffer_, &start);

  if (!gtk_text_iter_equal(&start, &sel_start))
    return;  // Propgate into GtkTextView.

  // We're showing a keyword and the user pressed backspace at the beginning
  // of the text. Delete the selected keyword.
  model_->ClearKeyword(GetText());

  // Stop propagating the signal emission into GtkTextView.
  static guint signal_id = g_signal_lookup("backspace", GTK_TYPE_TEXT_VIEW);
  g_signal_stop_emission(text_view_, signal_id, 0);
}

void AutocompleteEditViewGtk::HandleViewMoveFocus(GtkWidget* widget) {
  // Trigger Tab to search behavior only when Tab key is pressed.
  if (tab_was_pressed_ && model_->is_keyword_hint() &&
      !model_->keyword().empty()) {
    model_->AcceptKeyword();

    // If Tab to search behavior is triggered, then stop the signal emission to
    // prevent the focus from being moved.
    static guint signal_id = g_signal_lookup("move-focus", GTK_TYPE_WIDGET);
    g_signal_stop_emission(widget, signal_id, 0);
  }

  // Propagate the signal so that focus can be moved as normal.
}

void AutocompleteEditViewGtk::HandleCopyClipboard() {
  // On copy, we manually update the PRIMARY selection to contain the
  // highlighted text.  This matches Firefox -- we highlight the URL but don't
  // update PRIMARY on Ctrl-L, so Ctrl-L, Ctrl-C and then middle-click is a
  // convenient way to paste the current URL somewhere.
  if (!gtk_text_buffer_get_has_selection(text_buffer_))
    return;

  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  DCHECK(clipboard);
  if (!clipboard)
    return;

  // Passing gtk_text_buffer_copy_clipboard() a text buffer that already owns
  // the clipboard that's being updated clears the highlighted text, which we
  // don't want to do (and it also appears to at least sometimes trigger a
  // failed G_IS_OBJECT() assertion).
  if (gtk_clipboard_get_owner(clipboard) == G_OBJECT(text_buffer_))
    return;

  // We can't just call SavePrimarySelection(); that makes the text view lose
  // the selection and unhighlight its text.
  gtk_text_buffer_copy_clipboard(text_buffer_, clipboard);
}

void AutocompleteEditViewGtk::HandlePasteClipboard() {
  // We can't call model_->on_paste_replacing_all() here, because the actual
  // paste clipboard action may not be performed if the clipboard is empty.
  paste_clipboard_requested_ = true;
}

void AutocompleteEditViewGtk::SelectAllInternal(bool reversed,
                                                bool update_primary_selection) {
  GtkTextIter start, end;
  if (reversed) {
    gtk_text_buffer_get_bounds(text_buffer_, &end, &start);
  } else {
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  }
  if (!update_primary_selection)
    StartUpdatingHighlightedText();
  gtk_text_buffer_select_range(text_buffer_, &start, &end);
  if (!update_primary_selection)
    FinishUpdatingHighlightedText();
}

void AutocompleteEditViewGtk::StartUpdatingHighlightedText() {
  if (GTK_WIDGET_REALIZED(text_view_)) {
    GtkClipboard* clipboard =
        gtk_widget_get_clipboard(text_view_, GDK_SELECTION_PRIMARY);
    DCHECK(clipboard);
    if (clipboard)
      gtk_text_buffer_remove_selection_clipboard(text_buffer_, clipboard);
  }
  g_signal_handler_block(text_buffer_, mark_set_handler_id_);
}

void AutocompleteEditViewGtk::FinishUpdatingHighlightedText() {
  if (GTK_WIDGET_REALIZED(text_view_)) {
    GtkClipboard* clipboard =
        gtk_widget_get_clipboard(text_view_, GDK_SELECTION_PRIMARY);
    DCHECK(clipboard);
    if (clipboard)
      gtk_text_buffer_add_selection_clipboard(text_buffer_, clipboard);
  }
  g_signal_handler_unblock(text_buffer_, mark_set_handler_id_);
}

AutocompleteEditViewGtk::CharRange AutocompleteEditViewGtk::GetSelection() {
  // You can not just use get_selection_bounds here, since the order will be
  // ascending, and you don't know where the user's start and end of the
  // selection was (if the selection was forwards or backwards).  Get the
  // actual marks so that we can preserve the selection direction.
  GtkTextIter start, insert;
  GtkTextMark* mark;

  mark = gtk_text_buffer_get_selection_bound(text_buffer_);
  gtk_text_buffer_get_iter_at_mark(text_buffer_, &start, mark);

  mark = gtk_text_buffer_get_insert(text_buffer_);
  gtk_text_buffer_get_iter_at_mark(text_buffer_, &insert, mark);

  return CharRange(gtk_text_iter_get_offset(&start),
                   gtk_text_iter_get_offset(&insert));
}

void AutocompleteEditViewGtk::ItersFromCharRange(const CharRange& range,
                                                 GtkTextIter* iter_min,
                                                 GtkTextIter* iter_max) {
  gtk_text_buffer_get_iter_at_offset(text_buffer_, iter_min, range.cp_min);
  gtk_text_buffer_get_iter_at_offset(text_buffer_, iter_max, range.cp_max);
}

int AutocompleteEditViewGtk::GetTextLength() {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  return gtk_text_iter_get_offset(&end);
}

void AutocompleteEditViewGtk::EmphasizeURLComponents() {
  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url_parse::Parsed parts;
  std::wstring text = GetText();
  AutocompleteInput::Parse(text, model_->GetDesiredTLD(), &parts, NULL);
  bool emphasize = model_->CurrentTextIsURL() && (parts.host.len > 0);

  // Set the baseline emphasis.
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gtk_text_buffer_remove_all_tags(text_buffer_, &start, &end);
  if (emphasize) {
    gtk_text_buffer_apply_tag(text_buffer_, faded_text_tag_, &start, &end);

    // We've found a host name, give it more emphasis.
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &start, 0,
                                           GetUTF8Offset(text,
                                                         parts.host.begin));
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &end, 0,
                                           GetUTF8Offset(text,
                                                         parts.host.end()));

    gtk_text_buffer_apply_tag(text_buffer_, normal_text_tag_, &start, &end);
  } else {
    gtk_text_buffer_apply_tag(text_buffer_, normal_text_tag_, &start, &end);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && parts.scheme.is_nonempty() &&
      (scheme_security_level_ != ToolbarModel::NORMAL)) {
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &start, 0,
                                           GetUTF8Offset(text,
                                                         parts.scheme.begin));
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &end, 0,
                                           GetUTF8Offset(text,
                                                         parts.scheme.end()));
    if (scheme_security_level_ == ToolbarModel::SECURE) {
      gtk_text_buffer_apply_tag(text_buffer_, secure_scheme_tag_,
                                &start, &end);
    } else {
      gtk_text_buffer_apply_tag(text_buffer_, insecure_scheme_tag_,
                                &start, &end);
    }
  }
}

void AutocompleteEditViewGtk::TextChanged() {
  EmphasizeURLComponents();
  controller_->OnChanged();
}

void AutocompleteEditViewGtk::SavePrimarySelection(
    const std::string& selected_text) {
  GtkClipboard* clipboard =
      gtk_widget_get_clipboard(text_view_, GDK_SELECTION_PRIMARY);
  DCHECK(clipboard);
  if (!clipboard)
    return;

  gtk_clipboard_set_text(
      clipboard, selected_text.data(), selected_text.size());
}
