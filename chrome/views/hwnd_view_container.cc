// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/hwnd_view_container.h"

#include "base/gfx/native_theme.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/win_util.h"
#include "chrome/views/aero_tooltip_manager.h"
#include "chrome/views/accessibility/view_accessibility.h"
#include "chrome/views/hwnd_notification_source.h"
#include "chrome/views/root_view.h"

namespace ChromeViews {

static const DWORD kWindowDefaultChildStyle =
    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
static const DWORD kWindowDefaultStyle = WS_OVERLAPPEDWINDOW;
static const DWORD kWindowDefaultExStyle = 0;

// Property used to link the HWND to its RootView.
static const wchar_t* const kRootViewWindowProperty = L"__ROOT_VIEW__";

bool SetRootViewForHWND(HWND hwnd, RootView* root_view) {
  return ::SetProp(hwnd, kRootViewWindowProperty, root_view) ? true : false;
}

RootView* GetRootViewForHWND(HWND hwnd) {
  return reinterpret_cast<RootView*>(::GetProp(hwnd, kRootViewWindowProperty));
}

// Used to locate the HWNDViewContainer issuing the current Create. Only valid
// for the life of Create.
//
// This obviously assumes we only create HWNDViewContainers from the same
// thread, which is currently the case.
static HWNDViewContainer* instance_issuing_create = NULL;

///////////////////////////////////////////////////////////////////////////////
// FillLayout

FillLayout::FillLayout() {
}

FillLayout::~FillLayout() {
}

void FillLayout::Layout(View* host) {
  CRect bounds;
  host->GetViewContainer()->GetBounds(&bounds, false);
  if (host->GetChildViewCount() == 0)
    return;

  View* frame_view = host->GetChildViewAt(0);
  frame_view->SetBounds(CRect(CPoint(0, 0), bounds.Size()));
}

void FillLayout::GetPreferredSize(View* host, CSize* out) {
  DCHECK(host->GetChildViewCount() == 1);
  host->GetChildViewAt(0)->GetPreferredSize(out);
}

///////////////////////////////////////////////////////////////////////////////
// Window class tracking.

// static
const wchar_t* const HWNDViewContainer::kBaseClassName =
    L"Chrome_HWNDViewContainer_";

// Window class information used for registering unique windows.
struct ClassInfo {
  UINT style;
  HBRUSH background;

  explicit ClassInfo(int style)
      : style(style),
        background(NULL) {}

  // Compares two ClassInfos. Returns true if all members match.
  bool Equals(const ClassInfo& other) {
    return (other.style == style && other.background == background);
  }
};

// Represents a registered window class.
struct RegisteredClass {
  RegisteredClass(const ClassInfo& info,
                  const std::wstring& name,
                  ATOM atom)
      : info(info),
        name(name),
        atom(atom) {
  }

  // Info used to create the class.
  ClassInfo info;

  // The name given to the window.
  std::wstring name;

  // The ATOM returned from creating the window.
  ATOM atom;
};

typedef std::list<RegisteredClass> RegisteredClasses;

// The list of registered classes.
static RegisteredClasses* registered_classes = NULL;


///////////////////////////////////////////////////////////////////////////////
// HWNDViewContainer, public

HWNDViewContainer::HWNDViewContainer()
    : active_mouse_tracking_flags_(0),
      has_capture_(false),
      current_action_(FA_NONE),
      toplevel_(false),
      window_style_(0),
      window_ex_style_(kWindowDefaultExStyle),
      layered_(false),
      layered_alpha_(255),
      delete_on_destroy_(true),
      can_update_layered_window_(true),
      last_mouse_event_was_move_(false),
      is_mouse_down_(false),
      class_style_(CS_DBLCLKS),
      hwnd_(NULL),
      close_container_factory_(this) {
}

HWNDViewContainer::~HWNDViewContainer() {
  MessageLoopForUI::current()->RemoveObserver(this);
}

void HWNDViewContainer::Init(HWND parent,
                             const gfx::Rect& bounds,
                             bool has_own_focus_manager) {
  toplevel_ = parent == NULL;

  if (window_style_ == 0)
    window_style_ = toplevel_ ? kWindowDefaultStyle : kWindowDefaultChildStyle;

  // See if the style has been overridden.
  opaque_ = !(window_ex_style_ & WS_EX_TRANSPARENT);
  layered_ = !!(window_ex_style_ & WS_EX_LAYERED);

  // Force creation of the RootView if it hasn't been created yet.
  GetRootView();

  // Ensures the parent we have been passed is valid, otherwise CreateWindowEx
  // will fail.
  if (parent && !::IsWindow(parent)) {
    NOTREACHED() << "invalid parent window specified.";
    parent = NULL;
  }

  hwnd_ = CreateWindowEx(window_ex_style_, GetWindowClassName().c_str(), L"",
                         window_style_, bounds.x(), bounds.y(), bounds.width(),
                         bounds.height(), parent, NULL, NULL, this);
  DCHECK(hwnd_);
  // The window procedure should have set the data for us.
  DCHECK(win_util::GetWindowUserData(hwnd_) == this);

  root_view_->OnViewContainerCreated();

  if (has_own_focus_manager) {
    ChromeViews::FocusManager::CreateFocusManager(hwnd_, GetRootView());
  } else {
    // Subclass the window so we get the tab key messages when a view with no
    // associated native window is focused.
    FocusManager::InstallFocusSubclass(hwnd_, NULL);
  }

  // Sets the RootView as a property, so the automation can introspect windows.
  SetRootViewForHWND(hwnd_, root_view_.get());

  MessageLoopForUI::current()->AddObserver(this);

  // Windows special DWM window frame requires a special tooltip manager so
  // that window controls in Chrome windows don't flicker when you move your
  // mouse over them. See comment in aero_tooltip_manager.h.
  if (win_util::ShouldUseVistaFrame()) {
    tooltip_manager_.reset(new AeroTooltipManager(this, GetHWND()));
  } else {
    tooltip_manager_.reset(new TooltipManager(this, GetHWND()));
  }

  // This message initializes the window so that focus border are shown for
  // windows.
  ::SendMessage(GetHWND(),
                WM_CHANGEUISTATE,
                MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS),
                0);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(GetHWND(), NULL, 0);
}

void HWNDViewContainer::SetContentsView(View* view) {
  DCHECK(view && hwnd_) << "Can't be called until after the HWND is created!";
  // The ContentsView must be set up _after_ the window is created so that its
  // ViewContainer pointer is valid.
  root_view_->SetLayoutManager(new FillLayout);
  if (root_view_->GetChildViewCount() != 0)
    root_view_->RemoveAllChildViews(true);
  root_view_->AddChildView(view);

  // Manually size the window here to ensure the root view is laid out.
  RECT wr;
  GetWindowRect(&wr);
  ChangeSize(0, CSize(wr.right - wr.left, wr.bottom - wr.top));
}

///////////////////////////////////////////////////////////////////////////////
// ChromeViews::ViewContainer

void HWNDViewContainer::GetBounds(CRect *out, bool including_frame) const {
  if (including_frame) {
    GetWindowRect(out);
  } else {
    GetClientRect(out);

    POINT p = {0, 0};
    ::ClientToScreen(hwnd_, &p);

    out->left += p.x;
    out->top += p.y;
    out->right += p.x;
    out->bottom += p.y;
  }
}

void HWNDViewContainer::MoveToFront(bool should_activate) {
  int flags = SWP_NOMOVE | SWP_NOSIZE;
  if (!should_activate) {
    flags |= SWP_NOACTIVATE;
  }
  SetWindowPos(HWND_NOTOPMOST, 0, 0, 0, 0, flags);
}

HWND HWNDViewContainer::GetHWND() const {
  return hwnd_;
}

void HWNDViewContainer::PaintNow(const CRect& update_rect) {
  if (layered_) {
    PaintLayeredWindow();
  } else if (root_view_->NeedsPainting(false) && IsWindow()) {
    if (!opaque_ && GetParent()) {
      // We're transparent. Need to force painting to occur from our parent.
      CRect parent_update_rect = update_rect;
      POINT location_in_parent = { 0, 0 };
      ClientToScreen(hwnd_, &location_in_parent);
      ::ScreenToClient(GetParent(), &location_in_parent);
      parent_update_rect.OffsetRect(location_in_parent);
      ::RedrawWindow(GetParent(), parent_update_rect, NULL,
                     RDW_UPDATENOW | RDW_INVALIDATE | RDW_ALLCHILDREN);
    } else {
      RedrawWindow(hwnd_, update_rect, NULL,
                   RDW_UPDATENOW | RDW_INVALIDATE | RDW_ALLCHILDREN);
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

RootView* HWNDViewContainer::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

bool HWNDViewContainer::IsVisible() {
  return !!::IsWindowVisible(GetHWND());
}

bool HWNDViewContainer::IsActive() {
  return win_util::IsWindowActive(GetHWND());
}

TooltipManager* HWNDViewContainer::GetTooltipManager() {
  return tooltip_manager_.get();
}


void HWNDViewContainer::SetLayeredAlpha(BYTE layered_alpha) {
  layered_alpha_ = layered_alpha;

//  if (hwnd_)
//    UpdateWindowFromContents(contents_->getTopPlatformDevice().getBitmapDC());
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
RootView* HWNDViewContainer::FindRootView(HWND hwnd) {
  RootView* root_view =
    reinterpret_cast<RootView*>(GetProp(hwnd, kRootViewWindowProperty));
  if (root_view)
    return root_view;

  // Enumerate all children and check if they have a RootView.
  EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(&root_view));

  return root_view;
}

void HWNDViewContainer::Close() {
  // Let's hide ourselves right away.
  Hide();
  if (close_container_factory_.empty()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    MessageLoop::current()->PostTask(FROM_HERE,
        close_container_factory_.NewRunnableMethod(
            &HWNDViewContainer::CloseNow));
  }
}

void HWNDViewContainer::Hide() {
  // NOTE: Be careful not to activate any windows here (for example, calling
  // ShowWindow(SW_HIDE) will automatically activate another window).  This
  // code can be called while a window is being deactivated, and activating
  // another window will screw up the activation that is already in progress.
  SetWindowPos(NULL, 0, 0, 0, 0,
               SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
               SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
}

void HWNDViewContainer::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  if (IsWindow())
    DestroyWindow();
}

///////////////////////////////////////////////////////////////////////////////
// MessageLoop::Observer

void HWNDViewContainer::WillProcessMessage(const MSG& msg) {
}

void HWNDViewContainer::DidProcessMessage(const MSG& msg) {
  if (root_view_->NeedsPainting(true)) {
    PaintNow(root_view_->GetScheduledPaintRect());
  }
}

///////////////////////////////////////////////////////////////////////////////
// FocusTraversable

View* HWNDViewContainer::FindNextFocusableView(
    View* starting_view, bool reverse, Direction direction, bool dont_loop,
    FocusTraversable** focus_traversable, View** focus_traversable_view) {
  return root_view_->FindNextFocusableView(starting_view,
                                           reverse,
                                           direction,
                                           dont_loop,
                                           focus_traversable,
                                           focus_traversable_view);
}

FocusTraversable* HWNDViewContainer::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void HWNDViewContainer::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

View* HWNDViewContainer::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

void HWNDViewContainer::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

///////////////////////////////////////////////////////////////////////////////
// Message handlers

void HWNDViewContainer::OnCaptureChanged(HWND hwnd) {
  if (has_capture_) {
    if (is_mouse_down_)
      root_view_->ProcessMouseDragCanceled();
    is_mouse_down_ = false;
    has_capture_ = false;
  }
}

void HWNDViewContainer::OnClose() {
  // WARNING: this method is NOT called for all HWNDViewContainers. If you
  // need to do cleanup code before HWNDViewContainer is destroyed, put it
  // in OnDestroy.

  NotificationService::current()->Notify(
      NOTIFY_WINDOW_CLOSED, Source<HWND>(hwnd_),
      NotificationService::NoDetails());

  Close();
}

void HWNDViewContainer::OnDestroy() {
  root_view_->OnViewContainerDestroyed();

  RemoveProp(hwnd_, kRootViewWindowProperty);
}

LRESULT HWNDViewContainer::OnEraseBkgnd(HDC dc) {
  // This is needed for magical win32 flicker ju-ju
  return 1;
}

LRESULT HWNDViewContainer::OnGetObject(UINT uMsg, WPARAM w_param,
                                       LPARAM l_param) {
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

      CComPtr<IAccessible> accessibility_instance(instance);

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

      // Notify that an instance of IAccessible was allocated for m_hWnd
      ::NotifyWinEvent(EVENT_OBJECT_CREATE, GetHWND(), OBJID_CLIENT,
                       CHILDID_SELF);
    }

    // Create a reference to ViewAccessibility that MSAA will marshall
    // to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(accessibility_root_));
  }
  return reference_result;
}

void HWNDViewContainer::OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags) {
  KeyEvent event(Event::ET_KEY_PRESSED, c, rep_cnt, flags);
  root_view_->ProcessKeyEvent(event);
}

void HWNDViewContainer::OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags) {
  KeyEvent event(Event::ET_KEY_RELEASED, c, rep_cnt, flags);
  root_view_->ProcessKeyEvent(event);
}

void HWNDViewContainer::OnLButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, false);
}

void HWNDViewContainer::OnLButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_LBUTTON);
}

void HWNDViewContainer::OnLButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_LBUTTON, true);
}

void HWNDViewContainer::OnMButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, false);
}

void HWNDViewContainer::OnMButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_MBUTTON);
}

void HWNDViewContainer::OnMButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_MBUTTON, true);
}

LRESULT HWNDViewContainer::OnMouseActivate(HWND window,
                                           UINT hittest_code,
                                           UINT message) {
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

void HWNDViewContainer::OnMouseMove(UINT flags, const CPoint& point) {
  ProcessMouseMoved(point, flags, false);
}

LRESULT HWNDViewContainer::OnMouseLeave(UINT uMsg, WPARAM w_param,
                                        LPARAM l_param) {
  ProcessMouseExited();
  return 0;
}

LRESULT HWNDViewContainer::OnMouseWheel(UINT flags,
                                        short distance,
                                        const CPoint& point) {
  MouseWheelEvent e(distance,
                    point.x,
                    point.y,
                    Event::ConvertWindowsFlags(flags));
  return root_view_->ProcessMouseWheelEvent(e) ? 0 : 1;
}

LRESULT HWNDViewContainer::OnMouseRange(UINT msg,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  tooltip_manager_->OnMouse(msg, w_param, l_param);
  SetMsgHandled(FALSE);
  return 0;
}

void HWNDViewContainer::OnNCLButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void HWNDViewContainer::OnNCLButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void HWNDViewContainer::OnNCLButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT HWNDViewContainer::OnNCMouseLeave(UINT uMsg,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  ProcessMouseExited();
  return 0;
}

LRESULT HWNDViewContainer::OnNCMouseMove(UINT flags, const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  ProcessMouseMoved(temp, 0, true);

  // We need to process this message to stop Windows from drawing the window
  // controls as the mouse moves over the title bar area when the window is
  // maximized.
  return 0;
}

void HWNDViewContainer::OnNCRButtonDblClk(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void HWNDViewContainer::OnNCRButtonDown(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

void HWNDViewContainer::OnNCRButtonUp(UINT flags, const CPoint& point) {
  SetMsgHandled(FALSE);
}

LRESULT HWNDViewContainer::OnNotify(int w_param, NMHDR* l_param) {
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

void HWNDViewContainer::OnPaint(HDC dc) {
  root_view_->OnPaint(GetHWND());
}

void HWNDViewContainer::OnRButtonDown(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, false);
}

void HWNDViewContainer::OnRButtonUp(UINT flags, const CPoint& point) {
  ProcessMouseReleased(point, flags | MK_RBUTTON);
}

void HWNDViewContainer::OnRButtonDblClk(UINT flags, const CPoint& point) {
  ProcessMousePressed(point, flags | MK_RBUTTON, true);
}

LRESULT HWNDViewContainer::OnSettingChange(UINT msg,
                                           WPARAM w_param,
                                           LPARAM l_param) {
  if (toplevel_) {
    SetMsgHandled(FALSE);
    if (w_param != SPI_SETWORKAREA)
      return 0;  // Return value is effectively ignored in atlwin.h.

    AdjustWindowToFitScreenSize();
    SetMsgHandled(TRUE);
  }
  // Don't care, overridden by interested subclasses
  return 0;
}

void HWNDViewContainer::OnSize(UINT param, const CSize& size) {
  ChangeSize(param, size);
}

void HWNDViewContainer::OnThemeChanged() {
  // Notify NativeTheme.
  gfx::NativeTheme::instance()->CloseHandles();
}

void HWNDViewContainer::OnFinalMessage(HWND window) {
  if (delete_on_destroy_)
    delete this;
}

///////////////////////////////////////////////////////////////////////////////
// HWNDViewContainer, protected

void HWNDViewContainer::TrackMouseEvents(DWORD mouse_tracking_flags) {
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
    tme.hwndTrack = GetHWND();
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  } else if (mouse_tracking_flags != active_mouse_tracking_flags_) {
    TrackMouseEvents(active_mouse_tracking_flags_ | TME_CANCEL);
    TrackMouseEvents(mouse_tracking_flags);
  }
}

bool HWNDViewContainer::ProcessMousePressed(const CPoint& point,
                                            UINT flags,
                                            bool dbl_click) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED,
                           point.x,
                           point.y,
                           (dbl_click ? MouseEvent::EF_IS_DOUBLE_CLICK : 0) |
                           Event::ConvertWindowsFlags(flags));
  if (root_view_->OnMousePressed(mouse_pressed)) {
    is_mouse_down_ = true;
    if (!has_capture_) {
      SetCapture();
      has_capture_ = true;
      current_action_ = FA_FORWARDING;
    }
    return true;
  }
  return false;
}

void HWNDViewContainer::ProcessMouseDragged(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_drag(Event::ET_MOUSE_DRAGGED,
                        point.x,
                        point.y,
                        Event::ConvertWindowsFlags(flags));
  root_view_->OnMouseDragged(mouse_drag);
}

void HWNDViewContainer::ProcessMouseReleased(const CPoint& point, UINT flags) {
  last_mouse_event_was_move_ = false;
  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED,
                      point.x,
                      point.y,
                      Event::ConvertWindowsFlags(flags));
  // Release the capture first, that way we don't get confused if
  // OnMouseReleased blocks.
  if (has_capture_ && ReleaseCaptureOnMouseReleased()) {
    has_capture_ = false;
    current_action_ = FA_NONE;
    ReleaseCapture();
  }
  is_mouse_down_ = false;
  root_view_->OnMouseReleased(mouse_up, false);
}

void HWNDViewContainer::ProcessMouseMoved(const CPoint &point, UINT flags,
                                          bool is_nonclient) {
  // Windows only fires WM_MOUSELEAVE events if the application begins
  // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
  // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
  if (!has_capture_)
    TrackMouseEvents(is_nonclient ? TME_NONCLIENT | TME_LEAVE : TME_LEAVE);
  if (has_capture_ && is_mouse_down_) {
    ProcessMouseDragged(point, flags);
  } else {
    CPoint screen_loc = point;
    View::ConvertPointToScreen(root_view_.get(), &screen_loc);
    if (last_mouse_event_was_move_ && last_mouse_move_x_ == screen_loc.x &&
        last_mouse_move_y_ == screen_loc.y) {
      // Don't generate a mouse event for the same location as the last.
      return;
    }
    last_mouse_move_x_ = screen_loc.x;
    last_mouse_move_y_ = screen_loc.y;
    last_mouse_event_was_move_ = true;
    MouseEvent mouse_move(Event::ET_MOUSE_MOVED,
                          point.x,
                          point.y,
                          Event::ConvertWindowsFlags(flags));
    root_view_->OnMouseMoved(mouse_move);
  }
}

void HWNDViewContainer::ProcessMouseExited() {
  last_mouse_event_was_move_ = false;
  root_view_->ProcessOnMouseExited();
  // Reset our tracking flag so that future mouse movement over this
  // HWNDViewContainer results in a new tracking session.
  active_mouse_tracking_flags_ = 0;
}

void HWNDViewContainer::AdjustWindowToFitScreenSize() {
  // Desktop size has changed. Make sure we're still on screen.
  CRect wr;
  GetWindowRect(&wr);
  HMONITOR hmon = MonitorFromRect(&wr, MONITOR_DEFAULTTONEAREST);
  if (!hmon) {
    // No monitor available.
    return;
  }

  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hmon, &mi);
  gfx::Rect window_rect(wr);
  gfx::Rect monitor_rect(mi.rcWork);
  gfx::Rect new_window_rect = window_rect.AdjustToFit(monitor_rect);
  if (!new_window_rect.Equals(window_rect)) {
    // New position differs from last, resize window.
    ::SetWindowPos(GetHWND(),
                   0,
                   new_window_rect.x(),
                   new_window_rect.y(),
                   new_window_rect.width(),
                   new_window_rect.height(),
                   SWP_NOACTIVATE | SWP_NOZORDER);
  }
}

void HWNDViewContainer::ChangeSize(UINT size_param, const CSize& size) {
  CRect rect;
  if (layered_) {
    GetWindowRect(&rect);
    SizeContents(rect);
  } else {
    GetClientRect(&rect);
  }

  // Resizing changes the size of the view hierarchy and thus forces a
  // complete relayout.
  root_view_->SetBounds(CRect(CPoint(0,0), rect.Size()));
  root_view_->Layout();
  root_view_->SchedulePaint();

  if (layered_)
    PaintNow(rect);
}

RootView* HWNDViewContainer::CreateRootView() {
  return new RootView(this);
}

///////////////////////////////////////////////////////////////////////////////
// HWNDViewContainer, private:

void HWNDViewContainer::SizeContents(const CRect& window_rect) {
  contents_.reset(new ChromeCanvas(window_rect.Width(),
                                   window_rect.Height(),
                                   false));
}

void HWNDViewContainer::PaintLayeredWindow() {
  // Painting monkeys with our cliprect, so we need to save it so that the
  // call to UpdateLayeredWindow updates the entire window, not just the
  // cliprect.
  contents_->save(SkCanvas::kClip_SaveFlag);
  CRect dirty_rect = root_view_->GetScheduledPaintRect();
  contents_->ClipRectInt(
      dirty_rect.left, dirty_rect.top, dirty_rect.Width(), dirty_rect.Height());
  root_view_->ProcessPaint(contents_.get());
  contents_->restore();

  UpdateWindowFromContents(contents_->getTopPlatformDevice().getBitmapDC());
}

void HWNDViewContainer::UpdateWindowFromContents(HDC dib_dc) {
  DCHECK(layered_);
  if (can_update_layered_window_) {
    CRect wr;
    GetWindowRect(&wr);
    CSize size(wr.right - wr.left, wr.bottom - wr.top);
    CPoint zero_origin(0, 0);
    CPoint window_position = wr.TopLeft();

    BLENDFUNCTION blend = {AC_SRC_OVER, 0, layered_alpha_, AC_SRC_ALPHA};
    ::UpdateLayeredWindow(
        hwnd_, NULL, &window_position, &size, dib_dc, &zero_origin,
        RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
  }
}

std::wstring HWNDViewContainer::GetWindowClassName() {
  if (!registered_classes)
    registered_classes = new RegisteredClasses();
  ClassInfo class_info(initial_class_style());
  for (RegisteredClasses::iterator i = registered_classes->begin();
       i != registered_classes->end(); ++i) {
    if (class_info.Equals(i->info))
      return i->name;
  }

  // No class found, need to register one.
  static int registered_count = 0;
  std::wstring name =
      std::wstring(kBaseClassName) + IntToWString(registered_count++);
  WNDCLASSEX class_ex;
  class_ex.cbSize = sizeof(WNDCLASSEX);
  class_ex.style = class_info.style;
  class_ex.lpfnWndProc = &HWNDViewContainer::WndProc;
  class_ex.cbClsExtra = 0;
  class_ex.cbWndExtra = 0;
  class_ex.hInstance = NULL;
  class_ex.hIcon = LoadIcon(GetModuleHandle(L"chrome.dll"),
                            MAKEINTRESOURCE(IDR_MAINFRAME));
  class_ex.hCursor = LoadCursor(NULL, IDC_ARROW);
  class_ex.hbrBackground = reinterpret_cast<HBRUSH>(class_info.background + 1);
  class_ex.lpszMenuName = NULL;
  class_ex.lpszClassName = name.c_str();
  class_ex.hIconSm = class_ex.hIcon;
  ATOM atom = RegisterClassEx(&class_ex);
  DCHECK(atom);
  RegisteredClass registered_class(class_info, name, atom);
  registered_classes->push_back(registered_class);
  return name;
}

// static
LRESULT CALLBACK HWNDViewContainer::WndProc(HWND window,
                                            UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  if (message == WM_NCCREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l_param);
    HWNDViewContainer* vc =
        reinterpret_cast<HWNDViewContainer*>(cs->lpCreateParams);
    DCHECK(vc);
    win_util::SetWindowUserData(window, vc);
    vc->hwnd_ = window;
    return TRUE;
  }
  HWNDViewContainer* vc = reinterpret_cast<HWNDViewContainer*>(
      win_util::GetWindowUserData(window));
  if (!vc)
    return 0;
  LRESULT result = 0;
  if (!vc->ProcessWindowMessage(window, message, w_param, l_param, result))
    result = DefWindowProc(window, message, w_param, l_param);
  if (message == WM_NCDESTROY) {
    vc->hwnd_ = NULL;
    vc->OnFinalMessage(window);
  }
  return result;
}

}  // namespace ChromeViews

