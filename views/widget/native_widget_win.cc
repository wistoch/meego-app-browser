// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_win.h"

#include <dwmapi.h>

#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/system_monitor/system_monitor.h"
#include "ui/base/theme_provider.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/native_theme_win.h"
#include "ui/gfx/path.h"
#include "views/accessibility/view_accessibility.h"
#include "views/controls/native_control_win.h"
#include "views/focus/accelerator_handler.h"
#include "views/focus/focus_util_win.h"
#include "views/views_delegate.h"
#include "views/widget/aero_tooltip_manager.h"
#include "views/widget/child_window_message_processor.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/drop_target_win.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_delegate.h"
#include "views/widget/widget_utils.h"
#include "views/window/window_win.h"

#pragma comment(lib, "dwmapi.lib")

using ui::ViewProp;

namespace {

// Returns whether the specified window is the current active window.
bool IsWindowActive(HWND hwnd) {
  WINDOWINFO info;
  return ::GetWindowInfo(hwnd, &info) &&
         ((info.dwWindowStatus & WS_ACTIVECAPTION) != 0);
}

}  // namespace

namespace views {

// Property used to link the HWND to its RootView.
static const char* const kRootViewWindowProperty = "__ROOT_VIEW__";

// Links the HWND to it's Widget (as a Widget, not a NativeWidgetWin).
static const char* const kWidgetKey = "__VIEWS_WIDGET__";

// static
bool NativeWidgetWin::screen_reader_active_ = false;

// A custom MSAA object id used to determine if a screen reader is actively
// listening for MSAA events.
#define OBJID_CUSTOM 1

RootView* GetRootViewForHWND(HWND hwnd) {
  return reinterpret_cast<RootView*>(
      ViewProp::GetValue(hwnd, kRootViewWindowProperty));
}

///////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, public

NativeWidgetWin::NativeWidgetWin()
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
      restore_focus_when_enabled_(false),
      delegate_(NULL),
      accessibility_view_events_index_(-1),
      accessibility_view_events_(kMaxAccessibilityViewEvents) {
}

NativeWidgetWin::~NativeWidgetWin() {
}

// static
NativeWidgetWin* NativeWidgetWin::GetWidget(HWND hwnd) {
  // TODO(jcivelli): http://crbug.com/44499 We need a way to test that hwnd is
  //                 associated with a NativeWidgetWin (it might be a pure
  //                 WindowImpl).
  if (!WindowImpl::IsWindowImpl(hwnd))
    return NULL;
  return reinterpret_cast<NativeWidgetWin*>(ui::GetWindowUserData(hwnd));
}

// static
NativeWidgetWin* NativeWidgetWin::GetRootWidget(HWND hwnd) {
  // First, check if the top-level window is a Widget.
  HWND root = ::GetAncestor(hwnd, GA_ROOT);
  if (!root)
    return NULL;

  NativeWidgetWin* widget = NativeWidgetWin::GetWidget(root);
  if (widget)
    return widget;

  // Second, try to locate the last Widget window in the parent hierarchy.
  HWND parent_hwnd = hwnd;
  NativeWidgetWin* parent_widget;
  do {
    parent_widget = NativeWidgetWin::GetWidget(parent_hwnd);
    if (parent_widget) {
      widget = parent_widget;
      parent_hwnd = ::GetAncestor(parent_hwnd, GA_PARENT);
    }
  } while (parent_hwnd != NULL && parent_widget != NULL);

  return widget;
}

// static
bool NativeWidgetWin::IsAeroGlassEnabled() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
  // If composition is not enabled, we behave like on XP.
  BOOL enabled = FALSE;
  return SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled;
}

void NativeWidgetWin::SetUseLayeredBuffer(bool use_layered_buffer) {
  if (use_layered_buffer_ == use_layered_buffer)
    return;

  use_layered_buffer_ = use_layered_buffer;
  if (!hwnd())
    return;

  if (use_layered_buffer_)
    LayoutRootView();
  else
    contents_.reset(NULL);
}

View* NativeWidgetWin::GetAccessibilityViewEventAt(int id) {
  // Convert from MSAA child id.
  id = -(id + 1);
  DCHECK(id >= 0 && id < kMaxAccessibilityViewEvents);
  return accessibility_view_events_[id];
}

int NativeWidgetWin::AddAccessibilityViewEvent(View* view) {
  accessibility_view_events_index_ =
      (accessibility_view_events_index_ + 1) % kMaxAccessibilityViewEvents;
  accessibility_view_events_[accessibility_view_events_index_] = view;

  // Convert to MSAA child id.
  return -(accessibility_view_events_index_ + 1);
}

void NativeWidgetWin::ClearAccessibilityViewEvent(View* view) {
  for (std::vector<View*>::iterator it = accessibility_view_events_.begin();
      it != accessibility_view_events_.end();
      ++it) {
    if (*it == view)
      *it = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Widget implementation:

void NativeWidgetWin::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  // Force creation of the RootView; otherwise, we may get a WM_SIZE after the
  // window is created and before the root view is set up.
  GetRootView();

  // Create the window.
  WindowImpl::Init(parent, bounds);

  // Attempt to detect screen readers by sending an event with our custom id.
  if (!IsAccessibleWidget())
    NotifyWinEvent(EVENT_SYSTEM_ALERT, hwnd(), OBJID_CUSTOM, CHILDID_SELF);

  // See if the style has been overridden.
  opaque_ = !(window_ex_style() & WS_EX_TRANSPARENT);
  use_layered_buffer_ = (use_layered_buffer_ &&
                         !!(window_ex_style() & WS_EX_LAYERED));

  default_theme_provider_.reset(new DefaultThemeProvider());

  props_.push_back(SetWindowSupportsRerouteMouseWheel(hwnd()));

  drop_target_ = new DropTargetWin(root_view_.get());

  if ((window_style() & WS_CHILD) == 0 ||
      (NativeWidgetWin::GetRootWidget(parent) == NULL &&
          parent != GetDesktopWindow())) {
    // Top-level widgets and child widgets who do not have a top-level widget
    // ancestor get a FocusManager. Child widgets parented to the desktop do not
    // get a FocusManager because parenting to the desktop is the technique used
    // to intentionally exclude a widget from the FocusManager hierarchy.
    focus_manager_.reset(new FocusManager(this));
  }

  // Sets the RootView as a property, so the automation can introspect windows.
  SetNativeWindowProperty(kRootViewWindowProperty, root_view_.get());

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

void NativeWidgetWin::InitWithWidget(Widget* parent, const gfx::Rect& bounds) {
  Init(parent->GetNativeView(), bounds);
}

WidgetDelegate* NativeWidgetWin::GetWidgetDelegate() {
  return delegate_;
}

void NativeWidgetWin::SetWidgetDelegate(WidgetDelegate* delegate) {
  delegate_ = delegate;
}

void NativeWidgetWin::SetContentsView(View* view) {
  root_view_->SetContentsView(view);
}

void NativeWidgetWin::GetBounds(gfx::Rect* out, bool including_frame) const {
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

void NativeWidgetWin::SetBounds(const gfx::Rect& bounds) {
  LONG style = GetWindowLong(GWL_STYLE);
  if (style & WS_MAXIMIZE)
    SetWindowLong(GWL_STYLE, style & ~WS_MAXIMIZE);
  SetWindowPos(NULL, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);
}

void NativeWidgetWin::MoveAbove(Widget* other) {
  gfx::Rect bounds;
  GetBounds(&bounds, false);
  SetWindowPos(other->GetNativeView(), bounds.x(), bounds.y(),
               bounds.width(), bounds.height(), SWP_NOACTIVATE);
}

void NativeWidgetWin::SetShape(gfx::NativeRegion region) {
  SetWindowRgn(region, TRUE);
}

void NativeWidgetWin::Close() {
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
            &NativeWidgetWin::CloseNow));
  }
}

void NativeWidgetWin::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  if (IsWindow())
    DestroyWindow(hwnd());
}

void NativeWidgetWin::Show() {
  if (IsWindow())
    ShowWindow(SW_SHOWNOACTIVATE);
}

void NativeWidgetWin::Hide() {
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

gfx::NativeView NativeWidgetWin::GetNativeView() const {
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

void NativeWidgetWin::PaintNow(const gfx::Rect& update_rect) {
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

void NativeWidgetWin::SetOpacity(unsigned char opacity) {
  layered_alpha_ = static_cast<BYTE>(opacity);
}

void NativeWidgetWin::SetAlwaysOnTop(bool on_top) {
  if (on_top)
    set_window_ex_style(window_ex_style() | WS_EX_TOPMOST);
  else
    set_window_ex_style(window_ex_style() & ~WS_EX_TOPMOST);
}

RootView* NativeWidgetWin::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

Widget* NativeWidgetWin::GetRootWidget() const {
  return GetRootWidget(hwnd());
}

bool NativeWidgetWin::IsVisible() const {
  return !!::IsWindowVisible(hwnd());
}

bool NativeWidgetWin::IsActive() const {
  return IsWindowActive(hwnd());
}

bool NativeWidgetWin::IsAccessibleWidget() const {
  return screen_reader_active_;
}

TooltipManager* NativeWidgetWin::GetTooltipManager() {
  return tooltip_manager_.get();
}

void NativeWidgetWin::GenerateMousePressedForView(View* view,
                                            const gfx::Point& point) {
  gfx::Point point_in_widget(point);
  View::ConvertPointToWidget(view, &point_in_widget);
  root_view_->SetMouseHandler(view);
  ProcessMousePressed(point_in_widget.ToPOINT(), MK_LBUTTON, false, false);
}

bool NativeWidgetWin::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

Window* NativeWidgetWin::GetWindow() {
  return GetWindowImpl(hwnd());
}

const Window* NativeWidgetWin::GetWindow() const {
  return GetWindowImpl(hwnd());
}

void NativeWidgetWin::SetNativeWindowProperty(const char* name, void* value) {
  // Remove the existing property (if any).
  for (ViewProps::iterator i = props_.begin(); i != props_.end(); ++i) {
    if ((*i)->Key() == name) {
      props_.erase(i);
      break;
    }
  }

  if (value)
    props_.push_back(new ViewProp(hwnd(), name, value));
}

void* NativeWidgetWin::GetNativeWindowProperty(const char* name) {
  return ViewProp::GetValue(hwnd(), name);
}

ThemeProvider* NativeWidgetWin::GetThemeProvider() const {
  return GetWidgetThemeProvider(this);
}

ThemeProvider* NativeWidgetWin::GetDefaultThemeProvider() const {
  return default_theme_provider_.get();
}

FocusManager* NativeWidgetWin::GetFocusManager() {
  if (focus_manager_.get())
    return focus_manager_.get();

  NativeWidgetWin* widget = static_cast<NativeWidgetWin*>(GetRootWidget());
  if (widget && widget != this) {
    // NativeWidgetWin subclasses may override GetFocusManager(), for example
    // for dealing with cases where the widget has been unparented.
    return widget->GetFocusManager();
  }
  return NULL;
}

void NativeWidgetWin::ViewHierarchyChanged(bool is_add, View *parent,
                                           View *child) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(child);

  if (!is_add)
    ClearAccessibilityViewEvent(child);
}


bool NativeWidgetWin::ContainsNativeView(gfx::NativeView native_view) {
  if (hwnd() == native_view)
    return true;

  // Traverse the set of parents of the given view to determine if native_view
  // is a descendant of this window.
  HWND parent_window = ::GetParent(native_view);
  HWND previous_child = native_view;
  while (parent_window && parent_window != previous_child) {
    if (hwnd() == parent_window)
      return true;
    previous_child = parent_window;
    parent_window = ::GetParent(parent_window);
  }

  // A views::NativeViewHost may contain the given native view, without it being
  // an ancestor of hwnd(), so traverse the views::View hierarchy looking for
  // such views.
  return GetRootView()->ContainsNativeView(native_view);
}

////////////////////////////////////////////////////////////////////////////////
// MessageLoop::Observer

void NativeWidgetWin::WillProcessMessage(const MSG& msg) {
}

void NativeWidgetWin::DidProcessMessage(const MSG& msg) {
  if (root_view_->NeedsPainting(true))
    PaintNow(root_view_->GetScheduledPaintRect());
}

////////////////////////////////////////////////////////////////////////////////
// FocusTraversable

FocusSearch* NativeWidgetWin::GetFocusSearch() {
  return root_view_->GetFocusSearch();
}

FocusTraversable* NativeWidgetWin::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void NativeWidgetWin::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

View* NativeWidgetWin::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void NativeWidgetWin::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

///////////////////////////////////////////////////////////////////////////////
// Message handlers

void NativeWidgetWin::OnActivate(UINT action, BOOL minimized, HWND window) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnActivateApp(BOOL active, DWORD thread_id) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnAppCommand(HWND window,
                                      short app_command,
                                      WORD device,
                                      int keystate) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnCancelMode() {
}

void NativeWidgetWin::OnCaptureChanged(HWND hwnd) {
  if (has_capture_) {
    if (is_mouse_down_)
      root_view_->ProcessMouseDragCanceled();
    is_mouse_down_ = false;
    has_capture_ = false;
  }
}

void NativeWidgetWin::OnClose() {
  Close();
}

void NativeWidgetWin::OnCommand(UINT notification_code, int command_id,
                                HWND window) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnCreate(CREATESTRUCT* create_struct) {
  // Widget::GetWidgetFromNativeView expects the contents of this property
  // to be of type Widget, so the cast is necessary.
  SetNativeWindowProperty(kWidgetKey, static_cast<Widget*>(this));
  return 0;
}

void NativeWidgetWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(hwnd());
    drop_target_ = NULL;
  }

  props_.reset();
}

void NativeWidgetWin::OnDisplayChange(UINT bits_per_pixel, CSize screen_size) {
  if (GetWidgetDelegate())
    GetWidgetDelegate()->DisplayChanged();
}

LRESULT NativeWidgetWin::OnDwmCompositionChanged(UINT msg,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnEndSession(BOOL ending, UINT logoff) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnEnterSizeMove() {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnEraseBkgnd(HDC dc) {
  // This is needed for magical win32 flicker ju-ju
  return 1;
}

void NativeWidgetWin::OnExitMenuLoop(BOOL is_track_popup_menu) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnExitSizeMove() {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnGetObject(UINT uMsg, WPARAM w_param,
                                     LPARAM l_param) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Accessibility readers will send an OBJID_CLIENT message
  if (OBJID_CLIENT == l_param) {
    // Retrieve MSAA dispatch object for the root view.
    base::win::ScopedComPtr<IAccessible> root(
        ViewAccessibility::GetAccessibleForView(GetRootView()));

    // Create a reference that MSAA will marshall to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(root.Detach()));
  }

  if (OBJID_CUSTOM == l_param) {
    // An MSAA client requestes our custom id. Assume that we have detected an
    // active windows screen reader.
    OnScreenReaderDetected();

    // Return with failure.
    return static_cast<LRESULT>(0L);
  }

  return reference_result;
}

void NativeWidgetWin::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnHScroll(int scroll_type, short position,
                                HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnInitMenu(HMENU menu) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnInitMenuPopup(HMENU menu,
                                UINT position,
                                BOOL is_system_menu) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnKeyDown(UINT message, WPARAM w_param,
                                   LPARAM l_param) {
  RootView* root_view = GetFocusedViewRootView();
  if (!root_view)
    root_view = root_view_.get();

  MSG msg;
  MakeMSG(&msg, message, w_param, l_param);
  SetMsgHandled(root_view->ProcessKeyEvent(KeyEvent(msg)));
  return 0;
}

LRESULT NativeWidgetWin::OnKeyUp(UINT message, WPARAM w_param, LPARAM l_param) {
  RootView* root_view = GetFocusedViewRootView();
  if (!root_view)
    root_view = root_view_.get();

  MSG msg;
  MakeMSG(&msg, message, w_param, l_param);
  SetMsgHandled(root_view->ProcessKeyEvent(KeyEvent(msg)));
  return 0;
}

void NativeWidgetWin::OnKillFocus(HWND focused_window) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      this->GetNativeView(),
      focused_window);
  SetMsgHandled(FALSE);
}

// TODO(pkasting): ORing the pressed/released button into the flags is _wrong_.
// It makes it impossible to tell which button was modified when multiple
// buttons are/were held down.  We need to instead put the modified button into
// a separate member on the MouseEvent, then audit all consumers of MouseEvents
// to fix them to use the resulting values correctly.

void NativeWidgetWin::OnLButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, false, false);
}

void NativeWidgetWin::OnLButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_LBUTTON);
}

void NativeWidgetWin::OnLButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, true, false);
}

void NativeWidgetWin::OnMButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, false, false);
}

void NativeWidgetWin::OnMButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_MBUTTON);
}

void NativeWidgetWin::OnMButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, true, false);
}

LRESULT NativeWidgetWin::OnMouseActivate(HWND window, UINT hittest_code,
                                   UINT message) {
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

void NativeWidgetWin::OnMouseMove(UINT flags, const CPoint& point) {
  ProcessMouseMoved(point, flags, false);
}

LRESULT NativeWidgetWin::OnMouseLeave(UINT message, WPARAM w_param,
                                      LPARAM l_param) {
  tooltip_manager_->OnMouseLeave();
  ProcessMouseExited();
  return 0;
}

LRESULT NativeWidgetWin::OnMouseWheel(UINT message, WPARAM w_param,
                                      LPARAM l_param) {
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

void NativeWidgetWin::OnMove(const CPoint& point) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnMoving(UINT param, const LPRECT new_bounds) {
}

LRESULT NativeWidgetWin::OnMouseRange(UINT msg, WPARAM w_param,
                                      LPARAM l_param) {
  tooltip_manager_->OnMouse(msg, w_param, l_param);
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCActivate(BOOL active) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCHitTest(const CPoint& pt) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnNCLButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_LBUTTON, true, true));
}

void NativeWidgetWin::OnNCLButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_LBUTTON, false, true));
}

void NativeWidgetWin::OnNCLButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnNCMButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_MBUTTON, true, true));
}

void NativeWidgetWin::OnNCMButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_MBUTTON, false, true));
}

void NativeWidgetWin::OnNCMButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnNCMouseLeave(UINT uMsg, WPARAM w_param, LPARAM l_param) {
  ProcessMouseExited();
  return 0;
}

LRESULT NativeWidgetWin::OnNCMouseMove(UINT flags, const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, hwnd(), &temp, 1);
  ProcessMouseMoved(temp, 0, true);

  // We need to process this message to stop Windows from drawing the window
  // controls as the mouse moves over the title bar area when the window is
  // maximized.
  return 0;
}

void NativeWidgetWin::OnNCPaint(HRGN rgn) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnNCRButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_RBUTTON, true, true));
}

void NativeWidgetWin::OnNCRButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(ProcessMousePressed(point, flags | MK_RBUTTON, false, true));
}

void NativeWidgetWin::OnNCRButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnNCUAHDrawCaption(UINT msg,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCUAHDrawFrame(UINT msg, WPARAM w_param, LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNotify(int w_param, NMHDR* l_param) {
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

void NativeWidgetWin::OnPaint(HDC dc) {
  root_view_->OnPaint(hwnd());
}

LRESULT NativeWidgetWin::OnPowerBroadcast(DWORD power_event, DWORD data) {
  ui::SystemMonitor* monitor = ui::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessWmPowerBroadcastMessage(power_event);
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnRButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, false, false);
}

void NativeWidgetWin::OnRButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_RBUTTON);
}

void NativeWidgetWin::OnRButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, true, false);
}

LRESULT NativeWidgetWin::OnReflectedMessage(UINT msg,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnSetFocus(HWND focused_window) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      focused_window,
      this->GetNativeView());
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnSetIcon(UINT size_type, HICON new_icon) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnSetText(const wchar_t* text) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnSettingChange(UINT flags, const wchar_t* section) {
  if (flags == SPI_SETWORKAREA && GetWidgetDelegate())
    GetWidgetDelegate()->WorkAreaChanged();
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnSize(UINT param, const CSize& size) {
  LayoutRootView();
}

void NativeWidgetWin::OnSysCommand(UINT notification_code, CPoint click) {
}

void NativeWidgetWin::OnThemeChanged() {
  // Notify NativeTheme.
  gfx::NativeTheme::instance()->CloseHandles();
}

void NativeWidgetWin::OnFinalMessage(HWND window) {
  if (delete_on_destroy_)
    delete this;
}

void NativeWidgetWin::OnVScroll(int scroll_type, short position, HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  SetMsgHandled(FALSE);
}

gfx::Size NativeWidgetWin::GetRootViewSize() const {
  CRect rect;
  if (use_layered_buffer_)
    GetWindowRect(&rect);
  else
    GetClientRect(&rect);

  return gfx::Size(rect.Width(), rect.Height());
}

///////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, protected:

void NativeWidgetWin::TrackMouseEvents(DWORD mouse_tracking_flags) {
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

bool NativeWidgetWin::ProcessMousePressed(const CPoint& point,
                                          UINT flags,
                                          bool dbl_click,
                                          bool non_client) {
  last_mouse_event_was_move_ = false;
  // Windows gives screen coordinates for nonclient events, while the RootView
  // expects window coordinates; convert if necessary.
  gfx::Point converted_point(point);
  if (non_client)
    View::ConvertPointToView(NULL, root_view_.get(), &converted_point);
  MouseEvent mouse_pressed(ui::ET_MOUSE_PRESSED,
                           converted_point.x(),
                           converted_point.y(),
                           (dbl_click ? ui::EF_IS_DOUBLE_CLICK : 0) |
                           (non_client ? ui::EF_IS_NON_CLIENT : 0) |
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

void NativeWidgetWin::ProcessMouseDragged(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_drag(ui::ET_MOUSE_DRAGGED,
                        point.x,
                        point.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_->OnMouseDragged(mouse_drag);
}

void NativeWidgetWin::ProcessMouseReleased(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(ui::ET_MOUSE_RELEASED,
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

void NativeWidgetWin::ProcessMouseMoved(const CPoint &point, UINT flags,
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
    MouseEvent mouse_move(ui::ET_MOUSE_MOVED,
                          point.x,
                          point.y,
                          Event::ConvertWindowsFlags(flags));
    root_view_->OnMouseMoved(mouse_move);
  }
}

void NativeWidgetWin::ProcessMouseExited() {
  last_mouse_event_was_move_ = false;
  root_view_->ProcessOnMouseExited();
  // Reset our tracking flag so that future mouse movement over this NativeWidgetWin
  // results in a new tracking session.
  active_mouse_tracking_flags_ = 0;
}

void NativeWidgetWin::LayoutRootView() {
  gfx::Size size(GetRootViewSize());

  if (use_layered_buffer_)
    SizeContents(size);

  // Resizing changes the size of the view hierarchy and thus forces a
  // complete relayout.
  root_view_->SetBounds(0, 0, size.width(), size.height());
  root_view_->SchedulePaint();

  if (use_layered_buffer_)
    PaintNow(gfx::Rect(0, 0, size.width(), size.height()));
}

void NativeWidgetWin::OnScreenReaderDetected() {
  screen_reader_active_ = true;
}

bool NativeWidgetWin::ReleaseCaptureOnMouseReleased() {
  return true;
}

RootView* NativeWidgetWin::CreateRootView() {
  return new RootView(this);
}

///////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, private:

// static
Window* NativeWidgetWin::GetWindowImpl(HWND hwnd) {
  // NOTE: we can't use GetAncestor here as constrained windows are a Window,
  // but not a top level window.
  HWND parent = hwnd;
  while (parent) {
    NativeWidgetWin* widget =
        reinterpret_cast<NativeWidgetWin*>(ui::GetWindowUserData(parent));
    if (widget && widget->is_window_)
      return static_cast<WindowWin*>(widget);
    parent = ::GetParent(parent);
  }
  return NULL;
}

void NativeWidgetWin::SizeContents(const gfx::Size& window_size) {
  contents_.reset(new gfx::CanvasSkia(window_size.width(),
                                      window_size.height(),
                                      false));
}

void NativeWidgetWin::PaintLayeredWindow() {
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

void NativeWidgetWin::UpdateWindowFromContents(HDC dib_dc) {
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

RootView* NativeWidgetWin::GetFocusedViewRootView() {
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
static HWND GetControlHWNDForMessage(UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
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

HICON NativeWidgetWin::GetDefaultWindowIcon() const {
  if (ViewsDelegate::views_delegate)
    return ViewsDelegate::views_delegate->GetDefaultWindowIcon();
  return NULL;
}

// Some messages may be sent to us by a child HWND. If this is the case, this
// function will forward those messages on to the object associated with the
// source HWND and return true, in which case the window procedure must not do
// any further processing of the message. If there is no associated
// ChildWindowMessageProcessor, the return value will be false and the WndProc
// can continue processing the message normally.  |l_result| contains the result
// of the message processing by the control and must be returned by the WndProc
// if the return value is true.
static bool ProcessChildWindowMessage(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param,
                                      LRESULT* l_result) {
  *l_result = 0;

  HWND control_hwnd = GetControlHWNDForMessage(message, w_param, l_param);
  if (IsWindow(control_hwnd)) {
    ChildWindowMessageProcessor* processor =
        ChildWindowMessageProcessor::Get(control_hwnd);
    if (processor)
      return processor->ProcessMessage(message, w_param, l_param, l_result);
  }

  return false;
}

LRESULT NativeWidgetWin::OnWndProc(UINT message, WPARAM w_param, LPARAM l_param) {
  HWND window = hwnd();
  LRESULT result = 0;

  // First allow messages sent by child controls to be processed directly by
  // their associated views. If such a view is present, it will handle the
  // message *instead of* this NativeWidgetWin.
  if (ProcessChildWindowMessage(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  if (!ProcessWindowMessage(window, message, w_param, l_param, result))
    result = DefWindowProc(window, message, w_param, l_param);
  if (message == WM_NCDESTROY) {
    MessageLoopForUI::current()->RemoveObserver(this);
    OnFinalMessage(window);
  }
  if (message == WM_ACTIVATE)
    PostProcessActivateMessage(this, LOWORD(w_param));
  if (message == WM_ENABLE && restore_focus_when_enabled_) {
    restore_focus_when_enabled_ = false;
    focus_manager_->RestoreFocusedView();
  }
  return result;
}

// static
void NativeWidgetWin::PostProcessActivateMessage(NativeWidgetWin* widget,
                                                 int activation_state) {
  if (!widget->focus_manager_.get()) {
    NOTREACHED();
    return;
  }
  if (WA_INACTIVE == activation_state) {
    // We might get activated/inactivated without being enabled, so we need to
    // clear restore_focus_when_enabled_.
    widget->restore_focus_when_enabled_ = false;
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

void NativeWidgetWin::MakeMSG(MSG* msg, UINT message, WPARAM w_param,
                        LPARAM l_param) const {
  msg->hwnd = hwnd();
  msg->message = message;
  msg->wParam = w_param;
  msg->lParam = l_param;
  msg->time = 0;
  msg->pt.x = msg->pt.y = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
Widget* Widget::CreatePopupWidget(TransparencyParam transparent,
                                  EventsParam accept_events,
                                  DeleteParam delete_on_destroy,
                                  MirroringParam mirror_in_rtl) {
  NativeWidgetWin* popup = new NativeWidgetWin;
  DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
  if (mirror_in_rtl == MirrorOriginInRTL)
    ex_style |= l10n_util::GetExtendedTooltipStyles();
  if (transparent == Transparent)
    ex_style |= WS_EX_LAYERED;
  if (accept_events != AcceptEvents)
    ex_style |= WS_EX_TRANSPARENT;
  popup->set_window_style(WS_POPUP);
  popup->set_window_ex_style(ex_style);
  popup->set_delete_on_destroy(delete_on_destroy == DeleteOnDestroy);
  return popup;
}

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM l_param) {
  RootView* root_view = GetRootViewForHWND(hwnd);
  if (root_view) {
    *reinterpret_cast<RootView**>(l_param) = root_view;
    return FALSE;  // Stop enumerating.
  }
  return TRUE;  // Keep enumerating.
}

// static
RootView* Widget::FindRootView(HWND hwnd) {
  RootView* root_view = GetRootViewForHWND(hwnd);
  if (root_view)
    return root_view;

  // Enumerate all children and check if they have a RootView.
  EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(&root_view));

  return root_view;
}

// Enumerate child windows as they could have RootView distinct from
// the HWND's root view.
BOOL CALLBACK EnumAllRootViewsChildProc(HWND hwnd, LPARAM l_param) {
  RootView* root_view = GetRootViewForHWND(hwnd);
  if (root_view) {
    std::set<RootView*>* root_views_set =
        reinterpret_cast<std::set<RootView*>*>(l_param);
    root_views_set->insert(root_view);
  }
  return TRUE;  // Keep enumerating.
}

void Widget::FindAllRootViews(HWND window,
                              std::vector<RootView*>* root_views) {
  RootView* root_view = GetRootViewForHWND(window);
  std::set<RootView*> root_views_set;
  if (root_view)
    root_views_set.insert(root_view);
  // Enumerate all children and check if they have a RootView.
  EnumChildWindows(window, EnumAllRootViewsChildProc,
      reinterpret_cast<LPARAM>(&root_views_set));
  root_views->clear();
  root_views->reserve(root_views_set.size());
  for (std::set<RootView*>::iterator it = root_views_set.begin();
      it != root_views_set.end();
      ++it)
    root_views->push_back(*it);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
Widget* Widget::GetWidgetFromNativeView(gfx::NativeView native_view) {
  return IsWindow(native_view) ?
      reinterpret_cast<Widget*>(ViewProp::GetValue(native_view, kWidgetKey)) :
      NULL;
}

// static
Widget* Widget::GetWidgetFromNativeWindow(gfx::NativeWindow native_window) {
  return Widget::GetWidgetFromNativeView(native_window);
}

// static
void Widget::NotifyLocaleChanged() {
  NOTIMPLEMENTED();
}

}  // namespace views