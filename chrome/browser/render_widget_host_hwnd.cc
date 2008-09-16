// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_widget_host_hwnd.h"

#include "base/command_line.h"
#include "base/gfx/bitmap_header.h"
#include "base/gfx/rect.h"
#include "base/histogram.h"
#include "base/win_util.h"
#include "chrome/browser/render_process_host.h"
// TODO(beng): (Cleanup) we should not need to include this file... see comment
//             in |DidBecomeSelected|.
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/win_util.h"
#include "chrome/views/hwnd_view_container.h"
#include "webkit/glue/webcursor.h"

// Tooltips will wrap after this width. Yes, wrap. Imagine that!
static const int kTooltipMaxWidthPixels = 300;

// Maximum number of characters we allow in a tooltip.
static const int kMaxTooltipLength = 1024;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostHWND, public:

RenderWidgetHostHWND::RenderWidgetHostHWND(
    RenderWidgetHost* render_widget_host)
    : RenderWidgetHostView(),
      render_widget_host_(render_widget_host),
      real_cursor_(LoadCursor(NULL, IDC_ARROW)),
      real_cursor_type_(WebCursor::ARROW),
      track_mouse_leave_(false),
      ime_notification_(false),
      is_hidden_(false),
      close_on_deactivate_(false),
      tooltip_hwnd_(NULL),
      tooltip_showing_(false),
      shutdown_factory_(this),
      parent_hwnd_(NULL),
      is_loading_(false) {
}

RenderWidgetHostHWND::~RenderWidgetHostHWND() {
  if (real_cursor_type_ == WebCursor::CUSTOM)
    DestroyIcon(real_cursor_);
  ResetTooltip();
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostHWND, RenderWidgetHostView implementation:

void RenderWidgetHostHWND::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  EnsureTooltip();
  render_widget_host_->WasRestored();
}

void RenderWidgetHostHWND::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  ResetTooltip();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  render_widget_host_->WasHidden();

  // TODO(darin): what about constrained windows?  it doesn't look like they
  // see a message when their parent is hidden.  maybe there is something more
  // generic we can do at the TabContents API level instead of relying on
  // Windows messages.
}

void RenderWidgetHostHWND::SetSize(const gfx::Size& size) {
  if (is_hidden_)
    return;

  UINT swp_flags = SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOCOPYBITS |
      SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE |
      SWP_DEFERERASE;
  SetWindowPos(NULL, 0, 0, size.width(), size.height(), swp_flags);
  render_widget_host_->WasResized();
  EnsureTooltip();
}

HWND RenderWidgetHostHWND::GetPluginHWND() {
  return m_hWnd;
}

void RenderWidgetHostHWND::ForwardMouseEventToRenderer(UINT message,
                                                       WPARAM wparam,
                                                       LPARAM lparam) {
  WebMouseEvent event(m_hWnd, message, wparam, lparam);
  switch (event.type) {
    case WebInputEvent::MOUSE_MOVE:
      TrackMouseLeave(true);
      break;
    case WebInputEvent::MOUSE_LEAVE:
      TrackMouseLeave(false);
      break;
    case WebInputEvent::MOUSE_DOWN:
      SetCapture();
      break;
    case WebInputEvent::MOUSE_UP:
      if (GetCapture() == m_hWnd)
        ReleaseCapture();
      break;
  }

  render_widget_host_->ForwardMouseEvent(event);

  if (event.type == WebInputEvent::MOUSE_DOWN) {
    // This is a temporary workaround for bug 765011 to get focus when the
    // mouse is clicked. This happens after the mouse down event is sent to
    // the renderer because normally Windows does a WM_SETFOCUS after
    // WM_LBUTTONDOWN.
    SetFocus();
  }
}

void RenderWidgetHostHWND::Focus() {
  if (IsWindow())
    SetFocus();
}

void RenderWidgetHostHWND::Blur() {
  ChromeViews::FocusManager* focus_manager =
    ChromeViews::FocusManager::GetFocusManager(GetParent());
  // We don't have a FocusManager if we are hidden.
  if (focus_manager && render_widget_host_->CanBlur())
    focus_manager->ClearFocus();
}

bool RenderWidgetHostHWND::HasFocus() {
  return ::GetFocus() == m_hWnd;
}

void RenderWidgetHostHWND::Show() {
  DCHECK(parent_hwnd_);
  SetParent(parent_hwnd_);
  ShowWindow(SW_SHOW);

  DidBecomeSelected();
}

void RenderWidgetHostHWND::Hide() {
  if (::GetFocus() == m_hWnd)
    ::SetFocus(NULL);
  ShowWindow(SW_HIDE);
  parent_hwnd_ = GetParent();
  // Orphan the window so we stop receiving messages.
  SetParent(NULL);

  WasHidden();
}

gfx::Rect RenderWidgetHostHWND::GetViewBounds() const {
  CRect window_rect;
  GetWindowRect(&window_rect);
  return gfx::Rect(window_rect);
}

void RenderWidgetHostHWND::UpdateCursor(const WebCursor& cursor) {
  static HINSTANCE module_handle =
      GetModuleHandle(chrome::kBrowserResourcesDll);

  // If the last active cursor was a custom cursor, we need to destroy
  // it before setting the new one.
  if (real_cursor_type_ == WebCursor::CUSTOM)
    DestroyIcon(real_cursor_);

  real_cursor_type_ = cursor.type();
  if (real_cursor_type_ == cursor.WebCursor::CUSTOM) {
    real_cursor_ = cursor.GetCustomCursor();
  } else {
    // We cannot pass in NULL as the module handle as this would only
    // work for standard win32 cursors. We can also receive cursor
    // types which are defined as webkit resources. We need to specify
    // the module handle of chrome.dll while loading these cursors.
    real_cursor_ = cursor.GetCursor(module_handle);
  }

  UpdateCursorIfOverSelf();
}

void RenderWidgetHostHWND::UpdateCursorIfOverSelf() {
  static HINSTANCE module_handle =
      GetModuleHandle(chrome::kBrowserResourcesDll);

  HCURSOR display_cursor = real_cursor_;
  // If a page is in the loading state, we want to show the Arrow+Hourglass
  // cursor only when the current cursor is the ARROW cursor. In all other
  // cases we should continue to display the current cursor.
  if (is_loading_ && (real_cursor_type_ == WebCursor::ARROW)) {
    WebCursor page_loading_cursor(WebCursor::APPSTARTING);
    display_cursor = page_loading_cursor.GetCursor(module_handle);
  }

  // If the mouse is over our HWND, then update the cursor state immediately.
  CPoint pt;
  GetCursorPos(&pt);
  if (WindowFromPoint(pt) == m_hWnd)
    SetCursor(display_cursor);
}

void RenderWidgetHostHWND::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostHWND::IMEUpdateStatus(ViewHostMsg_ImeControl control,
                                           int x, int y) {
  if (control == IME_DISABLE) {
    ime_input_.DisableIME(m_hWnd);
  } else {
    ime_input_.EnableIME(m_hWnd, x, y, control == IME_COMPLETE_COMPOSITION);
  }
}

void RenderWidgetHostHWND::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  RECT invalid_rect = rect.ToRECT();

  // Paint the invalid region synchronously.  Our caller will not paint again
  // until we return, so by painting to the screen here, we ensure effective
  // rate-limiting of backing store updates.  This helps a lot on pages that
  // have animations or fairly expensive layout (e.g., google maps).
  //
  // Please refer to the RenderWidgetHostHWND::DidScrollRect function for the
  // reasoning behind the combination of flags passed to RedrawWindow.
  //
  RedrawWindow(&invalid_rect, NULL,
      RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
}

void RenderWidgetHostHWND::DidScrollRect(
    const gfx::Rect& rect, int dx, int dy) {
  if (is_hidden_)
    return;

  // We need to pass in SW_INVALIDATE to ScrollWindowEx.  The MSDN
  // documentation states that it only applies to the HRGN argument, which is
  // wrong.  Not passing in this flag does not invalidate the region which was
  // scrolled from, thus causing painting issues.
  RECT clip_rect = rect.ToRECT();
  ScrollWindowEx(dx, dy, NULL, &clip_rect, NULL, NULL, SW_INVALIDATE);

  RECT invalid_rect = {0};
  GetUpdateRect(&invalid_rect);

  // Paint the invalid region synchronously.  Our caller will not paint again
  // until we return, so by painting to the screen here, we ensure effective
  // rate-limiting of backing store updates.  This helps a lot on pages that
  // have animations or fairly expensive layout (e.g., google maps).
  //
  // Our RenderWidgetHostHWND does not have a non-client area, whereas the
  // children (plugin windows) may.  If we don't pass in RDW_FRAME then the
  // children don't receive WM_NCPAINT messages while scrolling, which causes
  // painting problems (http://b/issue?id=923945).  We need to pass
  // RDW_INVALIDATE as it is required for RDW_FRAME to work.
  //
  RedrawWindow(&invalid_rect, NULL,
      RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
}

void RenderWidgetHostHWND::RendererGone() {
  // TODO(darin): keep this around, and draw sad-tab into it.
  UpdateCursorIfOverSelf();
  DestroyWindow();
}

void RenderWidgetHostHWND::Destroy() {
  // We've been told to destroy.
  // By clearing close_on_deactivate_, we prevent further deactivations
  // (caused by windows messages resulting from the DestroyWindow) from
  // triggering further destructions.  The deletion of this is handled by
  // OnFinalMessage();
  close_on_deactivate_ = false;
  DestroyWindow();
}

void RenderWidgetHostHWND::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength so that we don't
    // accidentally DOS the user with a mega tooltip (since Windows doesn't seem
    // to do this itself).
    if (tooltip_text_.length() > kMaxTooltipLength)
      tooltip_text_ = tooltip_text_.substr(0, kMaxTooltipLength);

    // Need to check if the tooltip is already showing so that we don't
    // immediately show the tooltip with no delay when we move the mouse from
    // a region with no tooltip to a region with a tooltip.
    if (::IsWindow(tooltip_hwnd_) && tooltip_showing_) {
      ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
      ::SendMessage(tooltip_hwnd_, TTM_POPUP, 0, 0);
    }
  } else {
    // Make sure the tooltip gets closed after TTN_POP gets sent. For some
    // reason this doesn't happen automatically, so moving the mouse around
    // within the same link/image/etc doesn't cause the tooltip to re-appear.
    if (!tooltip_showing_) {
      if (::IsWindow(tooltip_hwnd_))
        ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostHWND, private:

LRESULT RenderWidgetHostHWND::OnCreate(CREATESTRUCT* create_struct) {
  // Call the WM_INPUTLANGCHANGE message handler to initialize the input locale
  // of a browser process.
  OnInputLangChange(0, 0);
  return 0;
}

void RenderWidgetHostHWND::OnActivate(UINT action, BOOL minimized,
                                      HWND window) {
  // If the container is a popup, clicking elsewhere on screen should close the
  // popup.
  if (close_on_deactivate_ && action == WA_INACTIVE) {
    // Send a windows message so that any derived classes
    // will get a change to override the default handling
    SendMessage(WM_CANCELMODE);
  }
}

void RenderWidgetHostHWND::OnDestroy() {
  ResetTooltip();
  TrackMouseLeave(false);
}

void RenderWidgetHostHWND::OnPaint(HDC dc) {
  DCHECK(render_widget_host_->process()->channel());

  CPaintDC paint_dc(m_hWnd);
  HBRUSH white_brush = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));

  RenderWidgetHost::BackingStore* backing_store =
      render_widget_host_->GetBackingStore();

  if (backing_store) {
    gfx::Rect damaged_rect(paint_dc.m_ps.rcPaint);

    gfx::Rect bitmap_rect(
        0, 0, backing_store->size().width(), backing_store->size().height());

    gfx::Rect paint_rect = bitmap_rect.Intersect(damaged_rect);
    if (!paint_rect.IsEmpty()) {
      BitBlt(paint_dc.m_hDC,
             paint_rect.x(),
             paint_rect.y(),
             paint_rect.width(),
             paint_rect.height(),
             backing_store->dc(),
             paint_rect.x(),
             paint_rect.y(),
             SRCCOPY);
    }

    // Fill the remaining portion of the damaged_rect with white
    if (damaged_rect.right() > bitmap_rect.right()) {
      RECT r;
      r.left = std::max(bitmap_rect.right(), damaged_rect.x());
      r.right = damaged_rect.right();
      r.top = damaged_rect.y();
      r.bottom = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      paint_dc.FillRect(&r, white_brush);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      RECT r;
      r.left = damaged_rect.x();
      r.right = damaged_rect.right();
      r.top = std::max(bitmap_rect.bottom(), damaged_rect.y());
      r.bottom = damaged_rect.bottom();
      paint_dc.FillRect(&r, white_brush);
    }
    if (!whiteout_start_time_.is_null()) {
      TimeDelta whiteout_duration = TimeTicks::Now() - whiteout_start_time_;
      UMA_HISTOGRAM_TIMES(L"MPArch.RWHH_WhiteoutDuration", whiteout_duration);
      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      whiteout_start_time_ = TimeTicks();
    }
  } else {
    paint_dc.FillRect(&paint_dc.m_ps.rcPaint, white_brush);
    if (whiteout_start_time_.is_null())
      whiteout_start_time_ = TimeTicks::Now();
  }
}

void RenderWidgetHostHWND::OnNCPaint(HRGN update_region) {
  // Do nothing.  This suppresses the resize corner that Windows would
  // otherwise draw for us.
}

LRESULT RenderWidgetHostHWND::OnEraseBkgnd(HDC dc) {
  return 1;
}

LRESULT RenderWidgetHostHWND::OnSetCursor(HWND window, UINT hittest_code,
                                          UINT mouse_message_id) {
  UpdateCursorIfOverSelf();
  return 0;
}

void RenderWidgetHostHWND::OnSetFocus(HWND window) {
  render_widget_host_->Focus();
}

void RenderWidgetHostHWND::OnKillFocus(HWND window) {
  render_widget_host_->Blur();
}

void RenderWidgetHostHWND::OnCaptureChanged(HWND window) {
  render_widget_host_->LostCapture();
}

void RenderWidgetHostHWND::OnCancelMode() {
  render_widget_host_->LostCapture();

  if (close_on_deactivate_ && shutdown_factory_.empty()) {
    // Dismiss popups and menus.  We do this asynchronously to avoid changing
    // activation within this callstack, which may interfere with another window
    // being activated.  We can synchronously hide the window, but we need to
    // not change activation while doing so.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
    MessageLoop::current()->PostTask(FROM_HERE,
        shutdown_factory_.NewRunnableMethod(
            &RenderWidgetHostHWND::ShutdownHost));
  }
}

void RenderWidgetHostHWND::OnInputLangChange(DWORD character_set,
                                             HKL input_language_id) {
  // Send the given Locale ID to the ImeInput object and retrieves whether
  // or not the current input context has IMEs.
  // If the current input context has IMEs, a browser process has to send a
  // request to a renderer process that it needs status messages about
  // the focused edit control from the renderer process.
  // On the other hand, if the current input context does not have IMEs, the
  // browser process also has to send a request to the renderer process that
  // it does not need the status messages any longer.
  // To minimize the number of this notification request, we should check if
  // the browser process is actually retrieving the status messages (this
  // state is stored in ime_notification_) and send a request only if the
  // browser process has to update this status, its details are listed below:
  // * If a browser process is not retrieving the status messages,
  //   (i.e. ime_notification_ == false),
  //   send this request only if the input context does have IMEs,
  //   (i.e. ime_status == true);
  //   When it successfully sends the request, toggle its notification status,
  //   (i.e.ime_notification_ = !ime_notification_ = true).
  // * If a browser process is retrieving the status messages
  //   (i.e. ime_notification_ == true),
  //   send this request only if the input context does not have IMEs,
  //   (i.e. ime_status == false).
  //   When it successfully sends the request, toggle its notification status,
  //   (i.e.ime_notification_ = !ime_notification_ = false).
  // To analyze the above actions, we can optimize them into the ones
  // listed below:
  // 1 Sending a request only if ime_status_ != ime_notification_, and;
  // 2 Copying ime_status to ime_notification_ if it sends the request
  //   successfully (because Action 1 shows ime_status = !ime_notification_.)
  bool ime_status = ime_input_.SetInputLanguage();
  if (ime_status != ime_notification_) {
    if (Send(new ViewMsg_ImeSetInputMode(render_widget_host_->routing_id(),
                                         ime_status))) {
      ime_notification_ = ime_status;
    }
  }
}

void RenderWidgetHostHWND::OnThemeChanged() {
  render_widget_host_->SystemThemeChanged();
}

LRESULT RenderWidgetHostHWND::OnNotify(int w_param, NMHDR* header) {
  if (tooltip_hwnd_ == NULL)
    return 0;

  switch (header->code) {
    case TTN_GETDISPINFO: {
      NMTTDISPINFOW* tooltip_info = reinterpret_cast<NMTTDISPINFOW*>(header);
      tooltip_info->szText[0] = L'\0';
      tooltip_info->lpszText = const_cast<wchar_t*>(tooltip_text_.c_str());
      ::SendMessage(
        tooltip_hwnd_, TTM_SETMAXTIPWIDTH, 0, kTooltipMaxWidthPixels);
      SetMsgHandled(TRUE);
      break;
                          }
    case TTN_POP:
      tooltip_showing_ = false;
      SetMsgHandled(TRUE);
      break;
    case TTN_SHOW:
      tooltip_showing_ = true;
      SetMsgHandled(TRUE);
      break;
  }
  return 0;
}

LRESULT RenderWidgetHostHWND::OnImeSetContext(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // We need status messages about the focused input control from a
  // renderer process when:
  //   * the current input context has IMEs, and;
  //   * an application is activated.
  // This seems to tell we should also check if the current input context has
  // IMEs before sending a request, however, this WM_IME_SETCONTEXT is
  // fortunately sent to an application only while the input context has IMEs.
  // Therefore, we just start/stop status messages according to the activation
  // status of this application without checks.
  bool activated = (wparam == TRUE);
  if (Send(new ViewMsg_ImeSetInputMode(
      render_widget_host_->routing_id(), activated))) {
    ime_notification_ = activated;
  }

  if (ime_notification_)
    ime_input_.CreateImeWindow(m_hWnd);

  ime_input_.CleanupComposition(m_hWnd);
  ime_input_.SetImeWindowStyle(m_hWnd, message, wparam, lparam, &handled);
  return 0;
}

LRESULT RenderWidgetHostHWND::OnImeStartComposition(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // Reset the composition status and create IME windows.
  ime_input_.CreateImeWindow(m_hWnd);
  ime_input_.ResetComposition(m_hWnd);
  // We have to prevent WTL from calling ::DefWindowProc() because the function
  // calls ::ImmSetCompositionWindow() and ::ImmSetCandidateWindow() to
  // over-write the position of IME windows.
  handled = TRUE;
  return 0;
}

LRESULT RenderWidgetHostHWND::OnImeComposition(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // At first, update the position of the IME window.
  ime_input_.UpdateImeWindow(m_hWnd);

  // Retrieve the result string and its attributes of the ongoing composition
  // and send it to a renderer process.
  ImeComposition composition;
  if (ime_input_.GetResult(m_hWnd, lparam, &composition)) {
    Send(new ViewMsg_ImeSetComposition(render_widget_host_->routing_id(),
                                       composition.string_type,
                                       composition.cursor_position,
                                       composition.target_start,
                                       composition.target_end,
                                       composition.ime_string));
    ime_input_.ResetComposition(m_hWnd);
    // Fall though and try reading the composition string.
    // Japanese IMEs send a message containing both GCS_RESULTSTR and
    // GCS_COMPSTR, which means an ongoing composition has been finished
    // by the start of another composition.
  }
  // Retrieve the composition string and its attributes of the ongoing
  // composition and send it to a renderer process.
  if (ime_input_.GetComposition(m_hWnd, lparam, &composition)) {
    Send(new ViewMsg_ImeSetComposition(render_widget_host_->routing_id(),
                                       composition.string_type,
                                       composition.cursor_position,
                                       composition.target_start,
                                       composition.target_end,
                                       composition.ime_string));
  }
  // We have to prevent WTL from calling ::DefWindowProc() because we do not
  // want for the IMM (Input Method Manager) to send WM_IME_CHAR messages.
  handled = TRUE;
  return 0;
}

LRESULT RenderWidgetHostHWND::OnImeEndComposition(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  if (ime_input_.is_composing()) {
    // A composition has been ended while there is an ongoing composition,
    // i.e. the ongoing composition has been canceled.
    // We need to reset the composition status both of the ImeInput object and
    // of the renderer process.
    std::wstring empty_string;
    Send(new ViewMsg_ImeSetComposition(render_widget_host_->routing_id(),
                                       0, -1, -1, -1, empty_string));
    ime_input_.ResetComposition(m_hWnd);
  }
  ime_input_.DestroyImeWindow(m_hWnd);
  // Let WTL call ::DefWindowProc() and release its resources.
  handled = FALSE;
  return 0;
}

LRESULT RenderWidgetHostHWND::OnMouseEvent(UINT message, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  handled = TRUE;

  if (::IsWindow(tooltip_hwnd_)) {
    // Forward mouse events through to the tooltip window
    MSG msg;
    msg.hwnd = m_hWnd;
    msg.message = message;
    msg.wParam = wparam;
    msg.lParam = lparam;
    SendMessage(tooltip_hwnd_, TTM_RELAYEVENT, NULL,
                reinterpret_cast<LPARAM>(&msg));
  }

  // TODO(jcampan): I am not sure if we should forward the message to the
  // WebContents first in the case of popups.  If we do, we would need to
  // convert the click from the popup window coordinates to the WebContents'
  // window coordinates. For now we don't forward the message in that case to
  // address bug #907474.
  // Note: GetParent() on popup windows returns the top window and not the
  // parent the window was created with (the parent and the owner of the popup
  // is the first non-child view of the view that was specified to the create
  // call).  So the WebContents window would have to be specified to the
  // RenderViewHostHWND as there is no way to retrieve it from the HWND.
  if (!close_on_deactivate_) {  // Don't forward if the container is a popup.
    switch (message) {
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_MOUSEMOVE:
      case WM_MOUSELEAVE:
      case WM_RBUTTONDOWN: {
        // Give the WebContents first crack at the message. It may want to
        // prevent forwarding to the renderer if some higher level browser
        // functionality is invoked.
        if (SendMessage(GetParent(), message, wparam, lparam) != 0)
          return 1;
      }
    }
  }

  ForwardMouseEventToRenderer(message, wparam, lparam);
  return 0;
}

LRESULT RenderWidgetHostHWND::OnKeyEvent(UINT message, WPARAM wparam,
                                         LPARAM lparam, BOOL& handled) {
  handled = TRUE;

  // If we are a pop-up, forward tab related messages to our parent HWND, so
  // that we are dismissed appropriately and so that the focus advance in our
  // parent.
  // TODO(jcampan): http://b/issue?id=1192881 Could be abstracted in the
  //                FocusManager.
  if (close_on_deactivate_ &&
      (((message == WM_KEYDOWN || message == WM_KEYUP) && (wparam == VK_TAB)) ||
        (message == WM_CHAR && wparam == L'\t'))) {
    DCHECK(parent_hwnd_);
    // First close the pop-up.
    SendMessage(WM_CANCELMODE);
    // Then move the focus by forwarding the tab key to the parent.
    return ::SendMessage(parent_hwnd_, message, wparam, lparam);
  }

  render_widget_host_->ForwardKeyboardEvent(
      WebKeyboardEvent(m_hWnd, message, wparam, lparam));
  return 0;
}

LRESULT RenderWidgetHostHWND::OnWheelEvent(UINT message, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  // Workaround for Thinkpad mousewheel driver. We get mouse wheel/scroll
  // messages even if we are not in the foreground. So here we check if
  // we have any owned popup windows in the foreground and dismiss them.
  if (m_hWnd != GetForegroundWindow()) {
    HWND toplevel_hwnd = ::GetAncestor(m_hWnd, GA_ROOT);
    EnumThreadWindows(
        GetCurrentThreadId(),
        DismissOwnedPopups,
        reinterpret_cast<LPARAM>(toplevel_hwnd));
  }

  // This is a bit of a hack, but will work for now since we don't want to
  // pollute this object with WebContents-specific functionality...
  bool handled_by_webcontents = false;
  if (GetParent()) {
    // Use a special reflected message to break recursion. If we send
    // WM_MOUSEWHEEL, the focus manager subclass of web contents will
    // route it back here.
    MSG new_message = {0};
    new_message.hwnd = m_hWnd;
    new_message.message = message;
    new_message.wParam = wparam;
    new_message.lParam = lparam;

    handled_by_webcontents =
        !!::SendMessage(GetParent(), ChromeViews::kReflectedMessage, 0,
                        reinterpret_cast<LPARAM>(&new_message));
  }

  if (!handled_by_webcontents) {
    render_widget_host_->ForwardWheelEvent(
        WebMouseWheelEvent(m_hWnd, message, wparam, lparam));
  }
  handled = TRUE;
  return 0;
}

LRESULT RenderWidgetHostHWND::OnMouseActivate(UINT, WPARAM, LPARAM,
                                              BOOL& handled) {
  // We handle WM_MOUSEACTIVATE to set focus to the underlying plugin
  // child window. This is to ensure that keyboard events are received
  // by the plugin. The correct way to fix this would be send over
  // an event to the renderer which would then eventually send over
  // a setFocus call to the plugin widget. This would ensure that
  // the renderer (webkit) knows about the plugin widget receiving
  // focus.
  // TODO(iyengar) Do the right thing as per the above comment.
  POINT cursor_pos = {0};
  ::GetCursorPos(&cursor_pos);
  MapWindowPoints(m_hWnd, &cursor_pos, 1);
  HWND child_window = ::RealChildWindowFromPoint(m_hWnd, cursor_pos);
  if (::IsWindow(child_window)) {
    ::SetFocus(child_window);
    return MA_NOACTIVATE;
  } else {
    handled = FALSE;
    return MA_ACTIVATE;
  }
}

void RenderWidgetHostHWND::OnFinalMessage(HWND window) {
  render_widget_host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostHWND::TrackMouseLeave(bool track) {
  if (track == track_mouse_leave_)
    return;
  track_mouse_leave_ = track;

  DCHECK(m_hWnd);

  TRACKMOUSEEVENT tme;
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  if (!track_mouse_leave_)
    tme.dwFlags |= TME_CANCEL;
  tme.hwndTrack = m_hWnd;

  TrackMouseEvent(&tme);
}

bool RenderWidgetHostHWND::Send(IPC::Message* message) {
  return render_widget_host_->Send(message);
}

void RenderWidgetHostHWND::EnsureTooltip() {
  UINT message = TTM_NEWTOOLRECT;

  TOOLINFO ti;
  ti.cbSize = sizeof(ti);
  ti.hwnd = m_hWnd;
  ti.uId = 0;
  if (!::IsWindow(tooltip_hwnd_)) {
    message = TTM_ADDTOOL;
    tooltip_hwnd_ = CreateWindowEx(
        WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
        TOOLTIPS_CLASS, NULL, TTS_NOPREFIX, 0, 0, 0, 0, m_hWnd, NULL,
        NULL, NULL);
    ti.uFlags = TTF_TRANSPARENT;
    ti.lpszText = LPSTR_TEXTCALLBACK;
  }

  CRect cr;
  GetClientRect(&ti.rect);
  SendMessage(tooltip_hwnd_, message, NULL, reinterpret_cast<LPARAM>(&ti));
}

void RenderWidgetHostHWND::ResetTooltip() {
  if (::IsWindow(tooltip_hwnd_))
    ::DestroyWindow(tooltip_hwnd_);
  tooltip_hwnd_ = NULL;
}

BOOL CALLBACK RenderWidgetHostHWND::DismissOwnedPopups(HWND window,
                                                       LPARAM arg) {
  const HWND toplevel_hwnd = reinterpret_cast<HWND>(arg);

  if (::IsWindowVisible(window)) {
    const HWND owner = ::GetWindow(window, GW_OWNER);
    if (toplevel_hwnd == owner) {
      ::PostMessage(window, WM_CANCELMODE, 0, 0);
    }
  }

  return TRUE;
}

void RenderWidgetHostHWND::ShutdownHost() {
  shutdown_factory_.RevokeAll();
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

