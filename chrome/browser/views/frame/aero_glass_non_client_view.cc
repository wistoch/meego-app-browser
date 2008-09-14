// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/aero_glass_non_client_view.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/frame/browser_view2.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/window_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/client_view.h"
#include "chrome/views/window_delegate.h"

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

  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }
  virtual const ChromeFont& GetTitleFont() const {
    return title_font_;
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
      title_font_ =
          rb.GetFont(ResourceBundle::BaseFont).DeriveFont(1, ChromeFont::BOLD);

      app_top_left_ = *rb.GetBitmapNamed(IDR_APP_TOP_LEFT);
      app_top_center_ = *rb.GetBitmapNamed(IDR_APP_TOP_CENTER);
      app_top_right_ = *rb.GetBitmapNamed(IDR_APP_TOP_RIGHT);      

      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;
  static SkBitmap app_top_left_;
  static SkBitmap app_top_center_;
  static SkBitmap app_top_right_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassWindowResources);
};

// static
SkBitmap* AeroGlassWindowResources::standard_frame_bitmaps_[];
ChromeFont AeroGlassWindowResources::title_font_;
SkBitmap AeroGlassWindowResources::app_top_left_;
SkBitmap AeroGlassWindowResources::app_top_center_;
SkBitmap AeroGlassWindowResources::app_top_right_;

AeroGlassWindowResources* AeroGlassNonClientView::resources_ = NULL;
SkBitmap AeroGlassNonClientView::distributor_logo_;

// The distance between the top of the TabStrip and the top of the non-client
// area of the window.
static const int kNoTitleTopSpacing = 8;
// The width of the client edge to the left and right of the window.
static const int kWindowHorizontalClientEdgeWidth = 2;
// The height of the client edge to the bottom of the window.
static const int kWindowBottomClientEdgeHeight = 2;
// The horizontal distance between the left of the minimize button and the
// right edge of the distributor logo.
static const int kDistributorLogoHorizontalOffset = 7;
// The distance from the top of the non-client view and the top edge of the
// distributor logo.
static const int kDistributorLogoVerticalOffset = 3;
// The distance of the TabStrip from the top of the window's client area.
static const int kTabStripY = 14;
// A single pixel.
static const int kPixel = 1;
// The height of the sizing border.
static const int kWindowSizingBorderSize = 8;
// The size (width/height) of the window icon.
static const int kWindowIconSize = 16;

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, public:

AeroGlassNonClientView::AeroGlassNonClientView(AeroGlassFrame* frame,
                                               BrowserView2* browser_view)
    : frame_(frame),
      browser_view_(browser_view) {
  InitClass();
}

AeroGlassNonClientView::~AeroGlassNonClientView() {
}

gfx::Rect AeroGlassNonClientView::GetBoundsForTabStrip(TabStrip* tabstrip) {
  // If we are maximized, the tab strip will be in line with the window
  // controls, so we need to make sure they don't overlap.
  int tabstrip_width = browser_view_->GetWidth();
  if(frame_->IsMaximized()) {
    TITLEBARINFOEX titlebar_info;
    titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
    SendMessage(frame_->GetHWND(), WM_GETTITLEBARINFOEX, 0,
      reinterpret_cast<WPARAM>(&titlebar_info));

    // rgrect[2] refers to the minimize button.
    tabstrip_width -= (tabstrip_width - titlebar_info.rgrect[2].left);
  }
  int tabstrip_height = tabstrip->GetPreferredHeight();
  int tabstrip_y = frame_->IsMaximized() ? 0 : kTabStripY;
  return gfx::Rect(0, tabstrip_y, tabstrip_width, tabstrip_height);
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, ChromeViews::NonClientView implementation:

gfx::Rect AeroGlassNonClientView::CalculateClientAreaBounds(int width,
                                                            int height) const {
  if (!browser_view_->IsToolbarVisible()) {
    // App windows don't have a toolbar.
    return gfx::Rect(0, 0, GetWidth(), GetHeight());
  }

  int top_margin = CalculateNonClientTopHeight();
  return gfx::Rect(kWindowHorizontalClientEdgeWidth, top_margin,
      std::max(0, width - (2 * kWindowHorizontalClientEdgeWidth)),
      std::max(0, height - top_margin - kWindowBottomClientEdgeHeight));
}

gfx::Size AeroGlassNonClientView::CalculateWindowSizeForClientSize(
    int width,
    int height) const {
  int top_margin = CalculateNonClientTopHeight();
  return gfx::Size(width + (2 * kWindowHorizontalClientEdgeWidth),
                   height + top_margin + kWindowBottomClientEdgeHeight);
}

CPoint AeroGlassNonClientView::GetSystemMenuPoint() const {
  CPoint offset(0, 0);
  MapWindowPoints(GetViewContainer()->GetHWND(), HWND_DESKTOP, &offset, 1);
  return offset;
}

int AeroGlassNonClientView::NonClientHitTest(const gfx::Point& point) {
  CRect bounds;
  CPoint test_point = point.ToPOINT();

  // See if the client view intersects the non-client area (e.g. blank areas
  // of the TabStrip).
  int component = frame_->client_view()->NonClientHitTest(point);
  if (component != HTNOWHERE)
    return component;

  // This check is only done when we have a toolbar, which is the only time
  // that we have a non-standard non-client area.
  if (browser_view_->IsToolbarVisible()) {
    // Because we tell Windows that our client area extends all the way to the
    // top of the browser window, but our BrowserView doesn't actually go up that
    // high, we need to make sure the right hit-test codes are returned for the
    // caption area above the tabs and the top sizing border.
    int client_view_right =
        frame_->client_view()->GetX() + frame_->client_view()->GetWidth();
    if (point.x() >= frame_->client_view()->GetX() &&
        point.x() < client_view_right) {
      if (point.y() < kWindowSizingBorderSize)
        return HTTOP;
      return HTCAPTION;
    }
  }

  // Let Windows figure it out.
  return HTNOWHERE;
}

void AeroGlassNonClientView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
}

void AeroGlassNonClientView::EnableClose(bool enable) {
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, ChromeViews::View overrides:

void AeroGlassNonClientView::Paint(ChromeCanvas* canvas) {
  PaintDistributorLogo(canvas);
  if (browser_view_->IsToolbarVisible()) {
    PaintToolbarBackground(canvas);
    PaintClientEdge(canvas);
  }
}

void AeroGlassNonClientView::Layout() {
  LayoutDistributorLogo();
  LayoutClientView();
}

void AeroGlassNonClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  frame_->client_view()->GetPreferredSize(out);
  out->cx += 2 * kWindowHorizontalClientEdgeWidth;
  out->cy += CalculateNonClientTopHeight() + kWindowBottomClientEdgeHeight;
}

void AeroGlassNonClientView::DidChangeBounds(const CRect& previous,
                                          const CRect& current) {
  Layout();
}

void AeroGlassNonClientView::ViewHierarchyChanged(bool is_add,
                                               ChromeViews::View* parent,
                                               ChromeViews::View* child) {
  if (is_add && child == this) {
    DCHECK(GetViewContainer());
    DCHECK(frame_->client_view()->GetParent() != this);
    AddChildView(frame_->client_view());
  }
}

///////////////////////////////////////////////////////////////////////////////
// AeroGlassNonClientView, private:

int AeroGlassNonClientView::CalculateNonClientTopHeight() const {
  if (frame_->window_delegate()->ShouldShowWindowTitle())
    return browser_view_->IsToolbarVisible() ? 2 : 0;
  return kNoTitleTopSpacing;
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
  if (browser_view_->IsToolbarVisible() ||
      browser_view_->IsTabStripVisible()) {
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

    if (frame_->window_delegate()->ShouldShowWindowTitle()) {
      // Since we're showing the toolbar or the tabstrip, we need to draw a
      // single pixel grey line along underneath them to terminate them
      // cleanly.
      canvas->FillRectInt(SkColorSetRGB(180, 188, 199), toolbar_bounds.x(),
                          toolbar_bounds.bottom() - 1, toolbar_bounds.width(),
                          1);
    }
  }
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
      frame_->client_view()->GetY() + toolbar_bounds.bottom() - kPixel,
      client_area_bounds.width(),
      std::max(0, GetHeight() - frame_->client_view()->GetY() -
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

void AeroGlassNonClientView::LayoutDistributorLogo() {
  if (distributor_logo_.empty())
    return;

  int logo_w = distributor_logo_.width();
  int logo_h = distributor_logo_.height();
  
  int w = GetWidth();
  int mbx = frame_->GetMinimizeButtonOffset();

  logo_bounds_.SetRect(
      GetWidth() - frame_->GetMinimizeButtonOffset() - logo_w,
      kDistributorLogoVerticalOffset, logo_w, logo_h);
}

void AeroGlassNonClientView::LayoutClientView() {
  gfx::Rect client_bounds(
      CalculateClientAreaBounds(GetWidth(), GetHeight()));
  frame_->client_view()->SetBounds(client_bounds.ToRECT());
}

// static
void AeroGlassNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resources_ = new AeroGlassWindowResources;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap* image = rb.GetBitmapNamed(IDR_DISTRIBUTOR_LOGO);
    if (!image->isNull())
      distributor_logo_ = *image;

    initialized = true;
  }
}

