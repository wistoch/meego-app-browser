// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/opaque_browser_frame_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/compiler_specific.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"
#include "views/window/window_resources.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif

// static
SkBitmap* OpaqueBrowserFrameView::distributor_logo_ = NULL;
gfx::Font* OpaqueBrowserFrameView::title_font_ = NULL;

#if defined(OS_CHROMEOS)
const int kCustomFrameBackgroundVerticalOffset = 15;
#endif

namespace {
// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 4;
// Besides the frame border, there's another 11 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 11;
// While resize areas on Windows are normally the same size as the window
// borders, our top area is shrunk by 1 px to make it easier to move the window
// around with our thinner top grabbable strip.  (Incidentally, our side and
// bottom resize areas don't match the frame border thickness either -- they
// span the whole nonclient area, so there's no "dead zone" for the mouse.)
const int kTopResizeAdjust = 1;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The titlebar never shrinks too short to show the caption button plus some
// padding below it.
const int kCaptionButtonHeightWithPadding = 19;
// The icon is inset 2 px from the left frame border.
const int kIconLeftSpacing = 2;
// The titlebar has a 2 px 3D edge along the top and bottom.
const int kTitlebarTopAndBottomEdgeThickness = 2;
// There is a 4 px gap between the icon and the title text.
const int kIconTitleSpacing = 4;
// There is a 5 px gap between the title text and the distributor logo (if
// present) or caption buttons.
const int kTitleLogoSpacing = 5;
// In maximized mode, the OTR avatar starts 2 px below the top of the screen, so
// that it doesn't extend into the "3D edge" portion of the titlebar.
const int kOTRMaximizedTopSpacing = 2;
// The OTR avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kOTRBottomSpacing = 2;
// There are 2 px on each side of the OTR avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kOTRSideSpacing = 2;
// The top 1 px of the tabstrip is shadow; in maximized mode we push this off
// the top of the screen so the tabs appear flush against the screen edge.
const int kTabstripTopShadowThickness = 1;
// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;
// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;
// When there's a distributor logo, we leave a 7 px gap between it and the
// caption buttons.
const int kLogoCaptionSpacing = 7;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, public:

OpaqueBrowserFrameView::OpaqueBrowserFrameView(BrowserFrame* frame,
                                               BrowserView* browser_view)
    : BrowserNonClientFrameView(),
      logo_icon_(new views::ImageView()),
      otr_avatar_icon_(new views::ImageView()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          minimize_button_(new views::ImageButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          maximize_button_(new views::ImageButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          restore_button_(new views::ImageButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          close_button_(new views::ImageButton(this))),
      window_icon_(NULL),
      frame_(frame),
      browser_view_(browser_view) {
  InitClass();

  ThemeProvider* tp = frame_->GetThemeProviderForFrame();
  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background =
      tp->GetBitmapNamed(IDR_THEME_WINDOW_CONTROL_BACKGROUND);
  minimize_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_MINIMIZE));
  minimize_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_MINIMIZE_H));
  minimize_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_MINIMIZE_P));
  if (browser_view_->IsBrowserTypeNormal())
    minimize_button_->SetBackground(color, background,
        tp->GetBitmapNamed(IDR_MINIMIZE_BUTTON_MASK));
  minimize_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_MINIMIZE));
  AddChildView(minimize_button_);

  maximize_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_MAXIMIZE));
  maximize_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_MAXIMIZE_H));
  maximize_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_MAXIMIZE_P));
  if (browser_view_->IsBrowserTypeNormal())
    maximize_button_->SetBackground(color, background,
        tp->GetBitmapNamed(IDR_MAXIMIZE_BUTTON_MASK));
  maximize_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_MAXIMIZE));
  AddChildView(maximize_button_);

  restore_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RESTORE));
  restore_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RESTORE_H));
  restore_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RESTORE_P));
  if (browser_view_->IsBrowserTypeNormal())
    restore_button_->SetBackground(color, background,
        tp->GetBitmapNamed(IDR_RESTORE_BUTTON_MASK));
  restore_button_->SetAccessibleName(
      l10n_util::GetString(IDS_ACCNAME_RESTORE));
  AddChildView(restore_button_);

  close_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_CLOSE));
  close_button_->SetImage(
      views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_CLOSE_H));
  close_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_CLOSE_P));
  if (browser_view_->IsBrowserTypeNormal())
    close_button_->SetBackground(color, background,
        tp->GetBitmapNamed(IDR_CLOSE_BUTTON_MASK));
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  otr_avatar_icon_->SetImage(browser_view_->GetOTRAvatarIcon());
  AddChildView(otr_avatar_icon_);
  if (distributor_logo_) {
    logo_icon_->SetImage(distributor_logo_);
  } else {
    logo_icon_->SetVisible(false);
  }
  AddChildView(logo_icon_);
  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view_->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }
}

OpaqueBrowserFrameView::~OpaqueBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForTabStrip(
    BaseTabStrip* tabstrip) const {
  int x_offset = browser_view_->browser_extender()->GetMainMenuWidth();
  int tabstrip_x = browser_view_->ShouldShowOffTheRecordAvatar() ?
      (otr_avatar_icon_->bounds().right() + kOTRSideSpacing) :
      NonClientBorderThickness() + x_offset;
  int tabstrip_width = minimize_button_->x() - tabstrip_x -
      (frame_->GetWindow()->IsMaximized() ?
      kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing);
  return gfx::Rect(tabstrip_x, NonClientTopBorderHeight(),
                   std::max(0, tabstrip_width),
                   tabstrip->GetPreferredHeight());
}

void OpaqueBrowserFrameView::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

gfx::Size OpaqueBrowserFrameView::GetMinimumSize() {
  gfx::Size min_size(browser_view_->GetMinimumSize());
  int border_thickness = NonClientBorderThickness();
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight() + border_thickness);

  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  int min_titlebar_width = (2 * FrameBorderThickness()) + kIconLeftSpacing +
    (d->ShouldShowWindowIcon() ? (IconSize() + kTitleLogoSpacing) : 0) +
    ((distributor_logo_ && browser_view_->ShouldShowDistributorLogo()) ?
         (distributor_logo_->width() + kLogoCaptionSpacing) : 0);

#if !defined(OS_CHROMEOS)
  min_titlebar_width +=
      minimize_button_->GetMinimumSize().width() +
      restore_button_->GetMinimumSize().width() +
      close_button_->GetMinimumSize().width();
#endif
  min_size.set_width(std::max(min_size.width(), min_titlebar_width));

  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

bool OpaqueBrowserFrameView::AlwaysUseNativeFrame() const {
  return frame_->AlwaysUseNativeFrame();
}

gfx::Rect OpaqueBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int OpaqueBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame_->GetWindow()->GetClientView()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->IsVisible() &&
      close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTCLOSE;
  if (restore_button_->IsVisible() &&
      restore_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTMAXBUTTON;
  if (maximize_button_->IsVisible() &&
      maximize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTMAXBUTTON;
  if (minimize_button_->IsVisible() &&
      minimize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(
      point))
    return HTMINBUTTON;
  if (window_icon_ &&
      window_icon_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTSYSMENU;

  int window_component = GetHTComponentForFrame(point, TopResizeHeight(),
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void OpaqueBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
  DCHECK(window_mask);

  if (frame_->GetWindow()->IsMaximized() || frame_->GetWindow()->IsFullscreen())
    return;

  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 2);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(2, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 2), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 2);
  window_mask->lineTo(SkIntToScalar(size.width()), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
  window_mask->close();
}

void OpaqueBrowserFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void OpaqueBrowserFrameView::ResetWindowControls() {
  restore_button_->SetState(views::CustomButton::BS_NORMAL);
  minimize_button_->SetState(views::CustomButton::BS_NORMAL);
  maximize_button_->SetState(views::CustomButton::BS_NORMAL);
  // The close button isn't affected by this constraint.
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::View overrides:

void OpaqueBrowserFrameView::Paint(gfx::Canvas* canvas) {
  views::Window* window = frame_->GetWindow();
  if (window->IsFullscreen())
    return;  // Nothing is visible, so don't bother to paint.

  if (window->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);
  PaintTitleBar(canvas);
  PaintToolbarBackground(canvas);
  if (!window->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void OpaqueBrowserFrameView::Layout() {
  LayoutWindowControls();
  LayoutDistributorLogo();
  LayoutTitleBar();
  LayoutOTRAvatar();
  LayoutClientView();
}

bool OpaqueBrowserFrameView::HitTest(const gfx::Point& l) const {
  // If the point is outside the bounds of the client area, claim it.
  bool in_nonclient = NonClientFrameView::HitTest(l);
  if (in_nonclient)
    return in_nonclient;

  // Otherwise claim it only if it's in a non-tab portion of the tabstrip.
  if (l.y() > browser_view_->tabstrip()->bounds().bottom())
    return false;

  // We convert from our parent's coordinates since we assume we fill its bounds
  // completely. We need to do this since we're not a parent of the tabstrip,
  // meaning ConvertPointToView would otherwise return something bogus.
  gfx::Point browser_view_point(l);
  View::ConvertPointToView(GetParent(), browser_view_, &browser_view_point);
  return browser_view_->IsPositionInWindowCaption(browser_view_point);
}

void OpaqueBrowserFrameView::ViewHierarchyChanged(bool is_add,
                                                  views::View* parent,
                                                  views::View* child) {
  if (is_add && child == this) {
    // The Accessibility glue looks for the product name on these two views to
    // determine if this is in fact a Chrome window.
    GetRootView()->SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
  }
}

bool OpaqueBrowserFrameView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_TITLEBAR;
  return true;
}

bool OpaqueBrowserFrameView::GetAccessibleName(std::wstring* name) {
  DCHECK(name);

  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void OpaqueBrowserFrameView::SetAccessibleName(const std::wstring& name) {
  accessible_name_ = name;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::ButtonListener implementation:

void OpaqueBrowserFrameView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  views::Window* window = frame_->GetWindow();
  if (sender == minimize_button_)
    window->Minimize();
  else if (sender == maximize_button_)
    window->Maximize();
  else if (sender == restore_button_)
    window->Restore();
  else if (sender == close_button_)
    window->Close();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, TabIconView::TabContentsProvider implementation:

bool OpaqueBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view_->GetSelectedTabContents();
  return current_tab ? current_tab->is_loading() : false;
}

SkBitmap OpaqueBrowserFrameView::GetFavIconForTabIconView() {
  return frame_->GetWindow()->GetDelegate()->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, private:

int OpaqueBrowserFrameView::FrameBorderThickness() const {
  views::Window* window = frame_->GetWindow();
  return (window->IsMaximized() || window->IsFullscreen()) ?
      0 : kFrameBorderThickness;
}

int OpaqueBrowserFrameView::TopResizeHeight() const {
  return FrameBorderThickness() - kTopResizeAdjust;
}

int OpaqueBrowserFrameView::NonClientBorderThickness() const {
  // When we fill the screen, we don't show a client edge.
  views::Window* window = frame_->GetWindow();
  return FrameBorderThickness() +
      ((window->IsMaximized() || window->IsFullscreen()) ?
       0 : kClientEdgeThickness);
}

int OpaqueBrowserFrameView::NonClientTopBorderHeight() const {
  views::Window* window = frame_->GetWindow();
  if (window->GetDelegate()->ShouldShowWindowTitle()) {
    return std::max(IconSize() + FrameBorderThickness(),
                    CaptionButtonY() + kCaptionButtonHeightWithPadding) +
        TitlebarBottomThickness();
  }

  if (browser_view_->IsTabStripVisible() && window->IsMaximized())
    return FrameBorderThickness() - kTabstripTopShadowThickness;

  return FrameBorderThickness() +
      ((window->IsMaximized() || window->IsFullscreen()) ?
       0 : kNonClientRestoredExtraThickness);
}

int OpaqueBrowserFrameView::CaptionButtonY() const {
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
  return frame_->GetWindow()->IsMaximized() ?
      FrameBorderThickness() : kFrameShadowThickness;
}

int OpaqueBrowserFrameView::TitlebarBottomThickness() const {
  // When a toolbar is edging the titlebar, it draws its own bottom edge.
  if (browser_view_->IsToolbarVisible())
    return 0;

  return kTitlebarTopAndBottomEdgeThickness +
      (frame_->GetWindow()->IsMaximized() ? 0 : kClientEdgeThickness);
}

int OpaqueBrowserFrameView::RightEdge() const {
  return width() - FrameBorderThickness();
}

int OpaqueBrowserFrameView::IconSize() const {
#if defined(OS_WIN)
  // This metric scales up if either the titlebar height or the titlebar font
  // size are increased.
  return GetSystemMetrics(SM_CYSMICON);
#else
  // Calculate the necessary height from the titlebar font size.
  // The title text has 2 px of padding between it and the frame border on both
  // top and bottom.
  const int kTitleBorderSpacing = 2;
  InitAppWindowResources();  // To make sure the title_font_ is loaded.
  // The bottom spacing should be the same apparent height as the top spacing.
  // The top spacing height is FrameBorderThickness() + kTitleBorderSpacing.  We
  // omit the frame border portion because that's not part of the icon height.
  // The bottom spacing, then, is kTitleBorderSpacing + kFrameBorderThickness to
  // the bottom edge of the titlebar.  We omit TitlebarBottomThickness() because
  // that's also not part of the icon height.
  return kTitleBorderSpacing + title_font_->height() + kTitleBorderSpacing +
      (kFrameBorderThickness - TitlebarBottomThickness());
#endif
}

void OpaqueBrowserFrameView::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();

  SkBitmap* top_left_corner = tp->GetBitmapNamed(IDR_WINDOW_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      tp->GetBitmapNamed(IDR_WINDOW_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = tp->GetBitmapNamed(IDR_WINDOW_TOP_CENTER);
  SkBitmap* right_edge = tp->GetBitmapNamed(IDR_WINDOW_RIGHT_SIDE);
  SkBitmap* left_edge = tp->GetBitmapNamed(IDR_WINDOW_LEFT_SIDE);
  SkBitmap* bottom_left_corner =
      tp->GetBitmapNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
      tp->GetBitmapNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = tp->GetBitmapNamed(IDR_WINDOW_BOTTOM_CENTER);


  // Window frame mode and color
  SkBitmap* theme_frame;
  SkColor frame_color;

  // Never theme app and popup windows.
  if (!browser_view_->IsBrowserTypeNormal()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    if (ShouldPaintAsActive()) {
      theme_frame = rb.GetBitmapNamed(IDR_FRAME);
      frame_color = browser_view_->IsOffTheRecord() ?
          ResourceBundle::frame_color_incognito :
          ResourceBundle::frame_color;
    } else {
      theme_frame = rb.GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
      frame_color = browser_view_->IsOffTheRecord() ?
          ResourceBundle::frame_color_incognito_inactive :
          ResourceBundle::frame_color_inactive;
    }
  } else if (!browser_view_->IsOffTheRecord()) {
    if (ShouldPaintAsActive()) {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME);
      frame_color = tp->GetColor(BrowserThemeProvider::COLOR_FRAME);
    } else {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
      frame_color = tp->GetColor(BrowserThemeProvider::COLOR_FRAME_INACTIVE);
    }
  } else {
    if (ShouldPaintAsActive()) {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO);
      frame_color = tp->GetColor(BrowserThemeProvider::COLOR_FRAME_INCOGNITO);
    } else {
      theme_frame = tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO_INACTIVE);
      frame_color = tp->GetColor(
          BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE);
    }
  }

  // Fill with the frame color first so we have a constant background for
  // areas not covered by the theme image.
  canvas->FillRectInt(frame_color, 0, 0, width(), theme_frame->height());
  // Now fill down the sides
  canvas->FillRectInt(frame_color,
                      0, theme_frame->height(),
                      left_edge->width(), height() - theme_frame->height());
  canvas->FillRectInt(frame_color,
                      width() - right_edge->width(), theme_frame->height(),
                      right_edge->width(), height() - theme_frame->height());
  // Now fill the bottom area.
  canvas->FillRectInt(frame_color,
      left_edge->width(), height() - bottom_edge->height(),
      width() - left_edge->width() - right_edge->width(),
      bottom_edge->height());

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), theme_frame->height());

  // Draw the theme frame overlay
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view_->IsBrowserTypeNormal() &&
      !browser_view_->IsOffTheRecord()) {
    SkBitmap* theme_overlay;
    if (ShouldPaintAsActive())
      theme_overlay = tp->GetBitmapNamed(IDR_THEME_FRAME_OVERLAY);
    else
      theme_overlay = tp->GetBitmapNamed(IDR_THEME_FRAME_OVERLAY_INACTIVE);
    canvas->DrawBitmapInt(*theme_overlay, 0, 0);
  }

  // Top.
  int top_left_height = std::min(top_left_corner->height(),
                                 height() - bottom_left_corner->height());
  canvas->DrawBitmapInt(*top_left_corner, 0, 0, top_left_corner->width(),
      top_left_height, 0, 0, top_left_corner->width(), top_left_height, false);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  int top_right_height = std::min(top_right_corner->height(),
                                  height() - bottom_right_corner->height());
  canvas->DrawBitmapInt(*top_right_corner, 0, 0, top_right_corner->width(),
      top_right_height, width() - top_right_corner->width(), 0,
      top_right_corner->width(), top_right_height, false);
  // Note: When we don't have a toolbar, we need to draw some kind of bottom
  // edge here.  Because the App Window graphics we use for this have an
  // attached client edge and their sizing algorithm is a little involved, we do
  // all this in PaintRestoredClientEdge().

  // Right.
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
      top_right_height, right_edge->width(),
      height() - top_right_height - bottom_right_corner->height());

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width() - bottom_right_corner->width(),
                        height() - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_edge, bottom_left_corner->width(),
                       height() - bottom_edge->height(),
                       width() - bottom_left_corner->width() -
                           bottom_right_corner->width(),
                       bottom_edge->height());
  canvas->DrawBitmapInt(*bottom_left_corner, 0,
                        height() - bottom_left_corner->height());

  // Left.
  canvas->TileImageInt(*left_edge, 0, top_left_height, left_edge->width(),
      height() - top_left_height - bottom_left_corner->height());
}


void OpaqueBrowserFrameView::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();
  views::Window* window = frame_->GetWindow();

  // Window frame mode and color
  SkBitmap* theme_frame;
  int y = 0;
  // Never theme app and popup windows.
  if (!browser_view_->IsBrowserTypeNormal()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    if (ShouldPaintAsActive())
      theme_frame = rb.GetBitmapNamed(IDR_FRAME);
    else
      theme_frame = rb.GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
  } else if (!browser_view_->IsOffTheRecord()) {
    theme_frame = ShouldPaintAsActive() ?
        tp->GetBitmapNamed(IDR_THEME_FRAME) :
        tp->GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
#if defined(OS_CHROMEOS)
    // TODO:(oshima): gtk based CHROMEOS is using non custom frame
    // mode which does this adjustment. This should be removed
    // once it's fully migrated to views. -1 is due to the layout
    // difference between views and gtk and will be removed.
    // See http://crbug.com/28580.
    y = -kCustomFrameBackgroundVerticalOffset - 1;
#endif
  } else {
    theme_frame = ShouldPaintAsActive() ?
        tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO) :
        tp->GetBitmapNamed(IDR_THEME_FRAME_INCOGNITO_INACTIVE);
#if defined(OS_CHROMEOS)
    y = -kCustomFrameBackgroundVerticalOffset - 1;
#endif
  }
  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, y, width(), theme_frame->height());

  // Draw the theme frame overlay
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view_->IsBrowserTypeNormal()) {
    SkBitmap* theme_overlay = ShouldPaintAsActive() ?
        tp->GetBitmapNamed(IDR_THEME_FRAME_OVERLAY) :
        tp->GetBitmapNamed(IDR_THEME_FRAME_OVERLAY_INACTIVE);
    canvas->DrawBitmapInt(*theme_overlay, 0, 0);
  }

  if (!browser_view_->IsToolbarVisible()) {
    // There's no toolbar to edge the frame border, so we need to draw a bottom
    // edge.  The graphic we use for this has a built in client edge, so we clip
    // it off the bottom.
    SkBitmap* top_center =
        tp->GetBitmapNamed(IDR_APP_TOP_CENTER);
    int edge_height = top_center->height() - kClientEdgeThickness;
    canvas->TileImageInt(*top_center, 0,
        window->GetClientView()->y() - edge_height, width(), edge_height);
  }
}

void OpaqueBrowserFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by the TabIconView.
  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  if (d->ShouldShowWindowTitle()) {
    InitAppWindowResources();  // To make sure the title_font_ is loaded.
    canvas->DrawStringInt(d->GetWindowTitle(), *title_font_, SK_ColorWHITE,
        MirroredLeftPointForRect(title_bounds_), title_bounds_.y(),
        title_bounds_.width(), title_bounds_.height());
    /* TODO(pkasting):  If this window is active, we should also draw a drop
     * shadow on the title.  This is tricky, because we don't want to hardcode a
     * shadow color (since we want to work with various themes), but we can't
     * alpha-blend either (since the Windows text APIs don't really do this).
     * So we'd need to sample the background color at the right location and
     * synthesize a good shadow color. */
  }
}

void OpaqueBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  if (!browser_view_->IsToolbarVisible())
    return;

  gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;

  ThemeProvider* tp = GetThemeProvider();
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToView(frame_->GetWindow()->GetClientView(),
                           this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  SkColor theme_toolbar_color =
      tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);
  canvas->FillRectInt(theme_toolbar_color,
      toolbar_bounds.x(), toolbar_bounds.y() + 2,
      toolbar_bounds.width(), toolbar_bounds.height() - 2);

  int strip_height = browser_view_->GetTabStripHeight();
  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);

  canvas->TileImageInt(*theme_toolbar,
      toolbar_bounds.x() - 1, strip_height - 1,  // crop src
      toolbar_bounds.x() - 1, toolbar_bounds.y() + 2,
      toolbar_bounds.width() + 2, theme_toolbar->height());

  SkBitmap* toolbar_left =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);

  // Gross hack: We split the toolbar images into two pieces, since sometimes
  // (popup mode) the toolbar isn't tall enough to show the whole image.  The
  // split happens between the top shadow section and the bottom gradient
  // section so that we never break the gradient.
  int split_point = kFrameShadowThickness * 2;
  int bottom_y = toolbar_bounds.y() + split_point;
  int bottom_edge_height =
      std::min(toolbar_left->height(), toolbar_bounds.height()) - split_point;

  canvas->DrawBitmapInt(*toolbar_left, 0, 0, toolbar_left->width(), split_point,
      toolbar_bounds.x() - toolbar_left->width(), toolbar_bounds.y(),
      toolbar_left->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_left, 0,
      toolbar_left->height() - bottom_edge_height, toolbar_left->width(),
      bottom_edge_height, toolbar_bounds.x() - toolbar_left->width(), bottom_y,
      toolbar_left->width(), bottom_edge_height, false);

  SkBitmap* toolbar_center =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
  canvas->TileImageInt(*toolbar_center, 0, 0, toolbar_bounds.x(),
      toolbar_bounds.y(), toolbar_bounds.width(), split_point);

  SkBitmap* toolbar_right = tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER);
  canvas->DrawBitmapInt(*toolbar_right, 0, 0, toolbar_right->width(),
      split_point, toolbar_bounds.right(), toolbar_bounds.y(),
      toolbar_right->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_right, 0,
      toolbar_right->height() - bottom_edge_height, toolbar_right->width(),
      bottom_edge_height, toolbar_bounds.right(), bottom_y,
      toolbar_right->width(), bottom_edge_height, false);

  // Draw the content/toolbar separator.
  canvas->DrawLineInt(ResourceBundle::toolbar_separator_color,
      toolbar_bounds.x(), toolbar_bounds.bottom() - 1,
      toolbar_bounds.right() - 1, toolbar_bounds.bottom() - 1);
}

void OpaqueBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();
  int client_area_top = frame_->GetWindow()->GetClientView()->y();

  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());
  SkColor toolbar_color = tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);

  if (browser_view_->IsToolbarVisible()) {
    // The client edges start below the toolbar or its corner images, whichever
    // is shorter.
    gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
    client_area_top += browser_view_->GetToolbarBounds().y() +
        std::min(tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER)->height(),
                 toolbar_bounds.height());
  } else {
    // The toolbar isn't going to draw a client edge for us, so draw one
    // ourselves.
    SkBitmap* top_left = tp->GetBitmapNamed(IDR_APP_TOP_LEFT);
    SkBitmap* top_center = tp->GetBitmapNamed(IDR_APP_TOP_CENTER);
    SkBitmap* top_right = tp->GetBitmapNamed(IDR_APP_TOP_RIGHT);
    int top_edge_y = client_area_top - top_center->height();
    int height = client_area_top - top_edge_y;

    canvas->DrawBitmapInt(*top_left, 0, 0, top_left->width(), height,
        client_area_bounds.x() - top_left->width(), top_edge_y,
        top_left->width(), height, false);
    canvas->TileImageInt(*top_center, 0, 0, client_area_bounds.x(), top_edge_y,
      client_area_bounds.width(), std::min(height, top_center->height()));
    canvas->DrawBitmapInt(*top_right, 0, 0, top_right->width(), height,
        client_area_bounds.right(), top_edge_y,
        top_right->width(), height, false);

    // Draw the toolbar color across the top edge.
    canvas->DrawLineInt(toolbar_color,
        client_area_bounds.x() - kClientEdgeThickness,
        client_area_top - kClientEdgeThickness,
        client_area_bounds.right() + kClientEdgeThickness,
        client_area_top - kClientEdgeThickness);
  }

  int client_area_bottom =
      std::max(client_area_top, height() - NonClientBorderThickness());
  int client_area_height = client_area_bottom - client_area_top;

  // Draw the toolbar color so that the one pixel areas down the sides
  // show the right color even if not covered by the toolbar image.
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_top,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness,
      client_area_bounds.right() + kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.right() - 1 + kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness,
      client_area_bounds.right() - 1 + kClientEdgeThickness,
      client_area_top);

  SkBitmap* right = tp->GetBitmapNamed(IDR_CONTENT_RIGHT_SIDE);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);
  canvas->DrawBitmapInt(
      *tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
      client_area_bounds.right(), client_area_bottom);

  SkBitmap* bottom = tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
      client_area_bottom, client_area_bounds.width(),
      bottom->height());

  SkBitmap* bottom_left =
      tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  canvas->DrawBitmapInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);

  SkBitmap* left = tp->GetBitmapNamed(IDR_CONTENT_LEFT_SIDE);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);
}

void OpaqueBrowserFrameView::LayoutWindowControls() {
  bool is_maximized = frame_->GetWindow()->IsMaximized();
#if defined(OS_CHROMEOS)
  minimize_button_->SetVisible(!is_maximized);
  restore_button_->SetVisible(!is_maximized);
  maximize_button_->SetVisible(!is_maximized);
  close_button_->SetVisible(!is_maximized);
  if (is_maximized) {
    // Set the bounds of the minimize button so that we don't have to change
    // other places that rely on the bounds. Put it slightly to the right
    // of the edge of the view, so that when we remove the spacing it lines
    // up with the edge.
    minimize_button_->SetBounds(
        RightEdge() + kNewTabCaptionMaximizedSpacing,
        0,
        0,
        0);
    return;
  }
#endif
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                   views::ImageButton::ALIGN_BOTTOM);
  int caption_y = CaptionButtonY();
  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend the rightmost
  // button to the screen corner to obey Fitts' Law.
  int right_extra_width = is_maximized ?
      (kFrameBorderThickness - kFrameShadowThickness) : 0;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(RightEdge() - close_button_size.width() -
      right_extra_width, caption_y,
      close_button_size.width() + right_extra_width,
      close_button_size.height());

  // When the window is restored, we show a maximized button; otherwise, we show
  // a restore button.
  bool is_restored = !is_maximized && !frame_->GetWindow()->IsMinimized();
  views::ImageButton* invisible_button = is_restored ?
      restore_button_ : maximize_button_;
  invisible_button->SetVisible(false);

  views::ImageButton* visible_button = is_restored ?
      maximize_button_ : restore_button_;
  visible_button->SetVisible(true);
  visible_button->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                    views::ImageButton::ALIGN_BOTTOM);
  gfx::Size visible_button_size = visible_button->GetPreferredSize();
  visible_button->SetBounds(close_button_->x() - visible_button_size.width(),
                            caption_y, visible_button_size.width(),
                            visible_button_size.height());

  minimize_button_->SetVisible(true);
  minimize_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                      views::ImageButton::ALIGN_BOTTOM);
  gfx::Size minimize_button_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      visible_button->x() - minimize_button_size.width(), caption_y,
      minimize_button_size.width(),
      minimize_button_size.height());
}

void OpaqueBrowserFrameView::LayoutDistributorLogo() {
  // Always lay out the logo, even when it's not present, so we can lay out the
  // window title based on its position.
  if (distributor_logo_ &&
      !frame_->GetWindow()->IsMaximized() &&
      browser_view_->ShouldShowDistributorLogo()) {
    logo_icon_->SetVisible(true);
    gfx::Size preferred_size = logo_icon_->GetPreferredSize();
    logo_icon_->SetBounds(
        minimize_button_->x() - preferred_size.width() - kLogoCaptionSpacing,
        TopResizeHeight(), preferred_size.width(),
        preferred_size.height());
  } else {
    logo_icon_->SetVisible(false);
    logo_icon_->SetBounds(minimize_button_->x(), TopResizeHeight(), 0, 0);
  }
}

void OpaqueBrowserFrameView::LayoutTitleBar() {
  // Always lay out the icon, even when it's not present, so we can lay out the
  // window title based on its position.
  int frame_thickness = FrameBorderThickness();
  int icon_x = frame_thickness + kIconLeftSpacing;
  int icon_size = IconSize();
  // This next statement handles vertically centering the icon when the icon is
  // shorter than the minimum space we reserve for the caption button.
  // Practically, this never occurs except in maximized mode, since otherwise
  // the minimum icon size supplied by Windows (16) + the frame border height
  // (4) >= the minimum caption button space (19 + the frame shadow thickness
  // (1)).  In maximized mode we want to bias rounding to put extra space above
  // the icon, since below it is the 2 px 3D edge, which looks to the eye like
  // additional space; hence the + 1 below.
  int icon_y = frame_thickness + ((NonClientTopBorderHeight() -
      frame_thickness - icon_size - TitlebarBottomThickness() + 1) / 2);

  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  if (d->ShouldShowWindowIcon()) {
    // Hack: Our frame border has a different "3D look" than Windows'.  Theirs
    // has a more complex gradient on the top that they push their icon/title
    // below; then the maximized window cuts this off and the icon/title are
    // centered in the remaining space.  Because the apparent shape of our
    // border is simpler, using the same positioning makes things look slightly
    // uncentered with restored windows, so we come up to compensate.  The frame
    // border has a 2 px 3D edge plus some empty space, so we adjust by half the
    // width of the empty space to center things.
    if (!frame_->GetWindow()->IsMaximized())
      icon_y -= (frame_thickness - kTitlebarTopAndBottomEdgeThickness) / 2;

    window_icon_->SetBounds(icon_x, icon_y, icon_size, icon_size);
  }

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    InitAppWindowResources();  // To make sure the title_font_ is loaded.
    int title_x = icon_x +
        (d->ShouldShowWindowIcon() ? icon_size + kIconTitleSpacing : 0);
    int title_height = title_font_->height();
    title_bounds_.SetRect(title_x, icon_y + ((icon_size - title_height) / 2),
        std::max(0, logo_icon_->x() - kTitleLogoSpacing - title_x),
        title_height);
  }
}

void OpaqueBrowserFrameView::LayoutOTRAvatar() {
  int top_height = NonClientTopBorderHeight();
  int tabstrip_height, otr_height;
  bool visible = browser_view_->ShouldShowOffTheRecordAvatar();
  gfx::Size preferred_size = otr_avatar_icon_->GetPreferredSize();
  if (browser_view_->IsTabStripVisible()) {
    tabstrip_height = browser_view_->GetTabStripHeight() - kOTRBottomSpacing;
    otr_height = frame_->GetWindow()->IsMaximized() ?
        (tabstrip_height - kOTRMaximizedTopSpacing) :
        preferred_size.height();

  } else {
    tabstrip_height = otr_height = 0;
    visible = false;
  }
  otr_avatar_icon_->SetVisible(visible);
  int x_offset = browser_view_->browser_extender()->GetMainMenuWidth();
  otr_avatar_icon_->SetBounds(NonClientBorderThickness() + kOTRSideSpacing +
                              x_offset,
                              top_height + tabstrip_height - otr_height,
                              preferred_size.width(), otr_height);
}

void OpaqueBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Rect OpaqueBrowserFrameView::CalculateClientAreaBounds(int width,
                                                            int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

// static
void OpaqueBrowserFrameView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
#if defined(GOOGLE_CHROME_BUILD)
    distributor_logo_ = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_DISTRIBUTOR_LOGO_LIGHT);
#endif
    initialized = true;
  }
}

// static
void OpaqueBrowserFrameView::InitAppWindowResources() {
  static bool initialized = false;
  if (!initialized) {
#if defined(OS_WIN)
    title_font_ = new gfx::Font(win_util::GetWindowTitleFont());
#else
    title_font_ = new gfx::Font();
#endif
    initialized = true;
  }
}
