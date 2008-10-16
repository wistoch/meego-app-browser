// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/custom_frame_window.h"

#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "base/win_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/client_view.h"
#include "chrome/views/native_button.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/root_view.h"
#include "chrome/views/window_delegate.h"
#include "chrome/views/window_resources.h"
#include "generated_resources.h"

namespace ChromeViews {

// A scoping class that removes the WS_VISIBLE style of a window.
//
// Why would we want such a thing? Well, it turns out Windows has some
// "unorthodox" behavior when it comes to painting its non-client areas.
// Sadly, the default implementation of some messages, e.g. WM_SETTEXT and
// WM_SETICON actually paint all or parts of the native title bar of the
// application. That's right, they just paint it. They don't go through
// WM_NCPAINT or anything like that that we already override. What this means
// is that we end up with occasional flicker of bits of the normal Windows
// title bar whenever we do things like change the title text, or right click
// on the caption. The solution turns out to be to handle these messages,
// use this scoped object to remove the WS_VISIBLE style which prevents this
// rendering from happening, call the default window procedure, then add the
// WS_VISIBLE style back when this object goes out of scope.
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
//
// *** Sigh. ***
class ScopedVisibilityRemover {
 public:
  explicit ScopedVisibilityRemover(HWND hwnd)
      : hwnd_(hwnd),
        window_style_(0) {
    window_style_ = GetWindowLong(hwnd_, GWL_STYLE);
    if (window_style_ & WS_VISIBLE)
      SetWindowLong(hwnd_, GWL_STYLE, window_style_ & ~WS_VISIBLE);
  }

  ~ScopedVisibilityRemover() {
    if (window_style_ & WS_VISIBLE)
      SetWindowLong(hwnd_, GWL_STYLE, window_style_);
  }

 private:
  // The window having its style changed.
  HWND hwnd_;

  // The original style of the window, including WS_VISIBLE if present.
  DWORD window_style_;
};

HCURSOR CustomFrameWindow::resize_cursors_[6];

// An enumeration of bitmap resources used by this window.
enum {
  FRAME_PART_BITMAP_FIRST = 0,  // Must be first.

  // Window Controls.
  FRAME_CLOSE_BUTTON_ICON,
  FRAME_CLOSE_BUTTON_ICON_H,
  FRAME_CLOSE_BUTTON_ICON_P,
  FRAME_CLOSE_BUTTON_ICON_SA,
  FRAME_CLOSE_BUTTON_ICON_SA_H,
  FRAME_CLOSE_BUTTON_ICON_SA_P,
  FRAME_RESTORE_BUTTON_ICON,
  FRAME_RESTORE_BUTTON_ICON_H,
  FRAME_RESTORE_BUTTON_ICON_P,
  FRAME_MAXIMIZE_BUTTON_ICON,
  FRAME_MAXIMIZE_BUTTON_ICON_H,
  FRAME_MAXIMIZE_BUTTON_ICON_P,
  FRAME_MINIMIZE_BUTTON_ICON,
  FRAME_MINIMIZE_BUTTON_ICON_H,
  FRAME_MINIMIZE_BUTTON_ICON_P,

  // Window Frame Border.
  FRAME_BOTTOM_EDGE,
  FRAME_BOTTOM_LEFT_CORNER,
  FRAME_BOTTOM_RIGHT_CORNER,
  FRAME_LEFT_EDGE,
  FRAME_RIGHT_EDGE,
  FRAME_TOP_EDGE,
  FRAME_TOP_LEFT_CORNER,
  FRAME_TOP_RIGHT_CORNER,

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

class ActiveWindowResources : public WindowResources {
 public:
  ActiveWindowResources() {
    InitClass();
  }
  virtual ~ActiveWindowResources() {
  }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
        IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
        IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
        IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
        IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
        IDR_WINDOW_BOTTOM_CENTER, IDR_WINDOW_BOTTOM_LEFT_CORNER,
            IDR_WINDOW_BOTTOM_RIGHT_CORNER, IDR_WINDOW_LEFT_SIDE,
            IDR_WINDOW_RIGHT_SIDE, IDR_WINDOW_TOP_CENTER,
            IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_RIGHT_CORNER,
        IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER, IDR_APP_TOP_RIGHT,
            IDR_CONTENT_RIGHT_SIDE, IDR_CONTENT_BOTTOM_RIGHT_CORNER,
            IDR_CONTENT_BOTTOM_CENTER, IDR_CONTENT_BOTTOM_LEFT_CORNER,
            IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ActiveWindowResources);
};

class InactiveWindowResources : public WindowResources {
 public:
  InactiveWindowResources() {
    InitClass();
  }
  virtual ~InactiveWindowResources() {
  }

  // WindowResources implementation:
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const {
    return standard_frame_bitmaps_[part];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      static const int kFramePartBitmapIds[] = {
        0,
        IDR_CLOSE, IDR_CLOSE_H, IDR_CLOSE_P,
        IDR_CLOSE_SA, IDR_CLOSE_SA_H, IDR_CLOSE_SA_P,
        IDR_RESTORE, IDR_RESTORE_H, IDR_RESTORE_P,
        IDR_MAXIMIZE, IDR_MAXIMIZE_H, IDR_MAXIMIZE_P,
        IDR_MINIMIZE, IDR_MINIMIZE_H, IDR_MINIMIZE_P,
        IDR_DEWINDOW_BOTTOM_CENTER, IDR_DEWINDOW_BOTTOM_LEFT_CORNER,
            IDR_DEWINDOW_BOTTOM_RIGHT_CORNER, IDR_DEWINDOW_LEFT_SIDE,
            IDR_DEWINDOW_RIGHT_SIDE, IDR_DEWINDOW_TOP_CENTER,
            IDR_DEWINDOW_TOP_LEFT_CORNER, IDR_DEWINDOW_TOP_RIGHT_CORNER,
        IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER, IDR_APP_TOP_RIGHT,
            IDR_CONTENT_RIGHT_SIDE, IDR_CONTENT_BOTTOM_RIGHT_CORNER,
            IDR_CONTENT_BOTTOM_CENTER, IDR_CONTENT_BOTTOM_LEFT_CORNER,
            IDR_CONTENT_LEFT_SIDE,
        0
      };

      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kFramePartBitmapIds[i];
        if (id != 0)
          standard_frame_bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* standard_frame_bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_EVIL_CONSTRUCTORS(InactiveWindowResources);
};

// static
SkBitmap* ActiveWindowResources::standard_frame_bitmaps_[];
SkBitmap* InactiveWindowResources::standard_frame_bitmaps_[];


///////////////////////////////////////////////////////////////////////////////
//
// DefaultNonClientView
//
//  A ChromeView that provides the "frame" for CustomFrameWindows. This means
//  rendering the non-standard window caption, border, and controls.
//
////////////////////////////////////////////////////////////////////////////////
class DefaultNonClientView : public NonClientView,
                             public BaseButton::ButtonListener {
 public:
  explicit DefaultNonClientView(CustomFrameWindow* container);
  virtual ~DefaultNonClientView();

  // Overridden from CustomFrameWindow::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);

  // View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // BaseButton::ButtonListener implementation:
  virtual void ButtonPressed(BaseButton* sender);

 private:
  // Updates the system menu icon button.
  void SetWindowIcon(SkBitmap window_icon);

  // Returns the height of the non-client area at the top of the window (the
  // title bar, etc).
  int CalculateContentsTop() const;

  // Paint various sub-components of this view.
  void PaintFrameBorder(ChromeCanvas* canvas);
  void PaintMaximizedFrameBorder(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  // Returns the resource collection to be used when rendering the window.
  WindowResources* resources() const {
    return container_->is_active() || paint_as_active() ? active_resources_
                                                        : inactive_resources_;
  }

  // The View that provides the background for the window, and optionally
  // dialog buttons. Note: the non-client view does _not_ own this view, the
  // container does.
  ClientView* client_view_;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // Window controls.
  Button* close_button_;
  Button* restore_button_;
  Button* maximize_button_;
  Button* minimize_button_;
  Button* system_menu_button_;  // Uses the window icon if visible.
  bool should_show_minmax_buttons_;

  // The window icon.
  SkBitmap window_icon_;

  // The window that owns this view.
  CustomFrameWindow* container_;

  // Initialize various static resources.
  static void InitClass();
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(DefaultNonClientView);
};

// static
WindowResources* DefaultNonClientView::active_resources_ = NULL;
WindowResources* DefaultNonClientView::inactive_resources_ = NULL;
ChromeFont DefaultNonClientView::title_font_;
static const int kWindowControlsTopOffset = 1;
static const int kWindowControlsRightOffset = 5;
static const int kWindowControlsTopZoomedOffset = 1;
static const int kWindowControlsRightZoomedOffset = 5;
static const int kWindowTopMarginZoomed = 1;
static const int kWindowIconLeftOffset = 5;
static const int kWindowIconTopOffset = 5;
static const int kTitleTopOffset = 6;
static const int kWindowIconTitleSpacing = 3;
static const int kTitleBottomSpacing = 6;
static const int kNoTitleTopSpacing = 8;
static const int kResizeAreaSize = 5;
static const int kResizeAreaNorthSize = 3;
static const int kResizeAreaCornerSize = 16;
static const int kWindowHorizontalBorderSize = 4;
static const int kWindowVerticalBorderSize = 4;

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, public:

DefaultNonClientView::DefaultNonClientView(
    CustomFrameWindow* container)
    : NonClientView(),
      client_view_(NULL),
      close_button_(new Button),
      restore_button_(new Button),
      maximize_button_(new Button),
      minimize_button_(new Button),
      system_menu_button_(new Button),
      should_show_minmax_buttons_(false),
      container_(container) {
  InitClass();
  WindowResources* resources = active_resources_;

  close_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
  close_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
  close_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));
  close_button_->SetListener(this, -1);
  AddChildView(close_button_);

  restore_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON));
  restore_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_H));
  restore_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_RESTORE_BUTTON_ICON_P));
  restore_button_->SetListener(this, -1);
  AddChildView(restore_button_);

  maximize_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON));
  maximize_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_H));
  maximize_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_MAXIMIZE_BUTTON_ICON_P));
  maximize_button_->SetListener(this, -1);
  AddChildView(maximize_button_);

  minimize_button_->SetImage(
      Button::BS_NORMAL, resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON));
  minimize_button_->SetImage(
      Button::BS_HOT, resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_H));
  minimize_button_->SetImage(
      Button::BS_PUSHED, resources->GetPartBitmap(FRAME_MINIMIZE_BUTTON_ICON_P));
  minimize_button_->SetListener(this, -1);
  AddChildView(minimize_button_);

  AddChildView(system_menu_button_);
}

DefaultNonClientView::~DefaultNonClientView() {
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, CustomFrameWindow::NonClientView implementation:

gfx::Rect DefaultNonClientView::CalculateClientAreaBounds(
    int width, int height) const {
  int top_margin = CalculateContentsTop();
  return gfx::Rect(kWindowHorizontalBorderSize, top_margin,
      std::max(0, width - (2 * kWindowHorizontalBorderSize)),
      std::max(0, height - top_margin - kWindowVerticalBorderSize));
}

gfx::Size DefaultNonClientView::CalculateWindowSizeForClientSize(
    int width, int height) const {
  int contents_top = CalculateContentsTop();
  return gfx::Size(
      width + (2 * kWindowHorizontalBorderSize),
      height + kWindowVerticalBorderSize + contents_top);
}

CPoint DefaultNonClientView::GetSystemMenuPoint() const {
  CPoint system_menu_point(
      system_menu_button_->x(),
      system_menu_button_->y() + system_menu_button_->height());
  MapWindowPoints(container_->GetHWND(), HWND_DESKTOP, &system_menu_point, 1);
  return system_menu_point;
}

// There is a subtle point that needs to be explained regarding the manner in
// which this function returns the HT* code Windows is expecting:
//
// |point| contains the cursor position in this View's coordinate system. If
// this View uses a right-to-left UI layout, the position represented by
// |point| will not reflect the UI mirroring because we don't create the
// container's HWND with WS_EX_LAYOUTRTL. Therefore, whenever the cursor
// position resides within the boundaries of one of our child Views (for
// example, the close_button_), we must retrieve the child View bounds such
// that bound are mirrored if the View uses right-to-left UI layout. This is
// why this function passes APPLY_MIRRORING_TRANSFORMATION as the |settings|
// whenever it calls GetBounds().
int DefaultNonClientView::NonClientHitTest(const gfx::Point& point) {
  // First see if it's within the grow box area, since that overlaps the client
  // bounds.
  int component = container_->client_view()->NonClientHitTest(point);
  if (component != HTNOWHERE)
    return component;

  // Then see if the point is within any of the window controls.
  gfx::Rect button_bounds =
      close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTCLOSE;
  button_bounds = restore_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTMAXBUTTON;
  button_bounds = maximize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTMAXBUTTON;
  button_bounds = minimize_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTMINBUTTON;
  button_bounds =
      system_menu_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
  if (button_bounds.Contains(point))
    return HTSYSMENU;

  component = GetHTComponentForFrame(
      point,
      kResizeAreaSize,
      kResizeAreaCornerSize,
      kResizeAreaNorthSize,
      container_->window_delegate()->CanResize());
  if (component == HTNOWHERE) {
    // Finally fall back to the caption.
    if (bounds().Contains(point))
      component = HTCAPTION;
    // Otherwise, the point is outside the window's bounds.
  }
  return component;
}

void DefaultNonClientView::GetWindowMask(const gfx::Size& size,
                                         gfx::Path* window_mask) {
  DCHECK(window_mask);

  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 3);
  window_mask->lineTo(SkIntToScalar(size.width()), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
  window_mask->close();
}

void DefaultNonClientView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, View overrides:

void DefaultNonClientView::Paint(ChromeCanvas* canvas) {
  if (container_->IsMaximized()) {
    PaintMaximizedFrameBorder(canvas);
  } else {
    PaintFrameBorder(canvas);
  }
  PaintClientEdge(canvas);

  WindowDelegate* d = container_->window_delegate();
  if (d->ShouldShowWindowTitle()) {
    canvas->DrawStringInt(d->GetWindowTitle(), title_font_, SK_ColorWHITE,
                          title_bounds_.x(), title_bounds_.y(),
                          title_bounds_.width(), title_bounds_.height());
  }
}

void DefaultNonClientView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  LayoutClientView();
  SchedulePaint();
}

gfx::Size DefaultNonClientView::GetPreferredSize() {
  gfx::Size prefsize = container_->client_view()->GetPreferredSize();
  prefsize.Enlarge(2 * kWindowHorizontalBorderSize,
                   CalculateContentsTop() + kWindowVerticalBorderSize);
  return prefsize;
}

void DefaultNonClientView::ViewHierarchyChanged(bool is_add,
                                                View* parent,
                                                View* child) {
  // Add our Client View as we are added to the Container so that if we are
  // subsequently resized all the parent-child relationships are established.
  if (is_add && GetContainer() && child == this)
    AddChildView(container_->client_view());
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, BaseButton::ButtonListener implementation:

void DefaultNonClientView::ButtonPressed(BaseButton* sender) {
  if (sender == close_button_) {
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
  } else if (sender == minimize_button_) {
    container_->ExecuteSystemMenuCommand(SC_MINIMIZE);
  } else if (sender == maximize_button_) {
    container_->ExecuteSystemMenuCommand(SC_MAXIMIZE);
  } else if (sender == restore_button_) {
    container_->ExecuteSystemMenuCommand(SC_RESTORE);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DefaultNonClientView, private:

void DefaultNonClientView::SetWindowIcon(SkBitmap window_icon) {
  // TODO(beng): (Cleanup) remove this persistent cache of the icon when Button
  //             takes a SkBitmap rather than SkBitmap*.
  window_icon_ = window_icon;
  system_menu_button_->SetImage(Button::BS_NORMAL, &window_icon);
}

int DefaultNonClientView::CalculateContentsTop() const {
  if (container_->window_delegate()->ShouldShowWindowTitle())
    return kTitleTopOffset + title_font_.height() + kTitleBottomSpacing;
  return kNoTitleTopSpacing;
}

void DefaultNonClientView::PaintFrameBorder(ChromeCanvas* canvas) {
  SkBitmap* top_left_corner =
      resources()->GetPartBitmap(FRAME_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      resources()->GetPartBitmap(FRAME_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_TOP_EDGE);
  SkBitmap* right_edge = resources()->GetPartBitmap(FRAME_RIGHT_EDGE);
  SkBitmap* left_edge = resources()->GetPartBitmap(FRAME_LEFT_EDGE);
  SkBitmap* bottom_left_corner =
      resources()->GetPartBitmap(FRAME_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
    resources()->GetPartBitmap(FRAME_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = resources()->GetPartBitmap(FRAME_BOTTOM_EDGE);

  // Top.
  canvas->DrawBitmapInt(*top_left_corner, 0, 0);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(*top_right_corner,
                        width() - top_right_corner->width(), 0);

  // Right.
  int top_stack_height = top_right_corner->height();
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
                       top_stack_height, right_edge->width(),
                       height() - top_stack_height -
                           bottom_right_corner->height());

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
  top_stack_height = top_left_corner->height();
  canvas->TileImageInt(*left_edge, 0, top_stack_height, left_edge->width(),
                       height() - top_stack_height -
                           bottom_left_corner->height());
}

void DefaultNonClientView::PaintMaximizedFrameBorder(
    ChromeCanvas* canvas) {
  SkBitmap* top_edge = resources()->GetPartBitmap(FRAME_TOP_EDGE);
  SkBitmap* bottom_edge =
      resources()->GetPartBitmap(FRAME_BOTTOM_EDGE);
  canvas->TileImageInt(*top_edge, 0, 0, width(), top_edge->height());
  canvas->TileImageInt(*bottom_edge, 0, height() - bottom_edge->height(),
                       width(), bottom_edge->height());
}

void DefaultNonClientView::PaintClientEdge(ChromeCanvas* canvas) {
  SkBitmap* top_left = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_LEFT);
  SkBitmap* top = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP);
  SkBitmap* top_right = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_TOP_RIGHT);
  SkBitmap* right = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_RIGHT);
  SkBitmap* bottom_right =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_RIGHT);
  SkBitmap* bottom = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM);
  SkBitmap* bottom_left =
      resources()->GetPartBitmap(FRAME_CLIENT_EDGE_BOTTOM_LEFT);
  SkBitmap* left = resources()->GetPartBitmap(FRAME_CLIENT_EDGE_LEFT);

  gfx::Rect client_area_bounds = container_->client_view()->bounds();

  canvas->DrawBitmapInt(*top_left, client_area_bounds.x() - top_left->width(),
                        client_area_bounds.y() - top->height());
  canvas->TileImageInt(*top, client_area_bounds.x(),
                       client_area_bounds.y() - top->height(),
                       client_area_bounds.width(), top->height());
  canvas->DrawBitmapInt(*top_right, client_area_bounds.right(),
                        client_area_bounds.y() - top->height());
  canvas->TileImageInt(*right, client_area_bounds.right(),
                       client_area_bounds.y() - top->height() +
                           top_right->height(),
                       right->width(), client_area_bounds.height());
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right(),
                        client_area_bounds.bottom());
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
                       client_area_bounds.bottom(),
                       client_area_bounds.width(), bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
                        client_area_bounds.x() - bottom_left->width(),
                        client_area_bounds.bottom());
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
                       client_area_bounds.y() - top->height() +
                           top_left->height(),
                       left->width(), client_area_bounds.height());
}

void DefaultNonClientView::LayoutWindowControls() {
  gfx::Size ps;
  if (container_->IsMaximized() || container_->IsMinimized()) {
    maximize_button_->SetVisible(false);
    restore_button_->SetVisible(true);
  }

  if (container_->IsMaximized()) {
    ps = close_button_->GetPreferredSize();
    close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_BOTTOM);
    close_button_->SetBounds(
        width() - ps.width() - kWindowControlsRightZoomedOffset,
        0, ps.width() + kWindowControlsRightZoomedOffset,
        ps.height() + kWindowControlsTopZoomedOffset);

    if (should_show_minmax_buttons_) {
      ps = restore_button_->GetPreferredSize();
      restore_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                         Button::ALIGN_BOTTOM);
      restore_button_->SetBounds(close_button_->x() - ps.width(), 0,
                                 ps.width(),
                                 ps.height() + kWindowControlsTopZoomedOffset);

      ps = minimize_button_->GetPreferredSize();
      minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_BOTTOM);
      minimize_button_->SetBounds(restore_button_->x() - ps.width(), 0,
                                  ps.width(),
                                  ps.height() + kWindowControlsTopZoomedOffset);
    }
  } else if (container_->IsMinimized()) {
    ps = close_button_->GetPreferredSize();
    close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_BOTTOM);
    close_button_->SetBounds(
        width() - ps.width() - kWindowControlsRightZoomedOffset,
        0, ps.width() + kWindowControlsRightZoomedOffset,
        ps.height() + kWindowControlsTopZoomedOffset);

    if (should_show_minmax_buttons_) {
      ps = restore_button_->GetPreferredSize();
      restore_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                         Button::ALIGN_BOTTOM);
      restore_button_->SetBounds(close_button_->x() - ps.width(), 0,
                                 ps.width(),
                                 ps.height() + kWindowControlsTopZoomedOffset);

      ps = minimize_button_->GetPreferredSize();
      minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_BOTTOM);
      minimize_button_->SetBounds(restore_button_->x() - ps.width(), 0,
                                  ps.width(),
                                  ps.height() +
                                      kWindowControlsTopZoomedOffset);
    }
  } else {
    ps = close_button_->GetPreferredSize();
    close_button_->SetImageAlignment(Button::ALIGN_LEFT, Button::ALIGN_TOP);
    close_button_->SetBounds(width() - kWindowControlsRightOffset - ps.width(),
                             kWindowControlsTopOffset, ps.width(),
                             ps.height());

    if (should_show_minmax_buttons_) {
      close_button_->SetImage(
          Button::BS_NORMAL,
          active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON));
      close_button_->SetImage(
          Button::BS_HOT,
          active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_H));
      close_button_->SetImage(
          Button::BS_PUSHED,
          active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_P));

      restore_button_->SetVisible(false);

      maximize_button_->SetVisible(true);
      ps = maximize_button_->GetPreferredSize();
      maximize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_TOP);
      maximize_button_->SetBounds(close_button_->x() - ps.width(),
                                  kWindowControlsTopOffset, ps.width(),
                                  ps.height());

      ps = minimize_button_->GetPreferredSize();
      minimize_button_->SetImageAlignment(Button::ALIGN_LEFT,
                                          Button::ALIGN_TOP);
      minimize_button_->SetBounds(maximize_button_->x() - ps.width(),
                                  kWindowControlsTopOffset, ps.width(),
                                  ps.height());
    }
  }
  if (!should_show_minmax_buttons_) {
    close_button_->SetImage(
        Button::BS_NORMAL,
        active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_SA));
    close_button_->SetImage(
        Button::BS_HOT,
        active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_SA_H));
    close_button_->SetImage(
        Button::BS_PUSHED,
        active_resources_->GetPartBitmap(FRAME_CLOSE_BUTTON_ICON_SA_P));

    restore_button_->SetVisible(false);
    maximize_button_->SetVisible(false);
    minimize_button_->SetVisible(false);
  }
}

void DefaultNonClientView::LayoutTitleBar() {
  int top_offset = container_->IsMaximized() ? kWindowTopMarginZoomed : 0;
  WindowDelegate* d = container_->window_delegate();

  // Size the window icon, if visible.
  if (d->ShouldShowWindowIcon()) {
    system_menu_button_->SetVisible(true);
    gfx::Size ps = system_menu_button_->GetPreferredSize();
    system_menu_button_->SetBounds(
        kWindowIconLeftOffset, kWindowIconTopOffset + top_offset, ps.width(),
        ps.height());
  } else {
    // Put the menu in the right place at least even if it is hidden so we
    // can size the title based on its position.
    system_menu_button_->SetBounds(kWindowIconLeftOffset,
                                   kWindowIconTopOffset, 0, 0);
  }

  // Size the title, if visible.
  if (d->ShouldShowWindowTitle()) {
    gfx::Rect system_menu_bounds = system_menu_button_->bounds();
    int spacing = d->ShouldShowWindowIcon() ? kWindowIconTitleSpacing : 0;
    int title_right = should_show_minmax_buttons_ ?
        minimize_button_->x() : close_button_->x();
    int title_left = system_menu_bounds.right() + spacing;
    title_bounds_.SetRect(title_left, kTitleTopOffset + top_offset,
        std::max(0, static_cast<int>(title_right - system_menu_bounds.right())),
        title_font_.height());

    // We draw the custom frame window's title directly rather than using a
    // ChromeViews::Label child view. Therefore, we have to mirror the title
    // position manually if the View's UI layout is right-to-left. Child Views
    // are automatically mirrored, which means that the parent view doesn't
    // need to manually modify their position depending on the View's UI
    // layout.
    //
    // Mirroring the title's position manually is certainly far from being
    // elegant, but we have no choice (other than changing the
    // DefaultNonClientView subclass to use a ChromeView::Label as a child View
    // instead of drawing the title's text directly on the canvas).
    title_bounds_.set_x(MirroredLeftPointForRect(title_bounds_));

    // Center the icon within the height of the title if the title is taller.
    int delta_y = title_bounds_.height() - system_menu_button_->height();
    if (delta_y > 0) {
      int new_y = title_bounds_.y() + static_cast<int>(delta_y / 2);
      system_menu_button_->SetBounds(system_menu_button_->x(), new_y,
                                     system_menu_button_->width(),
                                     system_menu_button_->height());
    }
  }
}

void DefaultNonClientView::LayoutClientView() {
  gfx::Rect client_bounds(
      CalculateClientAreaBounds(width(), height()));
  container_->client_view()->SetBounds(client_bounds.ToRECT());
}

// static
void DefaultNonClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    active_resources_ = new ActiveWindowResources;
    inactive_resources_ = new InactiveWindowResources;
    title_font_ = win_util::GetWindowTitleFont();
    initialized = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// NonClientViewLayout

class NonClientViewLayout : public ChromeViews::LayoutManager {
 public:
  // The size of the default window border and padding used by Windows Vista
  // with DWM disabled when clipping the window for maximized display.
  // TODO(beng): figure out how to get this programmatically, since it varies
  //             with adjustments to the Windows Border/Padding setting.
  static const int kBorderAndPadding = 8;

  NonClientViewLayout(ChromeViews::View* child,
                      ChromeViews::Window* window)
      : child_(child),
        window_(window) {
  }
  virtual ~NonClientViewLayout() {}

  // Overridden from ChromeViews::LayoutManager:
  virtual void Layout(ChromeViews::View* host) {
    int horizontal_border_width =
        window_->IsMaximized() ? kBorderAndPadding : 0;
    int vertical_border_height =
        window_->IsMaximized() ? kBorderAndPadding : 0;

    child_->SetBounds(horizontal_border_width, vertical_border_height,
                      host->width() - (2 * horizontal_border_width),
                      host->height() - (2 * vertical_border_height));
  }
  virtual gfx::Size GetPreferredSize(ChromeViews::View* host) {
    return child_->GetPreferredSize();
  }

 private:
  ChromeViews::View* child_;
  ChromeViews::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(NonClientViewLayout);
};

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, public:

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate)
    : Window(window_delegate),
      is_active_(false) {
  InitClass();
  non_client_view_ = new DefaultNonClientView(this);
}

CustomFrameWindow::CustomFrameWindow(WindowDelegate* window_delegate,
                                     NonClientView* non_client_view)
    : Window(window_delegate) {
  InitClass();
  non_client_view_ = non_client_view;
}

CustomFrameWindow::~CustomFrameWindow() {
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, Window overrides:

void CustomFrameWindow::Init(HWND parent, const gfx::Rect& bounds) {
  // TODO(beng): (Cleanup) Right now, the only way to specify a different
  //             non-client view is to subclass this object and provide one
  //             by setting this member before calling Init.
  if (!non_client_view_)
    non_client_view_ = new DefaultNonClientView(this);
  Window::Init(parent, bounds);

  // Windows Vista non-Aero-glass does wacky things with maximized windows that
  // require a special layout manager to compensate for.
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    GetRootView()->SetLayoutManager(
        new NonClientViewLayout(non_client_view_, this));
  }

  ResetWindowRegion();
}

void CustomFrameWindow::SetClientView(ClientView* cv) {
  DCHECK(cv && !client_view() && GetHWND());
  set_client_view(cv);
  // For a CustomFrameWindow, the non-client view is the root.
  ContainerWin::SetContentsView(non_client_view_);
  // When the non client view is added to the view hierarchy, it will cause the
  // client view to be added as well.
}

gfx::Size CustomFrameWindow::CalculateWindowSizeForClientSize(
    const gfx::Size& client_size) const {
  return non_client_view_->CalculateWindowSizeForClientSize(
      client_size.width(), client_size.height());
}

void CustomFrameWindow::UpdateWindowTitle() {
  // Layout winds up causing the title to be re-validated during
  // string measurement.
  non_client_view_->Layout();
  // Must call the base class too so that places like the Task Bar get updated.
  Window::UpdateWindowTitle();
}

void CustomFrameWindow::UpdateWindowIcon() {
  // The icon will be re-validated during painting.
  non_client_view_->SchedulePaint();
  // Call the base class so that places like the Task Bar get updated.
  Window::UpdateWindowIcon();
}

void CustomFrameWindow::EnableClose(bool enable) {
  non_client_view_->EnableClose(enable);
  // Make sure the SysMenu changes to reflect this change as well.
  Window::EnableClose(enable);
}

void CustomFrameWindow::DisableInactiveRendering(bool disable) {
  Window::DisableInactiveRendering(disable);
  non_client_view_->set_paint_as_active(disable);
  if (!disable)
    non_client_view_->SchedulePaint();
}

void CustomFrameWindow::SizeWindowToDefault() {
  gfx::Size pref = client_view()->GetPreferredSize();
  DCHECK(pref.width() > 0 && pref.height() > 0);
  gfx::Size window_size =
      non_client_view_->CalculateWindowSizeForClientSize(pref.width(),
                                                         pref.height());
  win_util::CenterAndSizeWindow(owning_window(), GetHWND(),
                                window_size.ToSIZE(), false);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, ContainerWin overrides:

void CustomFrameWindow::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  // We handle this message so that we can make sure we interact nicely with
  // the taskbar on different edges of the screen and auto-hide taskbars.

  HMONITOR primary_monitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO primary_info;
  primary_info.cbSize = sizeof(primary_info);
  GetMonitorInfo(primary_monitor, &primary_info);

  minmax_info->ptMaxSize.x =
      primary_info.rcWork.right - primary_info.rcWork.left;
  minmax_info->ptMaxSize.y =
      primary_info.rcWork.bottom - primary_info.rcWork.top;

  HMONITOR target_monitor =
      MonitorFromWindow(GetHWND(), MONITOR_DEFAULTTONEAREST);
  MONITORINFO target_info;
  target_info.cbSize = sizeof(target_info);
  GetMonitorInfo(target_monitor, &target_info);

  minmax_info->ptMaxPosition.x =
      abs(target_info.rcWork.left - target_info.rcMonitor.left);
  minmax_info->ptMaxPosition.y =
      abs(target_info.rcWork.top - target_info.rcMonitor.top);

  // Work around task bar auto-hiding. By default the window is sized over the
  // top of the un-hide strip, so we adjust the size by a single pixel to make
  // it work. Because of the way Windows adjusts the target size rect for non
  // primary screens (it's quite daft), we only do this for the primary screen,
  // which I think should cover at least 95% of use cases.
  if ((target_monitor == primary_monitor) &&
      EqualRect(&target_info.rcWork, &target_info.rcMonitor)) {
    --minmax_info->ptMaxSize.y;
  }
}

static void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

void CustomFrameWindow::OnInitMenu(HMENU menu) {
  bool minimized = IsMinimized();
  bool maximized = IsMaximized();
  bool minimized_or_maximized = minimized || maximized;

  EnableMenuItem(menu, SC_RESTORE,
                 window_delegate()->CanMaximize() && minimized_or_maximized);
  EnableMenuItem(menu, SC_MOVE, !minimized_or_maximized);
  EnableMenuItem(menu, SC_SIZE,
                 window_delegate()->CanResize() && !minimized_or_maximized);
  EnableMenuItem(menu, SC_MAXIMIZE,
                 window_delegate()->CanMaximize() && !maximized);
  EnableMenuItem(menu, SC_MINIMIZE,
                 window_delegate()->CanMaximize() && !minimized);
}

void CustomFrameWindow::OnMouseLeave() {
  bool process_mouse_exited = true;
  POINT pt;
  if (GetCursorPos(&pt)) {
    LRESULT ht_component =
        ::SendMessage(GetHWND(), WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
    if (ht_component != HTNOWHERE) {
      // If the mouse moved into a part of the window's non-client area, then
      // don't send a mouse exited event since the mouse is still within the
      // bounds of the ChromeView that's rendering the frame. Note that we do
      // _NOT_ do this for windows with native frames, since in that case the
      // mouse really will have left the bounds of the RootView.
      process_mouse_exited = false;
    }
  }

  if (process_mouse_exited)
    ProcessMouseExited();
}

LRESULT CustomFrameWindow::OnNCActivate(BOOL active) {
  is_active_ = !!active;

  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (IsWindowVisible(GetHWND())) {
    non_client_view_->SchedulePaint();
    // We need to force a paint now, as a user dragging a window will block
    // painting operations while the move is in progress.
    PaintNow(root_view_->GetScheduledPaintRect());
  }

  return TRUE;
}

LRESULT CustomFrameWindow::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We need to repaint all when the window bounds change.
  return WVR_REDRAW;
}

LRESULT CustomFrameWindow::OnNCHitTest(const CPoint& point) {
  // NC points are in screen coordinates.
  CPoint temp = point;
  MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
  return non_client_view_->NonClientHitTest(gfx::Point(temp.x, temp.y));
}

struct ClipState {
  // The window being painted.
  HWND parent;

  // DC painting to.
  HDC dc;

  // Origin of the window in terms of the screen.
  int x;
  int y;
};

// See comments in OnNCPaint for details of this function.
static BOOL CALLBACK ClipDCToChild(HWND window, LPARAM param) {
  ClipState* clip_state = reinterpret_cast<ClipState*>(param);
  if (GetParent(window) == clip_state->parent && IsWindowVisible(window)) {
    RECT bounds;
    GetWindowRect(window, &bounds);
    ExcludeClipRect(clip_state->dc,
                    bounds.left - clip_state->x,
                    bounds.top - clip_state->y,
                    bounds.right - clip_state->x,
                    bounds.bottom - clip_state->y);
  }
  return TRUE;
}

void CustomFrameWindow::OnNCPaint(HRGN rgn) {
  // We have an NC region and need to paint it. We expand the NC region to
  // include the dirty region of the root view. This is done to minimize
  // paints.
  CRect window_rect;
  GetWindowRect(&window_rect);

  if (window_rect.Width() != root_view_->width() ||
      window_rect.Height() != root_view_->height()) {
    // If the size of the window differs from the size of the root view it
    // means we're being asked to paint before we've gotten a WM_SIZE. This can
    // happen when the user is interactively resizing the window. To avoid
    // mass flickering we don't do anything here. Once we get the WM_SIZE we'll
    // reset the region of the window which triggers another WM_NCPAINT and
    // all is well.
    return;
  }

  CRect dirty_region;
  // A value of 1 indicates paint all.
  if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
    dirty_region = CRect(0, 0, window_rect.Width(), window_rect.Height());
  } else {
    RECT rgn_bounding_box;
    GetRgnBox(rgn, &rgn_bounding_box);
    if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect))
      return;  // Dirty region doesn't intersect window bounds, bale.

    // rgn_bounding_box is in screen coordinates. Map it to window coordinates.
    OffsetRect(&dirty_region, -window_rect.left, -window_rect.top);
  }

  // In theory GetDCEx should do what we want, but I couldn't get it to work.
  // In particular the docs mentiond DCX_CLIPCHILDREN, but as far as I can tell
  // it doesn't work at all. So, instead we get the DC for the window then
  // manually clip out the children.
  HDC dc = GetWindowDC(GetHWND());
  ClipState clip_state;
  clip_state.x = window_rect.left;
  clip_state.y = window_rect.top;
  clip_state.parent = GetHWND();
  clip_state.dc = dc;
  EnumChildWindows(GetHWND(), &ClipDCToChild,
                   reinterpret_cast<LPARAM>(&clip_state));

  RootView* root_view = GetRootView();
  CRect old_paint_region = root_view->GetScheduledPaintRectConstrainedToSize();

  if (!old_paint_region.IsRectEmpty()) {
    // The root view has a region that needs to be painted. Include it in the
    // region we're going to paint.

    CRect tmp = dirty_region;
    UnionRect(&dirty_region, &tmp, &old_paint_region);
  }

  root_view->SchedulePaint(dirty_region, false);

  // ChromeCanvasPaints destructor does the actual painting. As such, wrap the
  // following in a block to force paint to occur so that we can release the dc.
  {
    ChromeCanvasPaint canvas(dc, opaque(), dirty_region.left, dirty_region.top,
                             dirty_region.Width(), dirty_region.Height());

    root_view->ProcessPaint(&canvas);
  }

  ReleaseDC(GetHWND(), dc);
}

void CustomFrameWindow::OnNCLButtonDown(UINT ht_component,
                                        const CPoint& point) {
  switch (ht_component) {
    case HTCLOSE:
    case HTMINBUTTON:
    case HTMAXBUTTON: {
      // When the mouse is pressed down in these specific non-client areas, we
      // need to tell the RootView to send the mouse pressed event (which sets
      // capture, allowing subsequent WM_LBUTTONUP (note, _not_ WM_NCLBUTTONUP)
      // to fire so that the appropriate WM_SYSCOMMAND can be sent by the
      // applicable button's ButtonListener. We _have_ to do this this way
      // rather than letting Windows just send the syscommand itself (as would
      // happen if we never did this dance) because for some insane reason
      // DefWindowProc for WM_NCLBUTTONDOWN also renders the pressed window
      // control button appearance, in the Windows classic style, over our
      // view! Ick! By handling this message we prevent Windows from doing this
      // undesirable thing, but that means we need to roll the sys-command
      // handling ourselves.
      CPoint temp = point;
      MapWindowPoints(HWND_DESKTOP, GetHWND(), &temp, 1);
      UINT flags = 0;
      if ((GetKeyState(VK_CONTROL) & 0x80) == 0x80)
        flags |= MK_CONTROL;
      if ((GetKeyState(VK_SHIFT) & 0x80) == 0x80)
        flags |= MK_SHIFT;
      flags |= MK_LBUTTON;
      ProcessMousePressed(temp, flags, false);
      SetMsgHandled(TRUE);
      return;
    }
    default:
      Window::OnNCLButtonDown(ht_component, point);
      break;
  }
}

LRESULT CustomFrameWindow::OnNCUAHDrawCaption(UINT msg, WPARAM w_param,
                                              LPARAM l_param) {
  // See comment in hwnd_view_container.h at the definition of
  // WM_NCUAHDRAWCAPTION for an explanation about why we need to handle this
  // message.
  SetMsgHandled(TRUE);
  return 0;
}

LRESULT CustomFrameWindow::OnNCUAHDrawFrame(UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  // See comment in hwnd_view_container.h at the definition of
  // WM_NCUAHDRAWCAPTION for an explanation about why we need to handle this
  // message.
  SetMsgHandled(TRUE);
  return 0;
}

LRESULT CustomFrameWindow::OnSetCursor(HWND window, UINT hittest_code,
                                       UINT message) {
  int index = RC_NORMAL;
  switch (hittest_code) {
    case HTTOP:
    case HTBOTTOM:
      index = RC_VERTICAL;
      break;
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
      index = RC_NWSE;
      break;
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
      index = RC_NESW;
      break;
    case HTLEFT:
    case HTRIGHT:
      index = RC_HORIZONTAL;
      break;
    case HTCAPTION:
    case HTCLIENT:
      index = RC_NORMAL;
      break;
  }
  SetCursor(resize_cursors_[index]);
  return 0;
}

LRESULT CustomFrameWindow::OnSetIcon(UINT size_type, HICON new_icon) {
  ScopedVisibilityRemover remover(GetHWND());
  return DefWindowProc(GetHWND(), WM_SETICON, size_type,
                       reinterpret_cast<LPARAM>(new_icon));
}

LRESULT CustomFrameWindow::OnSetText(const wchar_t* text) {
  ScopedVisibilityRemover remover(GetHWND());
  return DefWindowProc(GetHWND(), WM_SETTEXT, NULL,
                       reinterpret_cast<LPARAM>(text));
}

void CustomFrameWindow::OnSize(UINT param, const CSize& size) {
  Window::OnSize(param, size);

  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been layed out.
  ResetWindowRegion();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameWindow, private:

// static
void CustomFrameWindow::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resize_cursors_[RC_NORMAL] = LoadCursor(NULL, IDC_ARROW);
    resize_cursors_[RC_VERTICAL] = LoadCursor(NULL, IDC_SIZENS);
    resize_cursors_[RC_HORIZONTAL] = LoadCursor(NULL, IDC_SIZEWE);
    resize_cursors_[RC_NESW] = LoadCursor(NULL, IDC_SIZENESW);
    resize_cursors_[RC_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
    initialized = true;
  }
}

void CustomFrameWindow::ResetWindowRegion() {
  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  HRGN current_rgn = CreateRectRgn(0, 0, 0, 0);
  int current_rgn_result = GetWindowRgn(GetHWND(), current_rgn);

  HRGN new_region = NULL;
  if (!IsMaximized()) {
    CRect window_rect;
    GetWindowRect(&window_rect);
    gfx::Path window_mask;
    non_client_view_->GetWindowMask(gfx::Size(window_rect.Width(),
                                              window_rect.Height()),
                                    &window_mask);
    new_region = window_mask.CreateHRGN();
  }

  if (current_rgn_result == ERROR ||
      !EqualRgn(current_rgn, new_region)) {
    // SetWindowRgn takes ownership of the HRGN created by CreateHRGN.
    SetWindowRgn(new_region, TRUE);
  } else if (new_region) {
    DeleteObject(new_region);
  }

  DeleteObject(current_rgn);
}

}  // namespace ChromeViews

