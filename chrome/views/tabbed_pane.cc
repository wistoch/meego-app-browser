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

#include "chrome/views/tabbed_pane.h"

#include <vssym32.h>

#include "base/gfx/native_theme.h"
#include "base/logging.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/views/background.h"
#include "chrome/views/hwnd_view_container.h"
#include "skia/include/SkColor.h"
#include "base/gfx/skia_utils.h"

namespace ChromeViews {

// A background object that paints the tab panel background which may be
// rendered by the system visual styles system.
class TabBackground : public Background {
 public:
  explicit TabBackground() {
    // TMT_FILLCOLORHINT returns a color value that supposedly
    // approximates the texture drawn by PaintTabPanelBackground.
    SkColor tab_page_color =
        gfx::NativeTheme::instance()->GetThemeColorWithDefault(
            gfx::NativeTheme::TAB, TABP_BODY, 0, TMT_FILLCOLORHINT,
            COLOR_3DFACE);
    SetNativeControlColor(tab_page_color);
  }
  virtual ~TabBackground() {}

  virtual void Paint(ChromeCanvas* canvas, View* view) const {
    HDC dc = canvas->beginPlatformPaint();
    RECT r = {0, 0, view->GetWidth(), view->GetHeight()};
    gfx::NativeTheme::instance()->PaintTabPanelBackground(dc, &r);
    canvas->endPlatformPaint();
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TabBackground);
};

TabbedPane::TabbedPane() : content_window_(NULL), listener_(NULL) {
}

TabbedPane::~TabbedPane() {
  // We own the tab views, let's delete them.
  STLDeleteContainerPointers(tab_views_.begin(), tab_views_.end());
}

void TabbedPane::SetListener(Listener* listener) {
  listener_ = listener;
}

void TabbedPane::AddTab(const std::wstring& title, View* contents) {
  AddTabAtIndex(static_cast<int>(tab_views_.size()), title, contents, true);
}

void TabbedPane::AddTabAtIndex(int index,
                               const std::wstring& title,
                               View* contents,
                               bool select_if_first_tab) {
  DCHECK(index <= static_cast<int>(tab_views_.size()));
  contents->SetParentOwned(false);
  tab_views_.insert(tab_views_.begin() + index, contents);

  TCITEM tcitem;
  tcitem.mask = TCIF_TEXT;

  // If the locale is RTL, we set the TCIF_RTLREADING so that BiDi text is
  // rendered properly on the tabs.
  if (UILayoutIsRightToLeft()) {
    tcitem.mask |= TCIF_RTLREADING;
  }

  tcitem.pszText = const_cast<wchar_t*>(title.c_str());
  int result = TabCtrl_InsertItem(tab_control_, index, &tcitem);
  DCHECK(result != -1);

  if (!contents->GetBackground()) {
    contents->SetBackground(new TabBackground);
  }

  if (tab_views_.size() == 1 && select_if_first_tab) {
    // If this is the only tab displayed, make sure the contents is set.
    content_window_->GetRootView()->AddChildView(contents);
  }

  // The newly added tab may have made the contents window smaller.
  ResizeContents(tab_control_);
}

View* TabbedPane::RemoveTabAtIndex(int index) {
  int tab_count = static_cast<int>(tab_views_.size());
  DCHECK(index >= 0 && index < tab_count);

  if (index < (tab_count - 1)) {
    // Select the next tab.
    SelectTabAt(index + 1);
  } else {
    // We are the last tab, select the previous one.
    if (index > 0) {
      SelectTabAt(index - 1);
    } else {
      // That was the last tab. Remove the contents.
      content_window_->GetRootView()->RemoveAllChildViews(false);
    }
  }
  TabCtrl_DeleteItem(tab_control_, index);

  // The removed tab may have made the contents window bigger.
  ResizeContents(tab_control_);

  std::vector<View*>::iterator iter = tab_views_.begin() + index;
  tab_views_.erase(iter);

  return *iter;
}

void TabbedPane::SelectTabAt(int index) {
  DCHECK(index < static_cast<int>(tab_views_.size()));
  TabCtrl_SetCurSel(tab_control_, index);
  DoSelectTabAt(index);
}

int TabbedPane::GetTabCount() {
  return TabCtrl_GetItemCount(tab_control_);
}

HWND TabbedPane::CreateNativeControl(HWND parent_container) {
  // Create the tab control.
  //
  // Note that we don't follow the common convention for NativeControl
  // subclasses and we don't pass the value returned from
  // NativeControl::GetAdditionalExStyle() as the dwExStyle parameter. Here is
  // why: on RTL locales, if we pass NativeControl::GetAdditionalExStyle() when
  // we basically tell Windows to create our HWND with the WS_EX_LAYOUTRTL. If
  // we do that, then the HWND we create for |content_window_| below will
  // inherit the WS_EX_LAYOUTRTL property and this will result in the contents
  // being flipped, which is not what we want (because we handle mirroring in
  // ChromeViews without the use of Windows' support for mirroring). Therefore,
  // we initially create our HWND without the aforementioned property and we
  // explicitly set this property our child is created. This way, on RTL
  // locales, our tabs will be nicely rendered from right to left (by virtue of
  // Windows doing the right thing with the TabbedPane HWND) and each tab
  // contents will use an RTL layout correctly (by virtue of the mirroring
  // infrastructure in ChromeViews doing the right thing with each View we put
  // in the tab).
  tab_control_ = ::CreateWindowEx(0,
                                  WC_TABCONTROL,
                                  L"",
                                  WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
                                  0, 0, GetWidth(), GetHeight(),
                                  parent_container, NULL, NULL, NULL);

  HFONT font = ResourceBundle::GetSharedInstance().
      GetFont(ResourceBundle::BaseFont).hfont();
  SendMessage(tab_control_, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);

  // Create the view container which is a child of the TabControl.
  content_window_ = new HWNDViewContainer();
  content_window_->Init(tab_control_, gfx::Rect(), NULL, false);

  // Explicitly setting the WS_EX_LAYOUTRTL property for the HWND (see above
  // for a thorough explanation regarding why we waited until |content_window_|
  // if created before we set this property for the tabbed pane's HWND).
  if (UILayoutIsRightToLeft()) {
    l10n_util::HWNDSetRTLLayout(tab_control_);
  }

  RootView* root_view = content_window_->GetRootView();
  root_view->SetLayoutManager(new FillLayout());
  DWORD sys_color = ::GetSysColor(COLOR_3DHILIGHT);
  SkColor color = SkColorSetRGB(GetRValue(sys_color), GetGValue(sys_color),
                                GetBValue(sys_color));
  root_view->SetBackground(Background::CreateSolidBackground(color));

  content_window_->SetFocusTraversableParentView(this);
  ResizeContents(tab_control_);
  return tab_control_;
}

LRESULT TabbedPane::OnNotify(int w_param, LPNMHDR l_param) {
  if (static_cast<LPNMHDR>(l_param)->code == TCN_SELCHANGE) {
    int selected_tab = TabCtrl_GetCurSel(tab_control_);
    DCHECK(selected_tab != -1);
    DoSelectTabAt(selected_tab);
    return TRUE;
  }
  return FALSE;
}

void TabbedPane::DoSelectTabAt(int index) {
  RootView* content_root = content_window_->GetRootView();

  // Clear the focus if the focused view was on the tab.
  FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);
  View* focused_view = focus_manager->GetFocusedView();
  if (focused_view && content_root->IsParentOf(focused_view))
    focus_manager->ClearFocus();

  content_root->RemoveAllChildViews(false);
  content_root->AddChildView(tab_views_[index]);
  content_root->Layout();
  if (listener_)
    listener_->TabSelectedAt(index);
}

void TabbedPane::Layout() {
  NativeControl::Layout();
  ResizeContents(GetNativeControlHWND());
}

RootView* TabbedPane::GetContentsRootView() {
  return content_window_->GetRootView();
}

FocusTraversable* TabbedPane::GetFocusTraversable() {
  return content_window_;
}

void TabbedPane::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  NativeControl::ViewHierarchyChanged(is_add, parent, child);

  if (is_add && (child == this) && content_window_) {
    // We have been added to a view hierarchy, update the FocusTraversable
    // parent.
    content_window_->SetFocusTraversableParent(GetRootView());
  }
}

void TabbedPane::ResizeContents(HWND tab_control) {
  DCHECK(tab_control);
  CRect content_bounds;
  if (!GetClientRect(tab_control, &content_bounds))
    return;
  TabCtrl_AdjustRect(tab_control, FALSE, &content_bounds);
  content_window_->MoveWindow(content_bounds.left, content_bounds.top,
                              content_bounds.Width(), content_bounds.Height(),
                              TRUE);
}

}
