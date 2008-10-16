// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/text_field.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <tom.h>  // For ITextDocument, a COM interface to CRichEditCtrl
#include <vsstyle.h>

#include "base/gfx/native_theme.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/gfx/insets.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/win_util.h"
#include "chrome/views/container.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/menu.h"

#include "generated_resources.h"

using gfx::NativeTheme;

namespace views {

static const int kDefaultEditStyle = WS_CHILD | WS_VISIBLE;

class TextField::Edit
    : public CWindowImpl<TextField::Edit, CRichEditCtrl,
                         CWinTraits<kDefaultEditStyle> >,
      public CRichEditCommands<TextField::Edit>,
      public Menu::Delegate {
 public:
  DECLARE_WND_CLASS(L"ChromeViewsTextFieldEdit");

  Edit(TextField* parent, bool draw_border);
  ~Edit();

  std::wstring GetText() const;
  void SetText(const std::wstring& text);

  std::wstring GetSelectedText() const;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  void SelectAll();

  // Clears the selection within the edit field and sets the caret to the end.
  void ClearSelection();

  // Removes the border.
  void RemoveBorder();

  void SetEnabled(bool enabled);

  // CWindowImpl
  BEGIN_MSG_MAP(Edit)
    MSG_WM_CHAR(OnChar)
    MSG_WM_CONTEXTMENU(OnContextMenu)
    MSG_WM_COPY(OnCopy)
    MSG_WM_CUT(OnCut)
    MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeComposition)
    MSG_WM_KEYDOWN(OnKeyDown)
    MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONDOWN(OnNonLButtonDown)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_MOUSELEAVE(OnMouseLeave)
    MSG_WM_NCCALCSIZE(OnNCCalcSize)
    MSG_WM_NCPAINT(OnNCPaint)
    MSG_WM_RBUTTONDOWN(OnNonLButtonDown)
    MSG_WM_PASTE(OnPaste)
    MSG_WM_SYSCHAR(OnSysChar)  // WM_SYSxxx == WM_xxx with ALT down
    MSG_WM_SYSKEYDOWN(OnKeyDown)
  END_MSG_MAP()

  // Menu::Delegate
  virtual bool IsCommandEnabled(int id) const;
  virtual void ExecuteCommand(int id);

 private:
  // This object freezes repainting of the edit until the object is destroyed.
  // Some methods of the CRichEditCtrl draw synchronously to the screen.  If we
  // don't freeze, the user will see a rapid series of calls to these as
  // flickers.
  //
  // Freezing the control while it is already frozen is permitted; the control
  // will unfreeze once both freezes are released (the freezes stack).
  class ScopedFreeze {
   public:
    ScopedFreeze(Edit* edit, ITextDocument* text_object_model);
    ~ScopedFreeze();

   private:
    Edit* const edit_;
    ITextDocument* const text_object_model_;

    DISALLOW_EVIL_CONSTRUCTORS(ScopedFreeze);
  };

  // message handlers
  void OnChar(TCHAR key, UINT repeat_count, UINT flags);
  void OnContextMenu(HWND window, const CPoint& point);
  void OnCopy();
  void OnCut();
  LRESULT OnImeComposition(UINT message, WPARAM wparam, LPARAM lparam);
  void OnKeyDown(TCHAR key, UINT repeat_count, UINT flags);
  void OnLButtonDblClk(UINT keys, const CPoint& point);
  void OnLButtonDown(UINT keys, const CPoint& point);
  void OnLButtonUp(UINT keys, const CPoint& point);
  void OnMouseLeave();
  void OnMouseMove(UINT keys, const CPoint& point);
  int OnNCCalcSize(BOOL w_param, LPARAM l_param);
  void OnNCPaint(HRGN region);
  void OnNonLButtonDown(UINT keys, const CPoint& point);
  void OnPaste();
  void OnSysChar(TCHAR ch, UINT repeat_count, UINT flags);

  // Helper function for OnChar() and OnKeyDown() that handles keystrokes that
  // could change the text in the edit.
  void HandleKeystroke(UINT message, TCHAR key, UINT repeat_count, UINT flags);

  // Every piece of code that can change the edit should call these functions
  // before and after the change.  These functions determine if anything
  // meaningful changed, and do any necessary updating and notification.
  void OnBeforePossibleChange();
  void OnAfterPossibleChange();

  // Given an X coordinate in client coordinates, returns that coordinate
  // clipped to be within the horizontal bounds of the visible text.
  //
  // This is used in our mouse handlers to work around quirky behaviors of the
  // underlying CRichEditCtrl like not supporting triple-click when the user
  // doesn't click on the text itself.
  //
  // |is_triple_click| should be true iff this is the third click of a triple
  // click.  Sadly, we need to clip slightly differently in this case.
  LONG ClipXCoordToVisibleText(LONG x, bool is_triple_click) const;

  // Sets whether the mouse is in the edit. As necessary this redraws the
  // edit.
  void SetContainsMouse(bool contains_mouse);

  // Getter for the text_object_model_, used by the ScopedFreeze class.  Note
  // that the pointer returned here is only valid as long as the Edit is still
  // alive.
  ITextDocument* GetTextObjectModel() const;

  // We need to know if the user triple-clicks, so track double click points
  // and times so we can see if subsequent clicks are actually triple clicks.
  bool tracking_double_click_;
  CPoint double_click_point_;
  DWORD double_click_time_;

  // Used to discard unnecessary WM_MOUSEMOVE events after the first such
  // unnecessary event.  See detailed comments in OnMouseMove().
  bool can_discard_mousemove_;

  // The text of this control before a possible change.
  std::wstring text_before_change_;

  // If true, the mouse is over the edit.
  bool contains_mouse_;

  static bool did_load_library_;

  TextField* parent_;

  // The context menu for the edit.
  scoped_ptr<Menu> context_menu_;

  // Border insets.
  gfx::Insets content_insets_;

  // Whether the border is drawn.
  bool draw_border_;

  // This interface is useful for accessing the CRichEditCtrl at a low level.
  mutable CComQIPtr<ITextDocument> text_object_model_;

  DISALLOW_EVIL_CONSTRUCTORS(Edit);
};

///////////////////////////////////////////////////////////////////////////////
// Helper classes

TextField::Edit::ScopedFreeze::ScopedFreeze(TextField::Edit* edit,
                                            ITextDocument* text_object_model)
    : edit_(edit),
      text_object_model_(text_object_model) {
  // Freeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Freeze(&count);
  }
}

TextField::Edit::ScopedFreeze::~ScopedFreeze() {
  // Unfreeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Unfreeze(&count);
    if (count == 0) {
      // We need to UpdateWindow() here instead of InvalidateRect() because, as
      // far as I can tell, the edit likes to synchronously erase its background
      // when unfreezing, thus requiring us to synchronously redraw if we don't
      // want flicker.
      edit_->UpdateWindow();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// TextField::Edit

bool TextField::Edit::did_load_library_ = false;

TextField::Edit::Edit(TextField* parent, bool draw_border)
    : parent_(parent),
      tracking_double_click_(false),
      double_click_time_(0),
      can_discard_mousemove_(false),
      contains_mouse_(false),
      draw_border_(draw_border) {
  if (!did_load_library_)
    did_load_library_ = !!LoadLibrary(L"riched20.dll");

  DWORD style = kDefaultEditStyle;
  if (parent->GetStyle() & TextField::STYLE_PASSWORD)
    style |= ES_PASSWORD;

  if (parent->read_only_)
    style |= ES_READONLY;

  if (parent->GetStyle() & TextField::STYLE_MULTILINE)
    style |= ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL;
  else
    style |= ES_AUTOHSCROLL;
  // Make sure we apply RTL related extended window styles if necessary.
  DWORD ex_style = l10n_util::GetExtendedStyles();

  RECT r = {0, 0, parent_->width(), parent_->height()};
  Create(parent_->GetContainer()->GetHWND(), r, NULL, style, ex_style);

  // Set up the text_object_model_.
  CComPtr<IRichEditOle> ole_interface;
  ole_interface.Attach(GetOleInterface());
  text_object_model_ = ole_interface;

  context_menu_.reset(new Menu(this, Menu::TOPLEFT, m_hWnd));
  context_menu_->AppendMenuItemWithLabel(IDS_UNDO,
                                         l10n_util::GetString(IDS_UNDO));
  context_menu_->AppendSeparator();
  context_menu_->AppendMenuItemWithLabel(IDS_CUT,
                                         l10n_util::GetString(IDS_CUT));
  context_menu_->AppendMenuItemWithLabel(IDS_COPY,
                                         l10n_util::GetString(IDS_COPY));
  context_menu_->AppendMenuItemWithLabel(IDS_PASTE,
                                         l10n_util::GetString(IDS_PASTE));
  context_menu_->AppendSeparator();
  context_menu_->AppendMenuItemWithLabel(IDS_SELECTALL,
                                         l10n_util::GetString(IDS_SELECTALL));
}

TextField::Edit::~Edit() {
}

std::wstring TextField::Edit::GetText() const {
  int len = GetTextLength() + 1;
  std::wstring str;
  GetWindowText(WriteInto(&str, len), len);
  return str;
}

void TextField::Edit::SetText(const std::wstring& text) {
  // Adjusting the string direction before setting the text in order to make
  // sure both RTL and LTR strings are displayed properly.
  std::wstring localized_text;
  if (l10n_util::AdjustStringForLocaleDirection(text, &localized_text))
    SetWindowText(localized_text.c_str());
  else
    SetWindowText(text.c_str());
}

std::wstring TextField::Edit::GetSelectedText() const {
  // Figure out the length of the selection.
  long start;
  long end;
  GetSel(start, end);

  // Grab the selected text.
  std::wstring str;
  GetSelText(WriteInto(&str, end - start + 1));

  return str;
}

void TextField::Edit::SelectAll() {
  // Using (0, -1) here is equivalent to calling SetSelAll(); both will select
  // the "phantom newline" that we're trying to avoid.
  SetSel(0, GetTextLength());
}

void TextField::Edit::ClearSelection() {
  SetSel(GetTextLength(), GetTextLength());
}

void TextField::Edit::RemoveBorder() {
  if (!draw_border_)
    return;

  draw_border_ = false;
  SetWindowPos(NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOACTIVATE |
               SWP_NOOWNERZORDER | SWP_NOSIZE);
}

void TextField::Edit::SetEnabled(bool enabled) {
  SendMessage(parent_->GetNativeComponent(), WM_ENABLE,
              static_cast<WPARAM>(enabled), 0);
}

bool TextField::Edit::IsCommandEnabled(int id) const {
  switch (id) {
    case IDS_UNDO:      return !parent_->IsReadOnly() && !!CanUndo();
    case IDS_CUT:       return !parent_->IsReadOnly() && !!CanCut();
    case IDS_COPY:      return !!CanCopy();
    case IDS_PASTE:     return !parent_->IsReadOnly() && !!CanPaste();
    case IDS_SELECTALL: return !!CanSelectAll();
    default:            NOTREACHED();
                        return false;
  }
}

void TextField::Edit::ExecuteCommand(int id) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  switch (id) {
    case IDS_UNDO:      Undo();       break;
    case IDS_CUT:       Cut();        break;
    case IDS_COPY:      Copy();       break;
    case IDS_PASTE:     Paste();      break;
    case IDS_SELECTALL: SelectAll();  break;
    default:            NOTREACHED(); break;
  }
  OnAfterPossibleChange();
}

void TextField::Edit::OnChar(TCHAR ch, UINT repeat_count, UINT flags) {
  HandleKeystroke(GetCurrentMessage()->message, ch, repeat_count, flags);
}

void TextField::Edit::OnContextMenu(HWND window, const CPoint& point) {
  CPoint p(point);
  if (point.x == -1 || point.y == -1) {
    GetCaretPos(&p);
    MapWindowPoints(HWND_DESKTOP, &p, 1);
  }
  context_menu_->RunMenuAt(p.x, p.y);
}

void TextField::Edit::OnCopy() {
  const std::wstring text(GetSelectedText());

  if (!text.empty()) {
    ClipboardService* clipboard = g_browser_process->clipboard_service();

    clipboard->Clear();
    clipboard->WriteText(text);
  }
}

void TextField::Edit::OnCut() {
  if (parent_->IsReadOnly())
    return;

  OnCopy();

  // This replace selection will have no effect (even on the undo stack) if the
  // current selection is empty.
  ReplaceSel(L"", true);
}

LRESULT TextField::Edit::OnImeComposition(UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  OnBeforePossibleChange();
  LRESULT result = DefWindowProc(message, wparam, lparam);
  OnAfterPossibleChange();
  return result;
}

void TextField::Edit::OnKeyDown(TCHAR key, UINT repeat_count, UINT flags) {
  // NOTE: Annoyingly, ctrl-alt-<key> generates WM_KEYDOWN rather than
  // WM_SYSKEYDOWN, so we need to check (flags & KF_ALTDOWN) in various places
  // in this function even with a WM_SYSKEYDOWN handler.

  switch (key) {
    case VK_RETURN:
      // If we are multi-line, we want to let returns through so they start a
      // new line.
      if (parent_->IsMultiLine())
        break;
      else
        return;
    // Hijacking Editing Commands
    //
    // We hijack the keyboard short-cuts for Cut, Copy, and Paste here so that
    // they go through our clipboard routines.  This allows us to be smarter
    // about how we interact with the clipboard and avoid bugs in the
    // CRichEditCtrl.  If we didn't hijack here, the edit control would handle
    // these internally with sending the WM_CUT, WM_COPY, or WM_PASTE messages.
    //
    // Cut:   Shift-Delete and Ctrl-x are treated as cut.  Ctrl-Shift-Delete and
    //        Ctrl-Shift-x are not treated as cut even though the underlying
    //        CRichTextEdit would treat them as such.
    // Copy:  Ctrl-v is treated as copy.  Shift-Ctrl-v is not.
    // Paste: Shift-Insert and Ctrl-v are tread as paste.  Ctrl-Shift-Insert and
    //        Ctrl-Shift-v are not.
    //
    // This behavior matches most, but not all Windows programs, and largely
    // conforms to what users expect.

    case VK_DELETE:
    case 'X':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'X') ? VK_CONTROL : VK_SHIFT) >= 0))
        break;
      if (GetKeyState((key == 'X') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Cut();
        OnAfterPossibleChange();
      }
      return;

    case 'C':
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        break;
      if (GetKeyState(VK_SHIFT) >= 0)
        Copy();
      return;

    case VK_INSERT:
    case 'V':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'V') ? VK_CONTROL : VK_SHIFT) >= 0))
        break;
      if (GetKeyState((key == 'V') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Paste();
        OnAfterPossibleChange();
      }
      return;

    case 0xbb:  // Ctrl-'='.  Triggers subscripting, even in plain text mode.
      return;
  }

  // CRichEditCtrl changes its text on WM_KEYDOWN instead of WM_CHAR for many
  // different keys (backspace, ctrl-v, ...), so we call this in both cases.
  HandleKeystroke(GetCurrentMessage()->message, key, repeat_count, flags);
}

void TextField::Edit::OnLButtonDblClk(UINT keys, const CPoint& point) {
  // Save the double click info for later triple-click detection.
  tracking_double_click_ = true;
  double_click_point_ = point;
  double_click_time_ = GetCurrentMessage()->time;

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDBLCLK, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange();
}

void TextField::Edit::OnLButtonDown(UINT keys, const CPoint& point) {
  // Check for triple click, then reset tracker.  Should be safe to subtract
  // double_click_time_ from the current message's time even if the timer has
  // wrapped in between.
  const bool is_triple_click = tracking_double_click_ &&
      win_util::IsDoubleClick(double_click_point_, point,
                              GetCurrentMessage()->time - double_click_time_);
  tracking_double_click_ = false;

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDOWN, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, is_triple_click),
                           point.y));
  OnAfterPossibleChange();
}

void TextField::Edit::OnLButtonUp(UINT keys, const CPoint& point) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONUP, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange();
}

void TextField::Edit::OnMouseLeave() {
  SetContainsMouse(false);
}

void TextField::Edit::OnMouseMove(UINT keys, const CPoint& point) {
  SetContainsMouse(true);
  // Clamp the selection to the visible text so the user can't drag to select
  // the "phantom newline".  In theory we could achieve this by clipping the X
  // coordinate, but in practice the edit seems to behave nondeterministically
  // with similar sequences of clipped input coordinates fed to it.  Maybe it's
  // reading the mouse cursor position directly?
  //
  // This solution has a minor visual flaw, however: if there's a visible
  // cursor at the edge of the text (only true when there's no selection),
  // dragging the mouse around outside that edge repaints the cursor on every
  // WM_MOUSEMOVE instead of allowing it to blink normally.  To fix this, we
  // special-case this exact case and discard the WM_MOUSEMOVE messages instead
  // of passing them along.
  //
  // But even this solution has a flaw!  (Argh.)  In the case where the user
  // has a selection that starts at the edge of the edit, and proceeds to the
  // middle of the edit, and the user is dragging back past the start edge to
  // remove the selection, there's a redraw problem where the change between
  // having the last few bits of text still selected and having nothing
  // selected can be slow to repaint (which feels noticeably strange).  This
  // occurs if you only let the edit receive a single WM_MOUSEMOVE past the
  // edge of the text.  I think on each WM_MOUSEMOVE the edit is repainting its
  // previous state, then updating its internal variables to the new state but
  // not repainting.  To fix this, we allow one more WM_MOUSEMOVE through after
  // the selection has supposedly been shrunk to nothing; this makes the edit
  // redraw the selection quickly so it feels smooth.
  CHARRANGE selection;
  GetSel(selection);
  const bool possibly_can_discard_mousemove =
      (selection.cpMin == selection.cpMax) &&
      (((selection.cpMin == 0) &&
        (ClipXCoordToVisibleText(point.x, false) > point.x)) ||
       ((selection.cpMin == GetTextLength()) &&
        (ClipXCoordToVisibleText(point.x, false) < point.x)));
  if (!can_discard_mousemove_ || !possibly_can_discard_mousemove) {
    can_discard_mousemove_ = possibly_can_discard_mousemove;
    ScopedFreeze freeze(this, GetTextObjectModel());
    OnBeforePossibleChange();
    // Force the Y coordinate to the center of the clip rect.  The edit
    // behaves strangely when the cursor is dragged vertically: if the cursor
    // is in the middle of the text, drags inside the clip rect do nothing,
    // and drags outside the clip rect act as if the cursor jumped to the
    // left edge of the text.  When the cursor is at the right edge, drags of
    // just a few pixels vertically end up selecting the "phantom newline"...
    // sometimes.
    RECT r;
    GetRect(&r);
    DefWindowProc(WM_MOUSEMOVE, keys,
                  MAKELPARAM(point.x, (r.bottom - r.top) / 2));
    OnAfterPossibleChange();
  }
}

int TextField::Edit::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  content_insets_.Set(0, 0, 0, 0);
  parent_->CalculateInsets(&content_insets_);
  if (w_param) {
    NCCALCSIZE_PARAMS* nc_params =
        reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);
    nc_params->rgrc[0].left += content_insets_.left();
    nc_params->rgrc[0].right -= content_insets_.right();
    nc_params->rgrc[0].top += content_insets_.top();
    nc_params->rgrc[0].bottom -= content_insets_.bottom();
  } else {
    RECT* rect = reinterpret_cast<RECT*>(l_param);
    rect->left += content_insets_.left();
    rect->right -= content_insets_.right();
    rect->top += content_insets_.top();
    rect->bottom -= content_insets_.bottom();
  }
  return 0;
}

void TextField::Edit::OnNCPaint(HRGN region) {
  if (!draw_border_)
    return;

  HDC hdc = GetWindowDC();

  CRect window_rect;
  GetWindowRect(&window_rect);
  // Convert to be relative to 0x0.
  window_rect.MoveToXY(0, 0);

  ExcludeClipRect(hdc,
                  window_rect.left + content_insets_.left(),
                  window_rect.top + content_insets_.top(),
                  window_rect.right - content_insets_.right(),
                  window_rect.bottom - content_insets_.bottom());

  FillRect(hdc, &window_rect, (HBRUSH) (COLOR_WINDOW+1));

  int part;
  int state;

  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    part = EP_EDITTEXT;

    if (!parent_->IsEnabled())
      state = ETS_DISABLED;
    else if (parent_->IsReadOnly())
      state = ETS_READONLY;
    else if (!contains_mouse_)
      state = ETS_NORMAL;
    else
      state = ETS_HOT;
  } else {
    part = EP_EDITBORDER_HVSCROLL;

    if (!parent_->IsEnabled())
      state = EPSHV_DISABLED;
    else if (GetFocus() == m_hWnd)
      state = EPSHV_FOCUSED;
    else if (contains_mouse_)
      state = EPSHV_HOT;
    else
      state = EPSHV_NORMAL;
    // Vista doesn't appear to have a unique state for readonly.
  }

  int classic_state =
      (!parent_->IsEnabled() || parent_->IsReadOnly()) ? DFCS_INACTIVE : 0;

  NativeTheme::instance()->PaintTextField(hdc, part, state, classic_state,
                                          &window_rect, NULL, false, true);

  // NOTE: I tried checking the transparent property of the theme and invoking
  // drawParentBackground, but it didn't seem to make a difference.

  ReleaseDC(hdc);
}

void TextField::Edit::OnNonLButtonDown(UINT keys, const CPoint& point) {
  // Interestingly, the edit doesn't seem to cancel triple clicking when the
  // x-buttons (which usually means "thumb buttons") are pressed, so we only
  // call this for M and R down.
  tracking_double_click_ = false;
  SetMsgHandled(false);
}

void TextField::Edit::OnPaste() {
  if (parent_->IsReadOnly())
    return;

  ClipboardService* clipboard = g_browser_process->clipboard_service();

  if (!clipboard->IsFormatAvailable(CF_UNICODETEXT))
    return;

  std::wstring clipboard_str;
  clipboard->ReadText(&clipboard_str);
  if (!clipboard_str.empty()) {
    const std::wstring collapsed(CollapseWhitespace(clipboard_str, false));
    ReplaceSel(collapsed.c_str(), true);
  }
}

void TextField::Edit::OnSysChar(TCHAR ch, UINT repeat_count, UINT flags) {
  // Nearly all alt-<xxx> combos result in beeping rather than doing something
  // useful, so we discard most.  Exceptions:
  //   * ctrl-alt-<xxx>, which is sometimes important, generates WM_CHAR instead
  //     of WM_SYSCHAR, so it doesn't need to be handled here.
  //   * alt-space gets translated by the default WM_SYSCHAR handler to a
  //     WM_SYSCOMMAND to open the application context menu, so we need to allow
  //     it through.
  if (ch == VK_SPACE)
    SetMsgHandled(false);
}

void TextField::Edit::HandleKeystroke(UINT message,
                                      TCHAR key,
                                      UINT repeat_count,
                                      UINT flags) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(message, key, MAKELPARAM(repeat_count, flags));
  OnAfterPossibleChange();

  TextField::Controller* controller = parent_->GetController();
  if (controller)
    controller->HandleKeystroke(parent_, message, key, repeat_count, flags);
}

void TextField::Edit::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
}

void TextField::Edit::OnAfterPossibleChange() {
  // Prevent the user from selecting the "phantom newline" at the end of the
  // edit.  If they try, we just silently move the end of the selection back to
  // the end of the real text.
  CHARRANGE new_sel;
  GetSel(new_sel);
  const int length = GetTextLength();
  if (new_sel.cpMax > length) {
    new_sel.cpMax = length;
    if (new_sel.cpMin > length)
      new_sel.cpMin = length;
    SetSel(new_sel);
  }

  const std::wstring new_text(GetText());
  if (new_text != text_before_change_) {
    parent_->SyncText();
    if (parent_->GetController())
      parent_->GetController()->ContentsChanged(parent_, new_text);
  }
}

LONG TextField::Edit::ClipXCoordToVisibleText(LONG x,
                                              bool is_triple_click) const {
  // Clip the X coordinate to the left edge of the text.  Careful:
  // PosFromChar(0) may return a negative X coordinate if the beginning of the
  // text has scrolled off the edit, so don't go past the clip rect's edge.
  RECT r;
  GetRect(&r);
  const int left_bound = std::max(r.left, PosFromChar(0).x);
  if (x < left_bound)
    return left_bound;

  // See if we need to clip to the right edge of the text.
  const int length = GetTextLength();
  // Asking for the coordinate of any character past the end of the text gets
  // the pixel just to the right of the last character.
  const int right_bound = std::min(r.right, PosFromChar(length).x);
  if ((length == 0) || (x < right_bound))
    return x;

  // For trailing characters that are 2 pixels wide of less (like "l" in some
  // fonts), we have a problem:
  //   * Clicks on any pixel within the character will place the cursor before
  //     the character.
  //   * Clicks on the pixel just after the character will not allow triple-
  //     click to work properly (true for any last character width).
  // So, we move to the last pixel of the character when this is a
  // triple-click, and moving to one past the last pixel in all other
  // scenarios.  This way, all clicks that can move the cursor will place it at
  // the end of the text, but triple-click will still work.
  return is_triple_click ? (right_bound - 1) : right_bound;
}

void TextField::Edit::SetContainsMouse(bool contains_mouse) {
  if (contains_mouse == contains_mouse_)
    return;

  contains_mouse_ = contains_mouse;

  if (!draw_border_)
    return;

  if (contains_mouse_) {
    // Register for notification when the mouse leaves. Need to do this so
    // that we can reset contains mouse properly.
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  }
  RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
}

ITextDocument* TextField::Edit::GetTextObjectModel() const {
  if (!text_object_model_) {
    CComPtr<IRichEditOle> ole_interface;
    ole_interface.Attach(GetOleInterface());
    text_object_model_ = ole_interface;
  }
  return text_object_model_;
}

/////////////////////////////////////////////////////////////////////////////
// TextField

TextField::~TextField() {
  if (edit_) {
    // If the edit hwnd still exists, we need to destroy it explicitly.
    if (*edit_)
      edit_->DestroyWindow();
    delete edit_;
  }
}

void TextField::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  Container* vc;

  if (is_add && (vc = GetContainer())) {
    // This notification is called from the AddChildView call below. Ignore it.
    if (native_view_ && !edit_)
      return;

    if (!native_view_) {
      native_view_ = new HWNDView();  // Deleted from our superclass destructor
      AddChildView(native_view_);

      // Maps the focus of the native control to the focus of this view.
      native_view_->SetAssociatedFocusView(this);
    }

    // If edit_ is invalid from a previous use. Reset it.
    if (edit_ && !IsWindow(edit_->m_hWnd)) {
      native_view_->Detach();
      delete edit_;
      edit_ = NULL;
    }

    if (!edit_) {
      edit_ = new Edit(this, draw_border_);
      edit_->SetFont(font_.hfont());
      native_view_->Attach(*edit_);
      if (!text_.empty())
        edit_->SetText(text_);
      if (!use_default_background_color_)
        SetBackgroundColor(background_color_);
      Layout();
    }
  } else if (!is_add && edit_ && IsWindow(edit_->m_hWnd)) {
    edit_->SetParent(NULL);
  }
}

void TextField::Layout() {
  if (native_view_) {
    native_view_->SetBounds(GetLocalBounds(true));
    native_view_->UpdateHWNDBounds();
  }
}

gfx::Size TextField::GetPreferredSize() {
  gfx::Insets insets;
  CalculateInsets(&insets);
  return gfx::Size(default_width_in_chars_ * font_.ave_char_width() +
                       insets.width(),
                   num_lines_ * font_.height() + insets.height());
}

std::wstring TextField::GetText() const {
  return text_;
}

void TextField::SetText(const std::wstring& text) {
  text_ = text;
  if (edit_)
    edit_->SetText(text);
}

void TextField::CalculateInsets(gfx::Insets* insets) {
  DCHECK(insets);

  if (!draw_border_)
    return;

  // NOTE: One would think GetThemeMargins would return the insets we should
  // use, but it doesn't. The margins returned by GetThemeMargins are always
  // 0.

  // This appears to be the insets used by Windows.
  insets->Set(3, 3, 3, 3);
}

void TextField::SyncText() {
  if (edit_)
    text_ = edit_->GetText();
}

void TextField::SetController(Controller* controller) {
  controller_ = controller;
}

TextField::Controller* TextField::GetController() const {
  return controller_;
}

bool TextField::IsReadOnly() const {
  return edit_ ? ((edit_->GetStyle() & ES_READONLY) != 0) : read_only_;
}

bool TextField::IsMultiLine() const {
  return (style_ & STYLE_MULTILINE) != 0;
}

void TextField::SetReadOnly(bool read_only) {
  if (edit_)
    edit_->SetReadOnly(read_only);
  else
    read_only_ = read_only;
}


void TextField::Focus() {
  ::SetFocus(native_view_->GetHWND());
}

void TextField::SelectAll() {
  if (edit_)
    edit_->SelectAll();
}

void TextField::ClearSelection() const {
  if (edit_)
    edit_->ClearSelection();
}

HWND TextField::GetNativeComponent() {
  return native_view_->GetHWND();
}

void TextField::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  use_default_background_color_ = false;
  if (edit_) {
    edit_->SetBackgroundColor(RGB(SkColorGetR(color),
                                  SkColorGetG(color),
                                  SkColorGetB(color)));
  }
}

void TextField::SetDefaultBackgroundColor() {
  use_default_background_color_ = true;
  if (edit_)
    edit_->SetBackgroundColor();
}

void TextField::SetFont(const ChromeFont& font) {
  font_ = font;
  if (edit_)
    edit_->SetFont(font.hfont());
}

ChromeFont TextField::GetFont() const {
  return font_;
}

bool TextField::SetHorizontalMargins(int left, int right) {
  // SendMessage expects the two values to be packed into one using MAKELONG
  // so we truncate to 16 bits if necessary.
  return ERROR_SUCCESS == SendMessage(GetNativeComponent(),
                                      (UINT) EM_SETMARGINS,
                                      (WPARAM) EC_LEFTMARGIN | EC_RIGHTMARGIN,
                                      (LPARAM) MAKELONG(left  & 0xFFFF,
                                                        right & 0xFFFF));
}

void TextField::SetHeightInLines(int num_lines) {
  DCHECK(IsMultiLine());
  num_lines_ = num_lines;
}

void TextField::RemoveBorder() {
  if (!draw_border_)
    return;

  draw_border_ = false;
  if (edit_)
    edit_->RemoveBorder();
}

void TextField::SetEnabled(bool enabled) {
  View::SetEnabled(enabled);
  SetReadOnly(enabled);
  edit_->SetEnabled(enabled);
}

bool TextField::IsFocusable() const {
  return IsEnabled() && !IsReadOnly();
}

void TextField::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll();
}

// We don't translate accelerators for ALT + numpad digit, they are used for
// entering special characters.
bool TextField::ShouldLookupAccelerators(const KeyEvent& e) {
  if (!e.IsAltDown())
    return true;

  return !win_util::IsNumPadDigit(e.GetCharacter(), e.IsExtendedKey());
}

}  // namespace views

