// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/aero_glass_non_client_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/client_view.h"
#include "chrome/views/window_delegate.h"
#include "chrome/views/window_resources.h"

// An enumeration of bitmap resources used by this window.
enum {
  FRAME_PART_BITMAP_FIRST = 0,  // must be first.

  // Client Edge Border.
  FRAME_CLIENT_EDGE_TOP_LEFT,
  FRAME_CLIENT_EDGE_TOP,
  FRAME_CLIENT_EDGE_TOP_RIGHT,
  FRAME_CLIENT_EDGE_RIGHT,
  FRAME_CLIENT_EDGE_BOTTOM_RIGHT,
  FRAME_CLIENT_EDGE_BOTTOM,
  FRAME_CLIENT_EDGE_BOTTOM_LEFT,
  FRAME_CLIENT_EDGE_LEFT,

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

class AeroGlassWindowResources {
 public:
  AeroGlassWindowResources() { InitClass(); }
  virtual ~AeroGlassWindowResources() { }

  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }

  SkBitmap app_top_left() const { return app_top_left_; }
  SkBitmap app_top_center() const { return app_top_center_; }
  SkBitmap app_top_right() const { return app_top_right_; }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CONTENT_TOP_LEFT_CORNER, IDR_CONTENT_TOP_CENTER,
            IDR_CONTENT_TOP_RIGHT_CORNER, IDR_CONTENT_RIGHT_SIDE,
            IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
            IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      app_top_left_ = *rb.GetBitmapNamed(IDR_APP_TOP_LEFT);
      app_top_center_ = *rb.GetBitmapNamed(IDR_APP_TOP_CENTER);
      app_top_right_ = *rb.GetBitmapNamed(IDR_APP_TOP_RIGHT);      

      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static SkBitmap app_top_left_;
  static SkBitmap app_top_center_;
  static SkBitmap app_top_right_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassWindowResources);
};

// static
SkBitmap* AeroGlassWindowResources::standard_frame_bitmaps_[];
SkBitmap AeroGlassWindowResources::app_top_left_;
SkBitmap AeroGlassWindowResources::app_top_center_;
SkBitmap AeroGlassWindowResources::app_top_right_;

AeroGlassWindowResources* AeroGlassNonClientView::resources_ = NULL;
SkBitmap AeroGlassNonClientView::distributor_logo_;

// The width of the client edge to the left and right of the window.
static const int kClientEdgeWidth = 3;
// The height of the client edge to the bottom of the window.
static const int kClientEdgeHeight = 2;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of the top and bottom edges triggers diagonal resizing.
const int kResizeEdgeWidth = 16;
// The horizontal distance between the left of the minimize button and the
// right edge of the distributor logo.
static const int kDistributorLogoHorizontalOffset = 7;
// The distance from the top of the non-client view and the top edge of the
// distributor logo.
static const int kDistributorLogoVerticalOffset = 3;
// The distance of the TabStrip from the top of the window's client area.
static const int kTabStripY = 19;
// How much space on the right is not used for the tab strip (to provide
// separation between the tabs and the window controls).
static const int kTabStripRightHorizOffset = 30;
// A single pixel.
static const int kPixel = 1;
// The size (width/height) of the window icon.
static const int kWindowIconSize = 16;
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

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, public:

AeroGlassNonClientView::AeroGlassNonClientView(AeroGlassFrame* frame,
                                               BrowserView* browser_view)
    : frame_(frame),
      browser_view_(browser_view) {
  InitClass();
}

AeroGlassNonClientView::~AeroGlassNonClientView() {
}

gfx::Rect AeroGlassNonClientView::GetBoundsForTabStrip(TabStrip* tabstrip) {
  int tabstrip_x = browser_view_->ShouldShowOffTheRecordAvatar() ?
      (otr_avatar_bounds_.right() + kOTRSideSpacing) : kClientEdgeWidth;
  int tabstrip_width = width() - tabstrip_x - kTabStripRightHorizOffset -
    (frame_->IsMaximized() ? frame_->GetMinimizeButtonOffset() : 0);
  int tabstrip_y =
    frame_->IsMaximized() ? (CalculateNonClientTopHeight() - 2) : kTabStripY;
  return gfx::Rect(tabstrip_x, tabstrip_y, std::max(0, tabstrip_width),
                   tabstrip->GetPreferredHeight());
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, views::NonClientView implementation:

gfx::Rect AeroGlassNonClientView::CalculateClientAreaBounds(int win_width,
                                                            int win_height) const {
  if (!browser_view_->IsTabStripVisible())
    return gfx::Rect(0, 0, width(), height());

  int top_margin = CalculateNonClientTopHeight();
  return gfx::Rect(kClientEdgeWidth, top_margin,
      std::max(0, win_width - (2 * kClientEdgeWidth)),
      std::max(0, win_height - top_margin - kClientEdgeHeight));
}

gfx::Size AeroGlassNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int top_margin = CalculateNonClientTopHeight();
  return gfx::Size(width + (2 * kClientEdgeWidth),
                   height + top_margin + kClientEdgeHeight);
}

CPoint AeroGlassNonClientView::GetSystemMenuPoint() const {
  CPoint offset(0, 0);
  MapWindowPoints(GetWidget()->GetHWND(), HWND_DESKTOP, &offset, 1);
  return offset;
}

int AeroGlassNonClientView::NonClientHitTest(const gfx::Point& point) {
  // If we don't have a tabstrip, we haven't customized the frame, so Windows
  // can figure this out.  If the point isn't within our bounds, then it's in
  // the native portion of the frame, so again Windows can figure it out.
  if (!browser_view_->IsTabStripVisible() || !bounds().Contains(point))
    return HTNOWHERE;

  // See if the client view intersects the non-client area (e.g. blank areas
  // of the TabStrip).
  int frame_component = frame_->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
  int resize_width = kResizeEdgeWidth - border_thickness;
  if (point.x() < kClientEdgeWidth) {
    if (point.y() < border_thickness)
      return HTTOPLEFT;
    if (point.y() >= (height() - kClientEdgeHeight))
      return HTBOTTOMLEFT;
    return HTLEFT;
  }
  if (point.x() >= (width() - kClientEdgeWidth)) {
    if (point.y() < border_thickness)
      return HTTOPRIGHT;
    if (point.y() >= (height() - kClientEdgeHeight))
      return HTBOTTOMRIGHT;
    return HTRIGHT;
  }
  if (point.y() < border_thickness) {
    if (point.x() < resize_width)
      return HTTOPLEFT;
    if (point.x() >= (width() - resize_width))
      return HTTOPRIGHT;
    return HTTOP;
  }
  if (point.y() >= (height() - kClientEdgeHeight)) {
    if (point.x() < resize_width)
      return HTBOTTOMLEFT;
    if (point.x() >= (width() - resize_width))
      return HTBOTTOMRIGHT;
    return HTBOTTOM;
  }
  return HTCAPTION;
}

void AeroGlassNonClientView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
  // We use the native window region.
}

void AeroGlassNonClientView::EnableClose(bool enable) {
  // This is handled exclusively by Window.
}

void AeroGlassNonClientView::ResetWindowControls() {
  // Our window controls are rendered by the system and do not require reset.
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, views::View overrides:

void AeroGlassNonClientView::Paint(ChromeCanvas* canvas) {
  PaintDistributorLogo(canvas);
  if (browser_view_->IsTabStripVisible())
    PaintToolbarBackground(canvas);
  PaintOTRAvatar(canvas);
  if (browser_view_->IsTabStripVisible())
    PaintClientEdge(canvas);
}

void AeroGlassNonClientView::Layout() {
  LayoutOTRAvatar();
  LayoutDistributorLogo();
  LayoutClientView();
}

gfx::Size AeroGlassNonClientView::GetPreferredSize() {
  gfx::Size prefsize = frame_->client_view()->GetPreferredSize();
  prefsize.Enlarge(2 * kClientEdgeWidth,
                   CalculateNonClientTopHeight() + kClientEdgeHeight);
  return prefsize;
}

void AeroGlassNonClientView::ViewHierarchyChanged(bool is_add,
                                                  views::View* parent,
                                                  views::View* child) {
  if (is_add && child == this) {
    DCHECK(GetWidget());
    DCHECK(frame_->client_view()->GetParent() != this);
    AddChildView(frame_->client_view());
  }
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, private:

int AeroGlassNonClientView::CalculateNonClientTopHeight() const {
  return browser_view_->IsTabStripVisible() ?
      GetSystemMetrics(SM_CYSIZEFRAME) : 0;
}

void AeroGlassNonClientView::PaintOTRAvatar(ChromeCanvas* canvas) {
  if (!browser_view_->ShouldShowOffTheRecordAvatar())
    return;

  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  canvas->DrawBitmapInt(otr_avatar_icon, 0,
      (otr_avatar_icon.height() - otr_avatar_bounds_.height()) / 2,
      otr_avatar_bounds_.width(), otr_avatar_bounds_.height(), 
      MirroredLeftPointForRect(otr_avatar_bounds_), otr_avatar_bounds_.y(),
      otr_avatar_bounds_.width(), otr_avatar_bounds_.height(), false);
}

void AeroGlassNonClientView::PaintDistributorLogo(ChromeCanvas* canvas) {
  // The distributor logo is only painted when the frame is not maximized and
  // when we actually have a logo.
  if (!frame_->IsMaximized() && !frame_->IsMinimized() && 
      !distributor_logo_.empty()) {
    canvas->DrawBitmapInt(distributor_logo_, logo_bounds_.x(),
                          logo_bounds_.y());
  }
}

void AeroGlassNonClientView::PaintToolbarBackground(ChromeCanvas* canvas) {
  SkBitmap* toolbar_left =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
  SkBitmap* toolbar_center =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
  SkBitmap* toolbar_right =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT);

  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  gfx::Point topleft(toolbar_bounds.x(), toolbar_bounds.y());
  View::ConvertPointToView(frame_->client_view(), this, &topleft);
  toolbar_bounds.set_x(topleft.x());
  toolbar_bounds.set_y(topleft.y());

  // We use TileImageInt for the left and right caps to clip the rendering
  // to the appropriate height of the toolbar.
  canvas->TileImageInt(*toolbar_left,
                       toolbar_bounds.x() - toolbar_left->width(),
                       toolbar_bounds.y(), toolbar_left->width(),
                       toolbar_bounds.height());
  canvas->TileImageInt(*toolbar_center,
                       toolbar_bounds.x(), toolbar_bounds.y(),
                       toolbar_bounds.width(), toolbar_center->height());
  canvas->TileImageInt(*toolbar_right, toolbar_bounds.right(),
                       toolbar_bounds.y(), toolbar_right->width(),
                       toolbar_bounds.height());
}

void AeroGlassNonClientView::PaintClientEdge(ChromeCanvas* canvas) {
  SkBitmap* right = resources_->GetPartBitmap(FRAME_CLIENT_EDGE_RIGHT);
  SkBitmap* bottom_right =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_RIGHT);
  SkBitmap* bottom = resources_->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM);
  SkBitmap* bottom_left =
      resources_->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_LEFT);
  SkBitmap* left = resources_->GetPartBitmap(FRAME_CLIENT_EDGE_LEFT);

  // The toolbar renders its own client edge in PaintToolbarBackground, however
  // there are other bands that need to have a client edge rendered along their
  // sides, such as the Bookmark bar, infobars, etc.
  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  gfx::Rect client_area_bounds = browser_view_->GetClientAreaBounds();
  // For some reason things don't line up quite right, so we add and subtract
  // pixels here and there for aesthetic bliss.
  // Enlarge the client area to include the toolbar, since the top edge of
  // the client area is the toolbar background and the client edge renders
  // the left and right sides of the toolbar background.
  client_area_bounds.SetRect(
      client_area_bounds.x(),
      frame_->client_view()->y() + toolbar_bounds.bottom() - kPixel,
      client_area_bounds.width(),
      std::max(0, height() - frame_->client_view()->y() -
          toolbar_bounds.bottom() + kPixel));

  int fudge = frame_->window_delegate()->ShouldShowWindowTitle() ? kPixel : 0;
  canvas->TileImageInt(*right, client_area_bounds.right(),
                       client_area_bounds.y() + fudge, right->width(),
                       client_area_bounds.height() - bottom_right->height() +
                           kPixel - fudge);
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right(),
                        client_area_bounds.bottom() - bottom_right->height() +
                            kPixel);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
                       client_area_bounds.bottom() - bottom_right->height() +
                           kPixel, client_area_bounds.width(),
                       bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
                        client_area_bounds.x() - bottom_left->width(),
                        client_area_bounds.bottom() - bottom_left->height() +
                            kPixel);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
                       client_area_bounds.y() + fudge, left->width(),
                       client_area_bounds.height() - bottom_left->height() +
                           kPixel - fudge);
}

void AeroGlassNonClientView::LayoutOTRAvatar() {
  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  int top_height = (frame_->IsMaximized() ?
      (CalculateNonClientTopHeight() - 2) : kTabStripY);
  int tabstrip_height = browser_view_->GetTabStripHeight() - kOTRBottomSpacing;
  int otr_height = frame_->IsMaximized() ?
      (tabstrip_height - kOTRMaximizedTopSpacing) :
      otr_avatar_icon.height();
  otr_avatar_bounds_.SetRect(kClientEdgeWidth + kOTRSideSpacing,
      top_height + tabstrip_height - otr_height, otr_avatar_icon.width(),
      otr_height);
}

void AeroGlassNonClientView::LayoutDistributorLogo() {
  if (distributor_logo_.empty())
    return;

  int logo_w = distributor_logo_.width();
  int logo_h = distributor_logo_.height();
  
  int w = width();
  int mbx = frame_->GetMinimizeButtonOffset();

  logo_bounds_.SetRect(
      width() - frame_->GetMinimizeButtonOffset() - logo_w,
      kDistributorLogoVerticalOffset, logo_w, logo_h);
}

void AeroGlassNonClientView::LayoutClientView() {
  gfx::Rect client_bounds = CalculateClientAreaBounds(width(), height());
  frame_->client_view()->SetBounds(client_bounds);
}

// static
void AeroGlassNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resources_ = new AeroGlassWindowResources;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
#if defined(GOOGLE_CHROME_BUILD)
    distributor_logo_ = *rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO);
#endif

    initialized = true;
  }
}

