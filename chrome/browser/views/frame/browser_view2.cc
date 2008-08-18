// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/views/frame/browser_view2.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents_container_view.h"
#include "chrome/browser/tabs/tab_strip.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/status_bubble.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"
#include "generated_resources.h"

// static
SkBitmap BrowserView2::default_favicon_;
SkBitmap BrowserView2::otr_avatar_;
static const int kToolbarTabStripVerticalOverlap = 3;
static const int kTabShadowSize = 2;
static const int kStatusBubbleHeight = 20;
static const int kStatusBubbleOffset = 2;
static const int kSeparationLineHeight = 1;
static const SkColor kSeparationLineColor = SkColorSetRGB(178, 178, 178);
static const wchar_t* kBrowserWindowKey = L"__BROWSER_WINDOW__";


static const struct { bool separator; int command; int label; } kMenuLayout[] = {
  { true, 0, 0 },
  { false, IDC_TASKMANAGER, IDS_TASKMANAGER },
  { true, 0, 0 },
  { false, 0, IDS_ENCODING },
  { false, 0, IDS_ZOOM },
  { false, IDC_PRINT, IDS_PRINT },
  { false, IDC_SAVEPAGE, IDS_SAVEPAGEAS },
  { false, IDC_FIND, IDS_FIND_IN_PAGE },
  { true, 0, 0 },
  { false, IDC_PASTE, IDS_PASTE },
  { false, IDC_COPY, IDS_COPY },
  { false, IDC_CUT, IDS_CUT },
  { true, 0, 0 },
  { false, IDC_NEWTAB, IDS_APP_MENU_NEW_WEB_PAGE },
  { false, IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB },
  { false, IDC_COPY_URL, IDS_APP_MENU_COPY_URL },
  { false, IDC_DUPLICATE, IDS_APP_MENU_DUPLICATE },
  { true, 0, 0 },
  { false, IDC_RELOAD, IDS_APP_MENU_RELOAD },
  { false, IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD },
  { false, IDC_BACK, IDS_CONTENT_CONTEXT_BACK }
};

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, public:

BrowserView2::BrowserView2(Browser* browser)
    : ClientView(NULL, NULL),
      frame_(NULL),
      browser_(browser),
      active_bookmark_bar_(NULL),
      active_info_bar_(NULL),
      active_download_shelf_(NULL),
      toolbar_(NULL),
      contents_container_(NULL),
      initialized_(false),
      can_drop_(false),
      forwarding_to_tab_strip_(false) {
  InitClass();
  show_bookmark_bar_pref_.Init(prefs::kShowBookmarkBar,
                               browser_->profile()->GetPrefs(), this);
  browser_->tabstrip_model()->AddObserver(this);
}

BrowserView2::~BrowserView2() {
  browser_->tabstrip_model()->RemoveObserver(this);
}

void BrowserView2::WindowMoved() {
  status_bubble_->Reposition();

  // Close the omnibox popup, if any.
  if (GetLocationBarView())
    GetLocationBarView()->location_entry()->ClosePopup();
}

gfx::Rect BrowserView2::GetToolbarBounds() const {
  CRect bounds;
  toolbar_->GetBounds(&bounds);
  return gfx::Rect(bounds);
}

gfx::Rect BrowserView2::GetClientAreaBounds() const {
  CRect bounds;
  contents_container_->GetBounds(&bounds);
  bounds.OffsetRect(GetX(), GetY());
  return gfx::Rect(bounds);
}

int BrowserView2::GetTabStripHeight() const {
  return tabstrip_->GetPreferredHeight();
}

bool BrowserView2::IsToolbarVisible() const {
  return SupportsWindowFeature(FEATURE_TOOLBAR) ||
         SupportsWindowFeature(FEATURE_LOCATIONBAR);
}

bool BrowserView2::IsTabStripVisible() const {
  return SupportsWindowFeature(FEATURE_TABSTRIP);
}

bool BrowserView2::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView2::AcceleratorPressed(
    const ChromeViews::Accelerator& accelerator) {
  DCHECK(accelerator_table_.get());
  std::map<ChromeViews::Accelerator, int>::const_iterator iter =
      accelerator_table_->find(accelerator);
  DCHECK(iter != accelerator_table_->end());

  int command_id = iter->second;
  if (browser_->SupportsCommand(command_id) &&
      browser_->IsCommandEnabled(command_id)) {
    browser_->ExecuteCommand(command_id);
    return true;
  }
  return false;
}

bool BrowserView2::GetAccelerator(int cmd_id,
                                  ChromeViews::Accelerator* accelerator) {
  std::map<ChromeViews::Accelerator, int>::iterator it =
      accelerator_table_->begin();
  for (; it != accelerator_table_->end(); ++it) {
    if (it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  return false;
}

bool BrowserView2::SystemCommandReceived(UINT notification_code,
                                         const gfx::Point& point) {
  bool handled = false;

  if (browser_->SupportsCommand(notification_code)) {
    browser_->ExecuteCommand(notification_code);
    handled = true;
  }

  return handled;
}

void BrowserView2::AddViewToDropList(ChromeViews::View* view) {
  dropable_views_.insert(view);
}

bool BrowserView2::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  if (BrowserList::IsShowingAppModalDialog()) {
    if (browser_ != BrowserList::GetLastActive()) {
      BrowserList::GetLastActive()->window()->FlashFrame();
      BrowserList::GetLastActive()->MoveToFront(true);
    }
    AppModalDialogQueue::ActivateModalDialog();
    return true;
  }
  return false;
}

void BrowserView2::ActivationChanged(bool activated) {
  // The Browser wants to update the BrowserList to let it know it's now
  // active.
  browser_->WindowActivationChanged(activated);
}

TabContents* BrowserView2::GetSelectedTabContents() const {
  return browser_->GetSelectedTabContents();
}

SkBitmap BrowserView2::GetOTRAvatarIcon() {
  if (otr_avatar_.isNull()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    otr_avatar_ = *rb.GetBitmapNamed(IDR_OTR_ICON);
  }
  return otr_avatar_;
}

void BrowserView2::PrepareToRunSystemMenu(Menu* menu) {
  int insertion_index = std::max(0, menu->ItemCount() - 1);
  system_menu_.reset(menu);
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (browser_->GetType() == BrowserType::TABBED_BROWSER) {
    system_menu_->AddSeparator(insertion_index);
    system_menu_->AddMenuItemWithLabel(insertion_index, IDC_TASKMANAGER,
                                       l10n_util::GetString(IDS_TASKMANAGER));
    // If it's a regular browser window with tabs, we don't add any more items,
    // since it already has menus (Page, Chrome).
    return;
  } else {
    BuildMenuForTabStriplessWindow(system_menu_.get(), insertion_index);
  }
}

void BrowserView2::SystemMenuEnded() {
  system_menu_.reset();
}

bool BrowserView2::SupportsWindowFeature(WindowFeature feature) const {
  return !!(FeaturesForBrowserType(browser_->GetType()) & feature);
}

// static
unsigned int BrowserView2::FeaturesForBrowserType(BrowserType::Type type) {
  unsigned int features = FEATURE_INFOBAR | FEATURE_DOWNLOADSHELF;
  if (type == BrowserType::TABBED_BROWSER)
    features |= FEATURE_TABSTRIP | FEATURE_TOOLBAR | FEATURE_BOOKMARKBAR;
  if (type != BrowserType::APPLICATION)
    features |= FEATURE_LOCATIONBAR;
  if (type != BrowserType::TABBED_BROWSER)
    features |= FEATURE_TITLEBAR;
  return features;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, BrowserWindow implementation:

void BrowserView2::Init() {
  // Stow a pointer to this object onto the window handle so that we can get
  // at it later when all we have is a HWND.
  SetProp(GetViewContainer()->GetHWND(), kBrowserWindowKey, this);

  LoadAccelerators();
  SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  tabstrip_ = new TabStrip(browser_->tabstrip_model());
  tabstrip_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TABSTRIP));
  AddChildView(tabstrip_);

  toolbar_ = new BrowserToolbarView(browser_->controller(), browser_.get());
  AddChildView(toolbar_);
  toolbar_->SetID(VIEW_ID_TOOLBAR);
  toolbar_->Init(browser_->profile());
  toolbar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TOOLBAR));

  contents_container_ = new TabContentsContainerView;
  set_contents_view(contents_container_);
  AddChildView(contents_container_);

  status_bubble_.reset(new StatusBubble(GetViewContainer()));
}

void BrowserView2::Show(int command, bool adjust_to_fit) {
  frame_->GetWindow()->Show();
}

void BrowserView2::Close() {
  frame_->GetWindow()->Close();
}

void* BrowserView2::GetPlatformID() {
  return GetViewContainer()->GetHWND();
}

TabStrip* BrowserView2::GetTabStrip() const {
  return tabstrip_;
}

StatusBubble* BrowserView2::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserView2::SelectedTabToolbarSizeChanged(bool is_animating) {
  if (is_animating) {
    contents_container_->set_fast_resize(true);
    UpdateUIForContents(browser_->GetSelectedTabContents());
    contents_container_->set_fast_resize(false);
  } else {
    UpdateUIForContents(browser_->GetSelectedTabContents());
    contents_container_->UpdateHWNDBounds();
  }
}

void BrowserView2::UpdateTitleBar() {
  frame_->GetWindow()->UpdateWindowTitle();
  frame_->GetWindow()->UpdateWindowIcon();
}

void BrowserView2::Activate() {
  frame_->GetWindow()->Activate();
}

void BrowserView2::FlashFrame() {
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = frame_->GetWindow()->GetHWND();
  fwi.dwFlags = FLASHW_ALL;
  fwi.uCount = 4;
  fwi.dwTimeout = 0;
  FlashWindowEx(&fwi);
}

void BrowserView2::ContinueDetachConstrainedWindowDrag(
    const gfx::Point& mouse_point,
    int frame_component) {
  HWND vc_hwnd = GetViewContainer()->GetHWND();
  if (frame_component == HTCLIENT) {
    // If the user's mouse was over the content area of the popup when they
    // clicked down, we need to re-play the mouse down event so as to actually
    // send the click to the renderer. If we don't do this, the user needs to
    // click again once the window is detached to interact.
    HWND inner_hwnd = browser_->GetSelectedTabContents()->GetContentHWND();
    POINT window_point = mouse_point.ToPOINT();
    MapWindowPoints(HWND_DESKTOP, inner_hwnd, &window_point, 1);
    PostMessage(inner_hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
                MAKELPARAM(window_point.x, window_point.y));
  } else if (frame_component != HTNOWHERE) {
    // The user's mouse is already moving, and the left button is down, but we
    // need to start moving this frame, so we _post_ it a NCLBUTTONDOWN message
    // with the corresponding frame component as supplied by the constrained
    // window where the user clicked. This tricks Windows into believing the
    // user just started performing that operation on the newly created window.
    // All the frame moving and sizing is then handled automatically by
    // Windows. We use PostMessage because we need to return to the message
    // loop first for Windows' built in moving/sizing to be triggered.
    POINTS pts;
    pts.x = mouse_point.x();
    pts.y = mouse_point.y();
    PostMessage(vc_hwnd, WM_NCLBUTTONDOWN, frame_component,
                reinterpret_cast<LPARAM>(&pts));
    // Also make sure the right cursor for the action is set.
    PostMessage(vc_hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(vc_hwnd),
                frame_component);
  }
}

void BrowserView2::SizeToContents(const gfx::Rect& contents_bounds) {
  frame_->SizeToContents(contents_bounds);
}

void BrowserView2::SetAcceleratorTable(
    std::map<ChromeViews::Accelerator, int>* accelerator_table) {
  accelerator_table_.reset(accelerator_table);
}

void BrowserView2::ValidateThrobber() {
  frame_->GetWindow()->UpdateWindowIcon();
}

gfx::Rect BrowserView2::GetNormalBounds() {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool ret = !!GetWindowPlacement(frame_->GetWindow()->GetHWND(), &wp);
  DCHECK(ret);
  return gfx::Rect(wp.rcNormalPosition);
}

bool BrowserView2::IsMaximized() {
  return frame_->GetWindow()->IsMaximized();
}

gfx::Rect BrowserView2::GetBoundsForContentBounds(
    const gfx::Rect content_rect) {
  return frame_->GetWindowBoundsForClientBounds(content_rect);
}

void BrowserView2::InfoBubbleShowing() {
  frame_->GetWindow()->DisableInactiveRendering(true);
}

void BrowserView2::InfoBubbleClosing() {
  frame_->GetWindow()->DisableInactiveRendering(false);
}

ToolbarStarToggle* BrowserView2::GetStarButton() const {
  return toolbar_->star_button();
}

LocationBarView* BrowserView2::GetLocationBarView() const {
  return toolbar_->GetLocationBarView();
}

GoButton* BrowserView2::GetGoButton() const {
  return toolbar_->GetGoButton();
}

BookmarkBarView* BrowserView2::GetBookmarkBarView() {
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!bookmark_bar_view_.get()) {
    bookmark_bar_view_.reset(new BookmarkBarView(current_tab->profile(),
                                                 browser_.get()));
    bookmark_bar_view_->SetParentOwned(false);
  } else {
    bookmark_bar_view_->SetProfile(current_tab->profile());
  }
  bookmark_bar_view_->SetPageNavigator(current_tab);
  return bookmark_bar_view_.get();
}

BrowserView* BrowserView2::GetBrowserView() const {
  return NULL;
}

void BrowserView2::UpdateToolbar(TabContents* contents,
                                 bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView2::FocusToolbar() {
  toolbar_->RequestFocus();
}

void BrowserView2::DestroyBrowser() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, NotificationObserver implementation:

void BrowserView2::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  if (type == NOTIFY_PREF_CHANGED &&
      *Details<std::wstring>(details).ptr() == prefs::kShowBookmarkBar) {
    if (MaybeShowBookmarkBar(browser_->GetSelectedTabContents()))
      Layout();
  } else {
    NOTREACHED() << "Got a notification we didn't register for!";
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, TabStripModelObserver implementation:

void BrowserView2::TabDetachedAt(TabContents* contents, int index) {
  if (contents == browser_->GetSelectedTabContents()) {
    // We need to reset the current tab contents to NULL before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected TabContents when it is removed.
    contents_container_->SetTabContents(NULL);
  }
}

void BrowserView2::TabSelectedAt(TabContents* old_contents,
                                 TabContents* new_contents,
                                 int index,
                                 bool user_gesture) {
  DCHECK(old_contents != new_contents);

  if (old_contents)
    old_contents->StoreFocus();

  // Tell the frame what happened so that the TabContents gets resized, etc.
  contents_container_->SetTabContents(new_contents);

  if (BrowserList::GetLastActive() == browser_)
    new_contents->RestoreFocus();

  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);
}

void BrowserView2::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, ChromeViews::WindowDelegate implementation:

bool BrowserView2::CanResize() const {
  return true;
}

bool BrowserView2::CanMaximize() const {
  return true;
}

bool BrowserView2::IsModal() const {
  return false;
}

std::wstring BrowserView2::GetWindowTitle() const {
  return browser_->GetCurrentPageTitle();
}

ChromeViews::View* BrowserView2::GetInitiallyFocusedView() const {
  return GetLocationBarView();
}

bool BrowserView2::ShouldShowWindowTitle() const {
  return SupportsWindowFeature(FEATURE_TITLEBAR);
}

SkBitmap BrowserView2::GetWindowIcon() {
  if (browser_->GetType() == BrowserType::APPLICATION) {
    SkBitmap favicon = browser_->GetCurrentPageIcon();
    if (favicon.isNull())
      return default_favicon_;
    return favicon;
  }
  return SkBitmap();
}

bool BrowserView2::ShouldShowWindowIcon() const {
  return SupportsWindowFeature(FEATURE_TITLEBAR);
}

void BrowserView2::ExecuteWindowsCommand(int command_id) {
  if (browser_->SupportsCommand(command_id) &&
      browser_->IsCommandEnabled(command_id)) {
    browser_->ExecuteCommand(command_id);
  }
}

void BrowserView2::SaveWindowPosition(const CRect& bounds,
                                      bool maximized,
                                      bool always_on_top) {
  browser_->SaveWindowPosition(gfx::Rect(bounds), maximized);
}

bool BrowserView2::RestoreWindowPosition(CRect* bounds,
                                         bool* maximized,
                                         bool* always_on_top) {
  DCHECK(bounds && maximized && always_on_top);
  // TODO(beng): (http://b/1317622) Make these functions take gfx::Rects.
  gfx::Rect b;
  browser_->RestoreWindowPosition(&b, maximized);
  *bounds = b.ToRECT();

  // We return true because we can _always_ locate reasonable bounds using the
  // WindowSizer, and we don't want to trigger the Window's built-in "size to
  // default" handling because the browser window has no default preferred
  // size.
  return true;
}

void BrowserView2::WindowClosing() {
}

ChromeViews::View* BrowserView2::GetContentsView() {
  return contents_container_;
}

ChromeViews::ClientView* BrowserView2::CreateClientView(
    ChromeViews::Window* window) {
  set_window(window);
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, ChromeViews::ClientView overrides:

bool BrowserView2::CanClose() const {
  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_->IsDragSessionActive())
    return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->GetWindow()->Hide();
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  /*
  // TODO(beng): for some reason, this won't compile. Figure it out.
  NotificationService::current()->Notify(
      NOTIFY_WINDOW_CLOSED, Source<HWND>(frame_->GetWindow()->GetHWND()),
      NotificationService::NoDetails());
      */
  return true;
}

int BrowserView2::NonClientHitTest(const gfx::Point& point) {
  // First learn about the kind of frame we dwell within...
  WINDOWINFO wi;
  wi.cbSize = sizeof(wi);
  GetWindowInfo(frame_->GetWindow()->GetHWND(), &wi);

  // Since we say that our client area extends to the top of the window (in
  // the frame's WM_NCHITTEST handler.
  CRect lb;
  GetLocalBounds(&lb, true);
  if (lb.PtInRect(point.ToPOINT())) {
    if (point.y() < static_cast<int>(wi.cyWindowBorders))
      return HTTOP;
  }

  CPoint point_in_view_coords(point.ToPOINT());
  View::ConvertPointToView(GetParent(), this, &point_in_view_coords);
  if (IsTabStripVisible() && tabstrip_->HitTest(point_in_view_coords) &&
      tabstrip_->CanProcessInputEvents()) {
    ChromeViews::Window* window = frame_->GetWindow();
    // The top few pixels of the TabStrip are a drop-shadow - as we're pretty
    // starved of dragable area, let's give it to window dragging (this also
    // makes sense visually).
    if (!window->IsMaximized() && point_in_view_coords.y < kTabShadowSize)
      return HTCAPTION;

    if (tabstrip_->PointIsWithinWindowCaption(point_in_view_coords))
      return HTCAPTION;

    return HTCLIENT;
  }

  // If the point's y coordinate is below the top of the toolbar and otherwise
  // within the bounds of this view, the point is considered to be within the
  // client area.
  CRect bounds;
  GetBounds(&bounds);
  bounds.top += toolbar_->GetY();
  if (gfx::Rect(bounds).Contains(point.x(), point.y()))
    return HTCLIENT;

  // If the point is somewhere else, delegate to the default implementation.
  return ClientView::NonClientHitTest(point);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView2, ChromeViews::View overrides:

void BrowserView2::Paint(ChromeCanvas* canvas) {
  //canvas->FillRectInt(SK_ColorRED, 0, 0, GetWidth(), GetHeight());
}

void BrowserView2::Layout() {
  int top = LayoutTabStrip();
  top = LayoutToolbar(top);
  top = LayoutBookmarkAndInfoBars(top);
  int bottom = LayoutDownloadShelf();
  LayoutTabContents(top, bottom);
  LayoutStatusBubble(bottom);
  SchedulePaint();
}

void BrowserView2::DidChangeBounds(const CRect& previous,
                                   const CRect& current) {
  Layout();
}

void BrowserView2::ViewHierarchyChanged(bool is_add,
                                        ChromeViews::View* parent,
                                        ChromeViews::View* child) {
  if (is_add && child == this && GetViewContainer() && !initialized_) {
    Init();
    initialized_ = true;
  }
  if (!is_add)
    dropable_views_.erase(child);
}

bool BrowserView2::CanDrop(const OSExchangeData& data) {
  can_drop_ = (tabstrip_->IsVisible() && !tabstrip_->IsAnimating() &&
               data.HasURL());
  return can_drop_;
}

void BrowserView2::OnDragEntered(const ChromeViews::DropTargetEvent& event) {
  if (can_drop_ && ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event));
    tabstrip_->OnDragEntered(*mapped_event.get());
  }
}

int BrowserView2::OnDragUpdated(const ChromeViews::DropTargetEvent& event) {
  if (can_drop_) {
    if (ShouldForwardToTabStrip(event)) {
      scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
      if (!forwarding_to_tab_strip_) {
        tabstrip_->OnDragEntered(*mapped_event.get());
        forwarding_to_tab_strip_ = true;
      }
      return tabstrip_->OnDragUpdated(*mapped_event.get());
    } else if (forwarding_to_tab_strip_) {
      forwarding_to_tab_strip_ = false;
      tabstrip_->OnDragExited();
    }
  }
  return DragDropTypes::DRAG_NONE;
}

void BrowserView2::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip_->OnDragExited();
  }
}

int BrowserView2::OnPerformDrop(const ChromeViews::DropTargetEvent& event) {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    scoped_ptr<ChromeViews::DropTargetEvent> mapped_event(
          MapEventToTabStrip(event));
    return tabstrip_->OnPerformDrop(*mapped_event.get());
  }
  return DragDropTypes::DRAG_NONE;
}


///////////////////////////////////////////////////////////////////////////////
// BrowserView2, private:

bool BrowserView2::ShouldForwardToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  if (!tabstrip_->IsVisible())
    return false;

  const int tab_y = tabstrip_->GetY();
  const int tab_height = tabstrip_->GetHeight();
  if (event.GetY() >= tab_y + tab_height)
    return false;

  if (event.GetY() >= tab_y)
    return true;

  // Mouse isn't over the tab strip. Only forward if the mouse isn't over
  // another view on the tab strip or is over a view we were told the user can
  // drop on.
  ChromeViews::View* view_over_mouse =
      GetViewForPoint(CPoint(event.GetX(), event.GetY()));
  return (view_over_mouse == this || view_over_mouse == tabstrip_ ||
          dropable_views_.find(view_over_mouse) != dropable_views_.end());
}

ChromeViews::DropTargetEvent* BrowserView2::MapEventToTabStrip(
    const ChromeViews::DropTargetEvent& event) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToView(this, tabstrip_, &tab_strip_loc);
  return new ChromeViews::DropTargetEvent(event.GetData(), tab_strip_loc.x(),
                                          tab_strip_loc.y(),
                                          event.GetSourceOperations());
}

int BrowserView2::LayoutTabStrip() {
  if (IsTabStripVisible()) {
    gfx::Rect tabstrip_bounds = frame_->GetBoundsForTabStrip(tabstrip_);
    tabstrip_->SetBounds(tabstrip_bounds.x(), tabstrip_bounds.y(),
                         tabstrip_bounds.width(), tabstrip_bounds.height());
    return tabstrip_bounds.bottom();
  }
  return 0;
}

int BrowserView2::LayoutToolbar(int top) {
  if (IsToolbarVisible()) {
    CSize ps;
    toolbar_->GetPreferredSize(&ps);
    int toolbar_y = top - kToolbarTabStripVerticalOverlap;
    toolbar_->SetBounds(0, toolbar_y, GetWidth(), ps.cy);
    return toolbar_y + ps.cy;
  }
  toolbar_->SetVisible(false);
  return top;
}

int BrowserView2::LayoutBookmarkAndInfoBars(int top) {
  if (SupportsWindowFeature(FEATURE_BOOKMARKBAR)) {
    // If we have an Info-bar showing, and we're showing the New Tab Page, and
    // the Bookmark bar isn't visible on all tabs, then we need to show the
    // Info bar _above_ the Bookmark bar, since the Bookmark bar is styled to
    // look like it's part of the New Tab Page...
    if (active_info_bar_ && active_bookmark_bar_ &&
        bookmark_bar_view_->IsNewTabPage() &&
        !bookmark_bar_view_->IsAlwaysShown()) {
      top = LayoutInfoBar(top);
      return LayoutBookmarkBar(top);
    }
    // Otherwise, Bookmark bar first, Info bar second.
    top = LayoutBookmarkBar(top);
  }
  return LayoutInfoBar(top);
}

int BrowserView2::LayoutBookmarkBar(int top) {
  if (SupportsWindowFeature(FEATURE_BOOKMARKBAR) && active_bookmark_bar_) {
    CSize ps;
    active_bookmark_bar_->GetPreferredSize(&ps);
    if (!active_info_bar_ || show_bookmark_bar_pref_.GetValue())
      top -= kSeparationLineHeight;
    active_bookmark_bar_->SetBounds(0, top, GetWidth(), ps.cy);
    top += ps.cy;
  }  
  return top;
}
int BrowserView2::LayoutInfoBar(int top) {
  if (SupportsWindowFeature(FEATURE_INFOBAR) && active_info_bar_) {
    CSize ps;
    active_info_bar_->GetPreferredSize(&ps);
    active_info_bar_->SetBounds(0, top, GetWidth(), ps.cy);
    top += ps.cy;
    if (SupportsWindowFeature(FEATURE_BOOKMARKBAR) && active_bookmark_bar_ &&
        !show_bookmark_bar_pref_.GetValue()) {
      top -= kSeparationLineHeight;
    }
  }
  return top;
}

void BrowserView2::LayoutTabContents(int top, int bottom) {
  contents_container_->SetBounds(0, top, GetWidth(), bottom - top);
}

int BrowserView2::LayoutDownloadShelf() {
  int bottom = GetHeight();
  if (SupportsWindowFeature(FEATURE_DOWNLOADSHELF) && active_download_shelf_) {
    CSize ps;
    active_download_shelf_->GetPreferredSize(&ps);
    active_download_shelf_->SetBounds(0, bottom - ps.cy, GetWidth(), ps.cy);
    bottom -= ps.cy;
  }
  return bottom;
}

void BrowserView2::LayoutStatusBubble(int top) {
  int status_bubble_y =
      top - kStatusBubbleHeight + kStatusBubbleOffset + GetY();
  status_bubble_->SetBounds(kStatusBubbleOffset, status_bubble_y,
                            GetWidth() / 3, kStatusBubbleHeight);
}

bool BrowserView2::MaybeShowBookmarkBar(TabContents* contents) {
  ChromeViews::View* new_bookmark_bar_view = NULL;
  if (SupportsWindowFeature(FEATURE_BOOKMARKBAR) && contents &&
      (contents->IsBookmarkBarAlwaysVisible() ||
       show_bookmark_bar_pref_.GetValue())) {
    new_bookmark_bar_view = GetBookmarkBarView();
  }
  return UpdateChildViewAndLayout(new_bookmark_bar_view,
                                  &active_bookmark_bar_);
}

bool BrowserView2::MaybeShowInfoBar(TabContents* contents) {
  ChromeViews::View* new_info_bar = NULL;
  if (contents && contents->IsInfoBarVisible())
    new_info_bar = contents->GetInfoBarView();
  return UpdateChildViewAndLayout(new_info_bar, &active_info_bar_);
}

bool BrowserView2::MaybeShowDownloadShelf(TabContents* contents) {
  ChromeViews::View* new_shelf = NULL;
  if (contents && contents->IsDownloadShelfVisible())
    new_shelf = contents->GetDownloadShelfView();
  return UpdateChildViewAndLayout(new_shelf, &active_download_shelf_);
}

void BrowserView2::UpdateUIForContents(TabContents* contents) {
  bool needs_layout = MaybeShowBookmarkBar(contents);
  needs_layout |= MaybeShowInfoBar(contents);
  needs_layout |= MaybeShowDownloadShelf(contents);
  if (needs_layout)
    Layout();
}

bool BrowserView2::UpdateChildViewAndLayout(ChromeViews::View* new_view,
                                            ChromeViews::View** old_view) {
  DCHECK(old_view);
  if (*old_view == new_view) {
    // The views haven't changed, if the views pref changed schedule a layout.
    if (new_view) {
      CSize pref_size;
      new_view->GetPreferredSize(&pref_size);
      if (pref_size.cy != new_view->GetHeight())
        return true;
    }
    return false;
  }

  // The views differ, and one may be null (but not both). Remove the old
  // view (if it non-null), and add the new one (if it is non-null). If the
  // height has changed, schedule a layout, otherwise reuse the existing
  // bounds to avoid scheduling a layout.

  int current_height = 0;
  if (*old_view) {
    current_height = (*old_view)->GetHeight();
    RemoveChildView(*old_view);
  }

  int new_height = 0;
  if (new_view) {
    CSize preferred_size;
    new_view->GetPreferredSize(&preferred_size);
    new_height = preferred_size.cy;
    AddChildView(new_view);
  }
  bool changed = false;
  if (new_height != current_height) {
    changed = true;
  } else if (new_view && *old_view) {
    // The view changed, but the new view wants the same size, give it the
    // bounds of the last view and have it repaint.
    CRect last_bounds;
    (*old_view)->GetBounds(&last_bounds);
    new_view->SetBounds(last_bounds.left, last_bounds.top,
                        last_bounds.Width(), last_bounds.Height());
    new_view->SchedulePaint();
  } else if (new_view) {
    DCHECK(new_height == 0);
    // The heights are the same, but the old view is null. This only happens
    // when the height is zero. Zero out the bounds.
    new_view->SetBounds(0, 0, 0, 0);
  }
  *old_view = new_view;
  return changed;
}

void BrowserView2::LoadAccelerators() {
  HACCEL accelerator_table = AtlLoadAccelerators(IDR_MAINFRAME);
  DCHECK(accelerator_table);

  // We have to copy the table to access its contents.
  int count = CopyAcceleratorTable(accelerator_table, 0, 0);
  if (count == 0) {
    // Nothing to do in that case.
    return;
  }

  ACCEL* accelerators = static_cast<ACCEL*>(malloc(sizeof(ACCEL) * count));
  CopyAcceleratorTable(accelerator_table, accelerators, count);

  ChromeViews::FocusManager* focus_manager =
    ChromeViews::FocusManager::GetFocusManager(GetViewContainer()->GetHWND());
  DCHECK(focus_manager);

  // Let's build our own accelerator table.
  accelerator_table_.reset(new std::map<ChromeViews::Accelerator, int>);
  for (int i = 0; i < count; ++i) {
    bool alt_down = (accelerators[i].fVirt & FALT) == FALT;
    bool ctrl_down = (accelerators[i].fVirt & FCONTROL) == FCONTROL;
    bool shift_down = (accelerators[i].fVirt & FSHIFT) == FSHIFT;
    ChromeViews::Accelerator accelerator(accelerators[i].key,
      shift_down, ctrl_down, alt_down);
    (*accelerator_table_)[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, this);
  }

  // We don't need the Windows accelerator table anymore.
  free(accelerators);
}

void BrowserView2::BuildMenuForTabStriplessWindow(Menu* menu,
                                                  int insertion_index) {
  for (int i = 0; i < arraysize(kMenuLayout); ++i) {
    if (kMenuLayout[i].separator) {
      menu->AddSeparator(insertion_index);
    } else {
      int command = kMenuLayout[i].command;

      menu->AddMenuItemWithLabel(insertion_index, command,
                                 l10n_util::GetString(kMenuLayout[i].label));

      // |command| can be zero on submenu items (IDS_ENCODING,
      // IDS_ZOOM) and on separators.
      if (command != 0) {
        menu->EnableMenuItemAt(
            insertion_index,
            browser_->IsCommandEnabled(command));
      }
    }
  }
}

// static
void BrowserView2::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    initialized = true;
  }
}
