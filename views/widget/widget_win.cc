// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_win.h"

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/gfx/path.h"
#include "app/l10n_util_win.h"
#include "app/win_util.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "views/accessibility/view_accessibility.h"
#include "views/controls/native_control_win.h"
#include "views/focus/focus_util_win.h"
#include "views/views_delegate.h"
#include "views/widget/aero_tooltip_manager.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/drop_target_win.h"
#include "views/widget/root_view.h"
#include "views/window/window_win.h"

namespace views {

// Property used to link the HWND to its RootView.
static const wchar_t* const kRootViewWindowProperty = L"__ROOT_VIEW__";

bool SetRootViewForHWND(HWND hwnd, RootView* root_view) {
  return SetProp(hwnd, kRootViewWindowProperty, root_view) ? true : false;
}

RootView* GetRootViewForHWND(HWND hwnd) {
  return reinterpret_cast<RootView*>(::GetProp(hwnd, kRootViewWindowProperty));
}

NativeControlWin* GetNativeControlWinForHWND(HWND hwnd) {
  return reinterpret_cast<NativeControlWin*>(
      GetProp(hwnd, NativeControlWin::kNativeControlWinKey));
}

///////////////////////////////////////////////////////////////////////////////
// WidgetWin, public

WidgetWin::WidgetWin()
    : close_widget_factory_(this),
      active_mouse_tracking_flags_(0),
      has_capture_(false),
      use_layered_buffer_(true),
      layered_alpha_(255),
      delete_on_destroy_(true),
      can_update_layered_window_(true),
      last_mouse_event_was_move_(false),
      is_mouse_down_(false),
      is_window_(false),
      restore_focus_when_enabled_(false) {
}

WidgetWin::~WidgetWin() {
  MessageLoopForUI::current()->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// Widget implementation:

void WidgetWin::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  // Force creation of the RootView; otherwise, we may get a WM_SIZE after the
  // window is created and before the root view is set up.
  GetRootView();

  // Create the window.
  WindowImpl::Init(parent, bounds);

  // See if the style has been overridden.
  opaque_ = !(window_ex_style() & WS_EX_TRANSPARENT);
  use_layered_buffer_ = (use_layered_buffer_ &&
                         !!(window_ex_style() & WS_EX_LAYERED));

  default_theme_provider_.reset(new DefaultThemeProvider());

  SetWindowSupportsRerouteMouseWheel(hwnd());

  drop_target_ = new DropTargetWin(root_view_.get());

  if ((window_style() & WS_CHILD) == 0) {
    // Top-level widgets get a FocusManager.
    focus_manager_.reset(new FocusManager(this));
  }

  // Sets the RootView as a property, so the automation can introspect windows.
  SetRootViewForHWND(hwnd(), root_view_.get());

  MessageLoopForUI::current()->AddObserver(this);

  // Windows special DWM window frame requires a special tooltip manager so
  // that window controls in Chrome windows don't flicker when you move your
  // mouse over them. See comment in aero_tooltip_manager.h.
  if (GetThemeProvider()->ShouldUseNativeFrame()) {
    tooltip_manager_.reset(new AeroTooltipManager(this));
  } else {
    tooltip_manager_.reset(new TooltipManagerWin(this));
  }

  // This message initializes the window so that focus border are shown for
  // windows.
  SendMessage(hwnd(),
              WM_CHANGEUISTATE,
              MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS),
              0);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ImmAssociateContextEx(hwnd(), NULL, 0);
}

void WidgetWin::SetContentsView(View* view) {
  root_view_->SetContentsView(view);
}

void WidgetWin::GetBounds(gfx::Rect* out, bool including_frame) const {
  CRect crect;
  if (including_frame) {
    GetWindowRect(&crect);
    *out = gfx::Rect(crect);
    return;
  }

  GetClientRect(&crect);
  POINT p = {0, 0};
  ClientToScreen(hwnd(), &p);
  out->SetRect(crect.left + p.x, crect.top + p.y,
               crect.Width(), crect.Height());
}

void WidgetWin::SetBounds(const gfx::Rect& bounds) {
  SetWindowPos(NULL, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);
}

void WidgetWin::SetShape(const gfx::Path& shape) {
  SetWindowRgn(shape.CreateHRGN(), TRUE);
}

void WidgetWin::Close() {
  if (!IsWindow())
    return;  // No need to do anything.

  // Let's hide ourselves right away.
  Hide();

  if (close_widget_factory_.empty()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    MessageLoop::current()->PostTask(FROM_HERE,
        close_widget_factory_.NewRunnableMethod(
            &WidgetWin::CloseNow));
  }
}

void WidgetWin::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  if (IsWindow())
    DestroyWindow(hwnd());
}

void WidgetWin::Show() {
  if (IsWindow())
    ShowWindow(SW_SHOWNOACTIVATE);
}

void WidgetWin::Hide() {
  if (IsWindow()) {
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

gfx::NativeView WidgetWin::GetNativeView() const {
  return WindowImpl::hwnd();
}

static BOOL CALLBACK EnumChildProcForRedraw(HWND hwnd, LPARAM lparam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  gfx::Rect invalid_rect = *reinterpret_cast<gfx::Rect*>(lparam);

  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);
  invalid_rect.Offset(-window_rect.left, -window_rect.top);

  int flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
  if (process_id == GetCurrentProcessId())
    flags |= RDW_UPDATENOW;
  RedrawWindow(hwnd, &invalid_rect.ToRECT(), NULL, flags);
  return TRUE;
}

void WidgetWin::PaintNow(const gfx::Rect& update_rect) {
  if (use_layered_buffer_) {
    PaintLayeredWindow();
  } else if (root_view_->NeedsPainting(false) && IsWindow()) {
    if (!opaque_ && GetParent()) {
      // We're transparent. Need to force painting to occur from our parent.
      CRect parent_update_rect = update_rect.ToRECT();
      POINT location_in_parent = { 0, 0 };
      ClientToScreen(hwnd(), &location_in_parent);
      ScreenToClient(GetParent(), &location_in_parent);
      parent_update_rect.OffsetRect(location_in_parent);
      RedrawWindow(GetParent(), parent_update_rect, NULL,
                     RDW_UPDATENOW | RDW_INVALIDATE | RDW_ALLCHILDREN);
    } else {
      // Paint child windows that are in a different process asynchronously.
      // This prevents a hang in other processes from blocking this process.

      // Calculate the invalid rect in screen coordinates before  the first
      // RedrawWindow call to the parent HWND, since that will empty update_rect
      // (which comes from a member variable) in the OnPaint call.
      CRect screen_rect_temp;
      GetWindowRect(&screen_rect_temp);
      gfx::Rect screen_rect(screen_rect_temp);
      gfx::Rect invalid_screen_rect = update_rect;
      invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());

      RedrawWindow(hwnd(), &update_rect.ToRECT(), NULL,
                   RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);

      LPARAM lparam = reinterpret_cast<LPARAM>(&invalid_screen_rect);
      EnumChildWindows(hwnd(), EnumChildProcForRedraw, lparam);
    }
    // As we were created with a style of WS_CLIPCHILDREN redraw requests may
    // result in an empty paint rect in WM_PAINT (this'll happen if a
    // child HWND completely contains the update _rect). In such a scenario
    // RootView would never get a ProcessPaint and always think it needs to
    // be painted (leading to a steady stream of RedrawWindow requests on every
    // event). For this reason we tell RootView it doesn't need to paint
    // here.
    root_view_->ClearPaintRect();
  }
}

void WidgetWin::SetOpacity(unsigned char opacity) {
  layered_alpha_ = static_cast<BYTE>(opacity);
}

RootView* WidgetWin::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

Widget* WidgetWin::GetRootWidget() const {
  return reinterpret_cast<WidgetWin*>(
      win_util::GetWindowUserData(GetAncestor(hwnd(), GA_ROOT)));
}

bool WidgetWin::IsVisible() const {
  return !!::IsWindowVisible(hwnd());
}

bool WidgetWin::IsActive() const {
  return win_util::IsWindowActive(hwnd());
}

void WidgetWin::GenerateMousePressedForView(View* view,
                                            const gfx::Point& point) {
  gfx::Point point_in_widget(point);
  View::ConvertPointToWidget(view, &point_in_widget);
  root_view_->SetMouseHandler(view);
  ProcessMousePressed(point_in_widget.ToPOINT(), MK_LBUTTON, false, false);
}

TooltipManager* WidgetWin::GetTooltipManager() {
  return tooltip_manager_.get();
}

ThemeProvider* WidgetWin::GetThemeProvider() const {
  Widget* widget = GetRootWidget();
  if (widget && widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ThemeProvider* provider = widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = widget->GetDefaultThemeProvider();
    if (provider)
      return provider;
  }
  return default_theme_provider_.get();
}

Window* WidgetWin::GetWindow() {
  return GetWindowImpl(hwnd());
}

const Window* WidgetWin::GetWindow() const {
  return GetWindowImpl(hwnd());
}

FocusManager* WidgetWin::GetFocusManager() {
  if (focus_manager_.get())
    return focus_manager_.get();

  WidgetWin* widget = static_cast<WidgetWin*>(GetRootWidget());
  if (widget && widget != this) {
    // WidgetWin subclasses may override GetFocusManager(), for example for
    // dealing with cases where the widget has been unparented.
    return widget->GetFocusManager();
  }
  return NULL;
}

void WidgetWin::ViewHierarchyChanged(bool is_add, View *parent,
                                     View *child) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(child);
}

bool WidgetWin::GetAccelerator(int cmd_id, Accelerator* accelerator) {
  return false;
}

void WidgetWin::SetUseLayeredBuffer(bool use_layered_buffer) {
  if (use_layered_buffer_ == use_layered_buffer)
    return;

  use_layered_buffer_ = use_layered_buffer;
  if (!hwnd())
    return;

  if (use_layered_buffer_) {
    // Force creation of the buffer at the right size.
    RECT wr;
    GetWindowRect(&wr);
    ChangeSize(0, CSize(wr.right - wr.left, wr.bottom - wr.top));
  } else {
    contents_.reset(NULL);
  }
}

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM l_param) {
  RootView* root_view =
    reinterpret_cast<RootView*>(GetProp(hwnd, kRootViewWindowProperty));
  if (root_view) {
    *reinterpret_cast<RootView**>(l_param) = root_view;
    return FALSE;  // Stop enumerating.
  }
  return TRUE;  // Keep enumerating.
}

// static
RootView* WidgetWin::FindRootView(HWND hwnd) {
  RootView* root_view =
    reinterpret_cast<RootView*>(GetProp(hwnd, kRootViewWindowProperty));
  if (root_view)
    return root_view;

  // Enumerate all children and check if they have a RootView.
  EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(&root_view));

  return root_view;
}

// static
WidgetWin* WidgetWin::GetWidget(HWND hwnd) {
  return reinterpret_cast<WidgetWin*>(win_util::GetWindowUserData(hwnd));
}

////////////////////////////////////////////////////////////////////////////////
// MessageLoop::Observer

void WidgetWin::WillProcessMessage(const MSG& msg) {
}

void WidgetWin::DidProcessMessage(const MSG& msg) {
  if (root_view_->NeedsPainting(true)) {
    PaintNow(root_view_->GetScheduledPaintRect());
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusTraversable

View* WidgetWin::FindNextFocusableView(
    View* starting_view, bool reverse, Direction direction,
    bool check_starting_view, FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  return root_view_->FindNextFocusableView(starting_view,
                                           reverse,
                                           direction,
                                           check_starting_view,
                                           focus_traversable,
                                           focus_traversable_view);
}

FocusTraversable* WidgetWin::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void WidgetWin::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

View* WidgetWin::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void WidgetWin::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

///////////////////////////////////////////////////////////////////////////////
// Message handlers

void WidgetWin::OnActivate(UINT action, BOOL minimized, HWND window) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnActivateApp(BOOL active, DWORD thread_id) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnAppCommand(HWND window, short app_command, WORD device,
                                int keystate) {
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnCancelMode() {
}

void WidgetWin::OnCaptureChanged(HWND hwnd) {
  if (has_capture_) {
    if (is_mouse_down_)
      root_view_->ProcessMouseDragCanceled();
    is_mouse_down_ = false;
    has_capture_ = false;
  }
}

void WidgetWin::OnClose() {
  Close();
}

void WidgetWin::OnCommand(UINT notification_code, int command_id, HWND window) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnCreate(CREATESTRUCT* create_struct) {
  return 0;
}

void WidgetWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(hwnd());
    drop_target_ = NULL;
  }

  RemoveProp(hwnd(), kRootViewWindowProperty);
}

LRESULT WidgetWin::OnDwmCompositionChanged(UINT msg,
                                           WPARAM w_param,
                                           LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnEndSession(BOOL ending, UINT logoff) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnEnterSizeMove() {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnEraseBkgnd(HDC dc) {
  // This is needed for magical win32 flicker ju-ju
  return 1;
}

void WidgetWin::OnExitMenuLoop(BOOL is_track_popup_menu) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnExitSizeMove() {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnGetObject(UINT uMsg, WPARAM w_param, LPARAM l_param) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Accessibility readers will send an OBJID_CLIENT message
  if (OBJID_CLIENT == l_param) {
    // If our MSAA root is already created, reuse that pointer. Otherwise,
    // create a new one.
    if (!accessibility_root_) {
      CComObject<ViewAccessibility>* instance = NULL;

      HRESULT hr = CComObject<ViewAccessibility>::CreateInstance(&instance);
      DCHECK(SUCCEEDED(hr));

      if (!instance) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      ScopedComPtr<IAccessible> accessibility_instance(instance);

      if (!SUCCEEDED(instance->Initialize(root_view_.get()))) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }

      // All is well, assign the temp instance to the class smart pointer
      accessibility_root_.Attach(accessibility_instance.Detach());

      if (!accessibility_root_) {
        // Return with failure.
        return static_cast<LRESULT>(0L);
      }
    }

    // Create a reference to ViewAccessibility that MSAA will marshall
    // to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(accessibility_root_));
  }
  return reference_result;
}

void WidgetWin::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnHScroll(int scroll_type, short position, HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnInitMenu(HMENU menu) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnInitMenuPopup(HMENU menu,
                                UINT position,
                                BOOL is_system_menu) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags) {
  KeyEvent event(Event::ET_KEY_PRESSED, win_util::WinToKeyboardCode(c),
                 KeyEvent::GetKeyStateFlags(), rep_cnt, flags);
  RootView* root_view = GetFocusedViewRootView();
  if (!root_view)
    root_view = root_view_.get();

  SetMsgHandled(root_view->ProcessKeyEvent(event));
}

void WidgetWin::OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags) {
  KeyEvent event(Event::ET_KEY_RELEASED, win_util::WinToKeyboardCode(c),
                 KeyEvent::GetKeyStateFlags(), rep_cnt, flags);
  RootView* root_view = GetFocusedViewRootView();
  if (!root_view)
    root_view = root_view_.get();

  SetMsgHandled(root_view->ProcessKeyEvent(event));
}

// TODO(pkasting): ORing the pressed/released button into the flags is _wrong_.
// It makes it impossible to tell which button was modified when multiple
// buttons are/were held down.  We need to instead put the modified button into
// a separate member on the MouseEvent, then audit all consumers of MouseEvents
// to fix them to use the resulting values correctly.

void WidgetWin::OnLButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, false, false);
}

void WidgetWin::OnLButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_LBUTTON);
}

void WidgetWin::OnLButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, true, false);
}

void WidgetWin::OnMButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, false, false);
}

void WidgetWin::OnMButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_MBUTTON);
}

void WidgetWin::OnMButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, true, false);
}

LRESULT WidgetWin::OnMouseActivate(HWND window, UINT hittest_code,
                                   UINT message) {
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

void WidgetWin::OnMouseMove(UINT flags, const CPoint& point) {
  ProcessMouseMoved(point, flags, false);
}

LRESULT WidgetWin::OnMouseLeave(UINT message, WPARAM w_param, LPARAM l_param) {
  tooltip_manager_->OnMouseLeave();
  ProcessMouseExited();
  return 0;
}

LRESULT WidgetWin::OnMouseWheel(UINT message, WPARAM w_param, LPARAM l_param) {
  // Reroute the mouse-wheel to the window under the mouse pointer if
  // applicable.
  if (message == WM_MOUSEWHEEL &&
      views::RerouteMouseWheel(hwnd(), w_param, l_param)) {
    return 0;
  }

  int flags = GET_KEYSTATE_WPARAM(w_param);
  short distance = GET_WHEEL_DELTA_WPARAM(w_param);
  int x = GET_X_LPARAM(l_param);
  int y = GET_Y_LPARAM(l_param);
  MouseWheelEvent e(distance, x, y, Event::ConvertWindowsFlags(flags));
  return root_view_->ProcessMouseWheelEvent(e) ? 0 : 1;
}

void WidgetWin::OnMove(const CPoint& point) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnMoving(UINT param, const LPRECT new_bounds) {
}

LRESULT WidgetWin::OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param) {
  tooltip_manager_->OnMouse(msg, w_param, l_param);
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT WidgetWin::OnNCActivate(BOOL active) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT WidgetWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT WidgetWin::OnNCHitTest(const CPoint& pt) {
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnNCLButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_LBUTTON, true, true));
}

void WidgetWin::OnNCLButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_LBUTTON, false, true));
}

void WidgetWin::OnNCLButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnNCMButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_MBUTTON, true, true));
}

void WidgetWin::OnNCMButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_MBUTTON, false, true));
}

void WidgetWin::OnNCMButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnNCMouseLeave(UINT uMsg, WPARAM w_param, LPARAM l_param) {
  ProcessMouseExited();
  return 0;
}

LRESULT WidgetWin::OnNCMouseMove(UINT flags, const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, hwnd(), &temp, 1);
  ProcessMouseMoved(temp, 0, true);

  // We need to process this message to stop Windows from drawing the window
  // controls as the mouse moves over the title bar area when the window is
  // maximized.
  return 0;
}

void WidgetWin::OnNCPaint(HRGN rgn) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnNCRButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_RBUTTON, true, true));
}

void WidgetWin::OnNCRButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_RBUTTON, false, true));
}

void WidgetWin::OnNCRButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnNCUAHDrawCaption(UINT msg,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT WidgetWin::OnNCUAHDrawFrame(UINT msg, WPARAM w_param, LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT WidgetWin::OnNotify(int w_param, NMHDR* l_param) {
  // We can be sent this message before the tooltip manager is created, if a
  // subclass overrides OnCreate and creates some kind of Windows control there
  // that sends WM_NOTIFY messages.
  if (tooltip_manager_.get()) {
    bool handled;
    LRESULT result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
    SetMsgHandled(handled);
    return result;
  }
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnPaint(HDC dc) {
  root_view_->OnPaint(hwnd());
}

LRESULT WidgetWin::OnPowerBroadcast(DWORD power_event, DWORD data) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessWmPowerBroadcastMessage(power_event);
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnRButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, false, false);
}

void WidgetWin::OnRButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_RBUTTON);
}

void WidgetWin::OnRButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, true, false);
}

LRESULT WidgetWin::OnReflectedMessage(UINT msg,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnSetFocus(HWND focused_window) {
  SetMsgHandled(FALSE);
}

LRESULT WidgetWin::OnSetIcon(UINT size_type, HICON new_icon) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT WidgetWin::OnSetText(const wchar_t* text) {
  SetMsgHandled(FALSE);
  return 0;
}

void WidgetWin::OnSettingChange(UINT flags, const wchar_t* section) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnSize(UINT param, const CSize& size) {
  ChangeSize(param, size);
}

void WidgetWin::OnSysCommand(UINT notification_code, CPoint click) {
}

void WidgetWin::OnThemeChanged() {
  // Notify NativeTheme.
  gfx::NativeTheme::instance()->CloseHandles();
}

void WidgetWin::OnFinalMessage(HWND window) {
  if (delete_on_destroy_)
    delete this;
}

void WidgetWin::OnVScroll(int scroll_type, short position, HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  SetMsgHandled(FALSE);
}

void WidgetWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  SetMsgHandled(FALSE);
}

///////////////////////////////////////////////////////////////////////////////
// WidgetWin, protected:

void WidgetWin::TrackMouseEvents(DWORD mouse_tracking_flags) {
  // Begin tracking mouse events for this HWND so that we get WM_MOUSELEAVE
  // when the user moves the mouse outside this HWND's bounds.
  if (active_mouse_tracking_flags_ == 0 || mouse_tracking_flags & TME_CANCEL) {
    if (mouse_tracking_flags & TME_CANCEL) {
      // We're about to cancel active mouse tracking, so empty out the stored
      // state.
      active_mouse_tracking_flags_ = 0;
    } else {
      active_mouse_tracking_flags_ = mouse_tracking_flags;
    }

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = mouse_tracking_flags;
    tme.hwndTrack = hwnd();
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  } else if (mouse_tracking_flags != active_mouse_tracking_flags_) {
    TrackMouseEvents(active_mouse_tracking_flags_ | TME_CANCEL);
    TrackMouseEvents(mouse_tracking_flags);
  }
}

bool WidgetWin::ProcessMousePressed(const CPoint& point,
                                    UINT flags,
                                    bool dbl_click,
                                    bool non_client) {
  last_mouse_event_was_move_ = false;
  // Windows gives screen coordinates for nonclient events, while the RootView
  // expects window coordinates; convert if necessary.
  gfx::Point converted_point(point);
  if (non_client)
    View::ConvertPointToView(NULL, root_view_.get(), &converted_point);
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED,
                           converted_point.x(),
                           converted_point.y(),
                           (dbl_click ? MouseEvent::EF_IS_DOUBLE_CLICK : 0) |
                           (non_client ? MouseEvent::EF_IS_NON_CLIENT : 0) |
                           Event::ConvertWindowsFlags(flags));
  if (root_view_->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    if (!has_capture_) {
      SetCapture();
      has_capture_ = true;
    }
    return true;
  }
  return false;
}

void WidgetWin::ProcessMouseDragged(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_drag(Event::ET_MOUSE_DRAGGED,
                        point.x,
                        point.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_->OnMouseDragged(mouse_drag);
}

void WidgetWin::ProcessMouseReleased(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED,
                      point.x,
                      point.y,
                      Event::ConvertWindowsFlags(flags));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.
  if (has_capture_ && ReleaseCaptureOnMouseReleased()) {
    has_capture_ = false;
    ReleaseCapture();
  }
  is_mouse_down_ = false;
  root_view_->OnMouseReleased(mouse_up, false);
}

void WidgetWin::ProcessMouseMoved(const CPoint &point, UINT flags,
                                  bool is_nonclient) {
  // Windows only fires WM_MOUSELEAVE events if the application begins
  // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
  // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
  if (!has_capture_)
    TrackMouseEvents(is_nonclient ? TME_NONCLIENT | TME_LEAVE : TME_LEAVE);
  if (has_capture_ && is_mouse_down_) {
    ProcessMouseDragged(point, flags);
  } else {
    gfx::Point screen_loc(point);
    View::ConvertPointToScreen(root_view_.get(), &screen_loc);
    if (last_mouse_event_was_move_ && last_mouse_move_x_ == screen_loc.x() &&
        last_mouse_move_y_ == screen_loc.y()) {
      // Don't generate a mouse event for the same location as the last.
      return;
    }
    last_mouse_move_x_ = screen_loc.x();
    last_mouse_move_y_ = screen_loc.y();
    last_mouse_event_was_move_ = true;
    MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                          point.x,
                          point.y,
                          Event::ConvertWindowsFlags(flags));
    root_view_->OnMouseMoved(mouse_move);
  }
}

void WidgetWin::ProcessMouseExited() {
  last_mouse_event_was_move_ = false;
  root_view_->ProcessOnMouseExited();
  // Reset our tracking flag so that future mouse movement over this WidgetWin
  // results in a new tracking session.
  active_mouse_tracking_flags_ = 0;
}

void WidgetWin::ChangeSize(UINT size_param, const CSize& size) {
  CRect rect;
  if (use_layered_buffer_) {
    GetWindowRect(&rect);
    SizeContents(rect);
  } else {
    GetClientRect(&rect);
  }

  // Resizing changes the size of the view hierarchy and thus forces a
  // complete relayout.
  root_view_->SetBounds(0, 0, rect.Width(), rect.Height());
  root_view_->SchedulePaint();

  if (use_layered_buffer_)
    PaintNow(gfx::Rect(rect));
}

bool WidgetWin::ReleaseCaptureOnMouseReleased() {
  return true;
}

RootView* WidgetWin::CreateRootView() {
  return new RootView(this);
}

///////////////////////////////////////////////////////////////////////////////
// WidgetWin, private:

// static
Window* WidgetWin::GetWindowImpl(HWND hwnd) {
  // NOTE: we can't use GetAncestor here as constrained windows are a Window,
  // but not a top level window.
  HWND parent = hwnd;
  while (parent) {
    WidgetWin* widget =
        reinterpret_cast<WidgetWin*>(win_util::GetWindowUserData(parent));
    if (widget && widget->is_window_)
      return static_cast<WindowWin*>(widget);
    parent = ::GetParent(parent);
  }
  return NULL;
}

void WidgetWin::SizeContents(const CRect& window_rect) {
  contents_.reset(new gfx::Canvas(window_rect.Width(),
                                  window_rect.Height(),
                                  false));
}

void WidgetWin::PaintLayeredWindow() {
  // Painting monkeys with our cliprect, so we need to save it so that the
  // call to UpdateLayeredWindow updates the entire window, not just the
  // cliprect.
  contents_->save(SkCanvas::kClip_SaveFlag);
  gfx::Rect dirty_rect = root_view_->GetScheduledPaintRect();
  contents_->ClipRectInt(dirty_rect.x(), dirty_rect.y(), dirty_rect.width(),
                         dirty_rect.height());
  root_view_->ProcessPaint(contents_.get());
  contents_->restore();

  UpdateWindowFromContents(contents_->getTopPlatformDevice().getBitmapDC());
}

void WidgetWin::UpdateWindowFromContents(HDC dib_dc) {
  DCHECK(use_layered_buffer_);
  if (can_update_layered_window_) {
    CRect wr;
    GetWindowRect(&wr);
    CSize size(wr.right - wr.left, wr.bottom - wr.top);
    CPoint zero_origin(0, 0);
    CPoint window_position = wr.TopLeft();

    BLENDFUNCTION blend = {AC_SRC_OVER, 0, layered_alpha_, AC_SRC_ALPHA};
    UpdateLayeredWindow(
        hwnd(), NULL, &window_position, &size, dib_dc, &zero_origin,
        RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
  }
}

RootView* WidgetWin::GetFocusedViewRootView() {
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return NULL;
  }
  View* focused_view = focus_manager->GetFocusedView();
  if (!focused_view)
    return NULL;
  return focused_view->GetRootView();
}

// Get the source HWND of the specified message. Depending on the message, the
// source HWND is encoded in either the WPARAM or the LPARAM value.
HWND GetControlHWNDForMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  // Each of the following messages can be sent by a child HWND and must be
  // forwarded to its associated NativeControlWin for handling.
  switch (message) {
    case WM_NOTIFY:
      return reinterpret_cast<NMHDR*>(l_param)->hwndFrom;
    case WM_COMMAND:
      return reinterpret_cast<HWND>(l_param);
    case WM_CONTEXTMENU:
      return reinterpret_cast<HWND>(w_param);
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
      return reinterpret_cast<HWND>(l_param);
  }
  return NULL;
}

HICON WidgetWin::GetDefaultWindowIcon() const {
  if (ViewsDelegate::views_delegate)
    return ViewsDelegate::views_delegate->GetDefaultWindowIcon();
  return NULL;
}

// Some messages may be sent to us by a child HWND managed by
// NativeControlWin. If this is the case, this function will forward those
// messages on to the object associated with the source HWND and return true,
// in which case the window procedure must not do any further processing of
// the message. If there is no associated NativeControlWin, the return value
// will be false and the WndProc can continue processing the message normally.
// |l_result| contains the result of the message processing by the control and
// must be returned by the WndProc if the return value is true.
bool ProcessNativeControlMessage(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param,
                                 LRESULT* l_result) {
  *l_result = 0;

  HWND control_hwnd = GetControlHWNDForMessage(message, w_param, l_param);
  if (IsWindow(control_hwnd)) {
    NativeControlWin* wrapper = GetNativeControlWinForHWND(control_hwnd);
    if (wrapper)
      return wrapper->ProcessMessage(message, w_param, l_param, l_result);
  }

  return false;
}

LRESULT WidgetWin::OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) {
  HWND window = hwnd();
  LRESULT result = 0;

  // First allow messages sent by child controls to be processed directly by
  // their associated views. If such a view is present, it will handle the
  // message *instead of* this WidgetWin.
  if (ProcessNativeControlMessage(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  if (!ProcessWindowMessage(window, message, w_param, l_param, result))
    result = DefWindowProc(window, message, w_param, l_param);
  if (message == WM_NCDESTROY)
    OnFinalMessage(window);
  if (message == WM_ACTIVATE)
    PostProcessActivateMessage(this, LOWORD(w_param));
  if (message == WM_ENABLE && restore_focus_when_enabled_) {
    restore_focus_when_enabled_ = false;
    focus_manager_->RestoreFocusedView();
  }
  return result;
}

// static
void WidgetWin::PostProcessActivateMessage(WidgetWin* widget,
                                           int activation_state) {
  if (!widget->focus_manager_.get()) {
    NOTREACHED();
    return;
  }
  if (WA_INACTIVE == activation_state) {
    widget->focus_manager_->StoreFocusedView();
  } else {
    // We must restore the focus after the message has been DefProc'ed as it
    // does set the focus to the last focused HWND.
    // Note that if the window is not enabled, we cannot restore the focus as
    // calling ::SetFocus on a child of the non-enabled top-window would fail.
    // This is the case when showing a modal dialog (such as 'open file',
    // 'print'...) from a different thread.
    // In that case we delay the focus restoration to when the window is enabled
    // again.
    if (!IsWindowEnabled(widget->GetNativeView())) {
      DCHECK(!widget->restore_focus_when_enabled_);
      widget->restore_focus_when_enabled_ = true;
      return;
    }
    widget->focus_manager_->RestoreFocusedView();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
Widget* Widget::CreateTransparentPopupWidget(bool delete_on_destroy) {
  WidgetWin* popup = new WidgetWin;
  popup->set_window_style(WS_POPUP);
  popup->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                             WS_EX_TRANSPARENT |
                             l10n_util::GetExtendedTooltipStyles());
  popup->set_delete_on_destroy(delete_on_destroy);
  return popup;
}

}  // namespace views
