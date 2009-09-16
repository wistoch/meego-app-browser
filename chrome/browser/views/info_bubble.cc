// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bubble.h"

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/gfx/path.h"
#include "app/resource_bundle.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif

using views::View;

namespace {

// All sizes are in pixels.

// Size of the border, along each edge.
const int kBorderSize = 1;

// Size of the arrow.
const int kArrowSize = 5;

// Number of pixels to the start of the arrow from the edge of the window.
const int kArrowXOffset = 13;

// Number of pixels between the tip of the arrow and the region we're
// pointing to.
const int kArrowToContentPadding = -4;

// Background color of the bubble.
#if defined(OS_WIN)
const SkColor kBackgroundColor = color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor kBackgroundColor = SK_ColorWHITE;
#endif

// Color of the border and arrow.
const SkColor kBorderColor1 = SkColorSetRGB(99, 99, 99);
// Border shadow color.
const SkColor kBorderColor2 = SkColorSetRGB(160, 160, 160);

// Intended dimensions of the bubble's corner images. If you update these,
// make sure that the OnSize code works.
const int kInfoBubbleCornerWidth = 3;
const int kInfoBubbleCornerHeight = 3;

// Bubble corner images.
const SkBitmap* kInfoBubbleCornerTopLeft = NULL;
const SkBitmap* kInfoBubbleCornerTopRight = NULL;
const SkBitmap* kInfoBubbleCornerBottomLeft = NULL;
const SkBitmap* kInfoBubbleCornerBottomRight = NULL;

// Margins around the content.
const int kInfoBubbleViewTopMargin = 6;
const int kInfoBubbleViewBottomMargin = 9;
const int kInfoBubbleViewLeftMargin = 6;
const int kInfoBubbleViewRightMargin = 6;

}  // namespace

// InfoBubble -----------------------------------------------------------------

// static
InfoBubble* InfoBubble::Show(views::Window* parent,
                             const gfx::Rect& position_relative_to,
                             views::View* contents,
                             InfoBubbleDelegate* delegate) {
  InfoBubble* window = new InfoBubble();
  window->Init(parent, position_relative_to, contents, delegate);
  return window;
}

void InfoBubble::Close() {
  Close(false);
}

InfoBubble::InfoBubble()
    :
#if defined(OS_LINUX)
      WidgetGtk(TYPE_POPUP),
#endif
      delegate_(NULL),
      parent_(NULL),
      content_view_(NULL),
      closed_(false) {
}

void InfoBubble::Init(views::Window* parent,
                      const gfx::Rect& position_relative_to,
                      views::View* contents,
                      InfoBubbleDelegate* delegate) {
  parent_ = parent;
  parent_->DisableInactiveRendering();

  delegate_ = delegate;

  if (kInfoBubbleCornerTopLeft == NULL) {
    kInfoBubbleCornerTopLeft = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_TOP_LEFT);
    kInfoBubbleCornerTopRight = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_TOP_RIGHT);
    kInfoBubbleCornerBottomLeft = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_BOTTOM_LEFT);
    kInfoBubbleCornerBottomRight = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_INFO_BUBBLE_CORNER_BOTTOM_RIGHT);
  }
#if defined(OS_WIN)
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW);
  set_initial_class_style(
      (win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
      0 : CS_DROPSHADOW);
#endif
  content_view_ = CreateContentView(contents);

#if defined(OS_WIN)
  WidgetWin::Init(parent->GetNativeWindow(), gfx::Rect());
#else
  WidgetGtk::Init(GTK_WIDGET(parent->GetNativeWindow()), gfx::Rect());
#endif

  SetContentsView(content_view_);
  // The preferred size may differ when parented. Ask for the bounds again
  // and if they differ reset the bounds.
  gfx::Rect parented_bounds =
      content_view_->CalculateWindowBoundsAndAjust(position_relative_to);
  SetBounds(parented_bounds);

#if defined(OS_WIN)
  // Register the Escape accelerator for closing.
  GetFocusManager()->RegisterAccelerator(
      views::Accelerator(VK_ESCAPE, false, false, false), this);
#endif

  NotificationService::current()->Notify(NotificationType::INFO_BUBBLE_CREATED,
                                         Source<InfoBubble>(this),
                                         NotificationService::NoDetails());

  // Show the window.
#if defined(OS_WIN)
  ShowWindow(SW_SHOW);
#else
  static_cast<WidgetGtk*>(window)->Show();
#endif
}

InfoBubble::ContentView* InfoBubble::CreateContentView(View* content) {
  return new ContentView(content, this);
}

#if defined(OS_WIN)
void InfoBubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // The popup should close when it is deactivated.
  if (action == WA_INACTIVE && !closed_) {
    Close();
  } else if (action == WA_ACTIVE) {
    DCHECK(GetRootView()->GetChildViewCount() > 0);
    GetRootView()->GetChildViewAt(0)->RequestFocus();
  }
}

void InfoBubble::OnSize(UINT param, const CSize& size) {
  // See OnSizeAllocate for the Linux version.
  gfx::Path path;
  content_view_->GetMask(gfx::Size(size.cx, size.cy), &path);
  SetWindowRgn(path.CreateHRGN(), TRUE);
  WidgetWin::OnSize(param, size);
}
#elif defined(OS_LINUX)
void InfoBubble::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  gfx::Path path;
  content_view_->GetMask(gfx::Size(allocation->width, allocation->height),
                         &path);
  SetShape(path);
  WidgetGtk::OnSizeAllocate(widget, allocation);
}
#endif

void InfoBubble::Close(bool closed_by_escape) {
  if (closed_)
    return;

  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape);
  closed_ = true;
#if defined(OS_WIN)
  WidgetWin::Close();
#else
  WidgetGtk::Close();
#endif
}

bool InfoBubble::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (!delegate_ || delegate_->CloseOnEscape()) {
    Close(true);
    return true;
  }
  return false;
}

// ContentView ----------------------------------------------------------------

InfoBubble::ContentView::ContentView(views::View* content, InfoBubble* host)
    : content_(content),
      host_(host) {
  if (UILayoutIsRightToLeft()) {
    arrow_edge_ = TOP_RIGHT;
  } else {
    arrow_edge_ = TOP_LEFT;
  }
}

gfx::Rect InfoBubble::ContentView::CalculateWindowBoundsAndAjust(
    const gfx::Rect& position_relative_to) {
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect monitor_bounds(
      monitor_provider->GetMonitorWorkAreaMatching(position_relative_to));
  // Calculate the bounds using TOP_LEFT (the default).
  gfx::Rect window_bounds = CalculateWindowBounds(position_relative_to);
  if (monitor_bounds.IsEmpty() || monitor_bounds.Contains(window_bounds))
    return window_bounds;
  // Didn't fit, adjust the edge to fit as much as we can.
  if (window_bounds.bottom() > monitor_bounds.bottom())
    SetArrowEdge(BOTTOM_LEFT);
  if (window_bounds.right() > monitor_bounds.right()) {
    if (IsTop())
      SetArrowEdge(TOP_RIGHT);
    else
      SetArrowEdge(BOTTOM_RIGHT);
  }
  // And return new bounds.
  return CalculateWindowBounds(position_relative_to);
}

gfx::Size InfoBubble::ContentView::GetPreferredSize() {
  DCHECK(GetChildViewCount() == 1);
  View* content = GetChildViewAt(0);
  gfx::Size pref = content->GetPreferredSize();
  pref.Enlarge(kBorderSize + kBorderSize + kInfoBubbleViewLeftMargin +
                   kInfoBubbleViewRightMargin,
               kBorderSize + kBorderSize + kArrowSize +
                   kInfoBubbleViewTopMargin + kInfoBubbleViewBottomMargin);
  return pref;
}

void InfoBubble::ContentView::Layout() {
  DCHECK(GetChildViewCount() == 1);
  View* content = GetChildViewAt(0);
  int x = kBorderSize;
  int y = kBorderSize;
  int content_width = width() - kBorderSize - kBorderSize -
              kInfoBubbleViewLeftMargin - kInfoBubbleViewRightMargin;
  int content_height = height() - kBorderSize - kBorderSize - kArrowSize -
               kInfoBubbleViewTopMargin - kInfoBubbleViewBottomMargin;
  if (IsTop())
    y += kArrowSize;
  x += kInfoBubbleViewLeftMargin;
  y += kInfoBubbleViewTopMargin;
  content->SetBounds(x, y, content_width, content_height);
}

void InfoBubble::ContentView::GetMask(const gfx::Size& size, gfx::Path* mask) {
  // Redefine the window visible region so that our dropshadows look right.
  SkScalar width = SkIntToScalar(size.width());
  SkScalar height = SkIntToScalar(size.height());
  SkScalar arrow_size = SkIntToScalar(kArrowSize);
  SkScalar arrow_x = SkIntToScalar(
      (IsLeft() ? kArrowXOffset : width - kArrowXOffset) - 1);
  SkScalar corner_size = SkIntToScalar(kInfoBubbleCornerHeight);

  if (IsTop()) {
    // Top left corner.
    mask->moveTo(0, arrow_size + corner_size - 1);
    mask->lineTo(corner_size - 1, arrow_size);

    // Draw the arrow and the notch of the arrow.
    mask->lineTo(arrow_x - arrow_size, arrow_size);
    mask->lineTo(arrow_x, 0);
    mask->lineTo(arrow_x + 3, 0);
    mask->lineTo(arrow_x + arrow_size + 3, arrow_size);

    // Top right corner.
    mask->lineTo(width - corner_size + 1, arrow_size);
    mask->lineTo(width, arrow_size + corner_size - 1);

    // Bottom right corner.
    mask->lineTo(width, height - corner_size);
    mask->lineTo(width - corner_size, height);

    // Bottom left corner.
    mask->lineTo(corner_size, height);
    mask->lineTo(0, height - corner_size);
  } else {
    // Top left corner.
    mask->moveTo(0, corner_size - 1);
    mask->lineTo(corner_size - 1, 0);

    // Top right corner.
    mask->lineTo(width - corner_size + 1, 0);
    mask->lineTo(width, corner_size - 1);

    // Bottom right corner.
    mask->lineTo(width, height - corner_size - arrow_size);
    mask->lineTo(width - corner_size, height - arrow_size);

    // Draw the arrow and the notch of the arrow.
    mask->lineTo(arrow_x + arrow_size + 2, height - arrow_size);
    mask->lineTo(arrow_x + 2, height);
    mask->lineTo(arrow_x + 1, height);
    mask->lineTo(arrow_x - arrow_size + 1, height - arrow_size);

    // Bottom left corner.
    mask->lineTo(corner_size, height - arrow_size);
    mask->lineTo(0, height - corner_size - arrow_size);
  }

  mask->close();
}

void InfoBubble::ContentView::Paint(gfx::Canvas* canvas) {
  int bubble_x = 0;
  int bubble_y = 0;
  int bubble_w = width();
  int bubble_h = height() - kArrowSize;

  int border_w = bubble_w - 2 * kInfoBubbleCornerWidth;
  int border_h = bubble_h - 2 * kInfoBubbleCornerHeight;

  if (IsTop())
    bubble_y += kArrowSize;

  // Fill in the background.
  // Left side.
  canvas->FillRectInt(kBackgroundColor,
                      bubble_x, bubble_y + kInfoBubbleCornerHeight,
                      kInfoBubbleCornerWidth, border_h);
  // Center Column.
  canvas->FillRectInt(kBackgroundColor,
                      kInfoBubbleCornerWidth, bubble_y,
                      border_w, bubble_h);
  // Right Column.
  canvas->FillRectInt(kBackgroundColor,
                      bubble_w - kInfoBubbleCornerWidth,
                      bubble_y + kInfoBubbleCornerHeight,
                      kInfoBubbleCornerWidth, border_h);

  // Draw the border.
  // Top border.
  canvas->DrawLineInt(kBorderColor1,
                      kInfoBubbleCornerWidth, bubble_y,
                      kInfoBubbleCornerWidth + border_w, bubble_y);
  // Bottom border.
  canvas->DrawLineInt(kBorderColor1,
                      kInfoBubbleCornerWidth, bubble_y + bubble_h - 1,
                      kInfoBubbleCornerWidth + border_w,
                      bubble_y + bubble_h - 1);
  // Left border.
  canvas->DrawLineInt(kBorderColor1,
                      bubble_x, bubble_y + kInfoBubbleCornerHeight,
                      bubble_x, bubble_y + kInfoBubbleCornerHeight + border_h);

  // Right border.
  canvas->DrawLineInt(kBorderColor1,
                      width() - 1, bubble_y + kInfoBubbleCornerHeight,
                      width() - 1,
                      bubble_y + kInfoBubbleCornerHeight + border_h);

  // Draw the corners.
  canvas->DrawBitmapInt(*kInfoBubbleCornerTopLeft, 0, bubble_y);
  canvas->DrawBitmapInt(*kInfoBubbleCornerTopRight,
                        bubble_w - kInfoBubbleCornerWidth, bubble_y);
  canvas->DrawBitmapInt(*kInfoBubbleCornerBottomLeft, 0,
                        bubble_y + bubble_h - kInfoBubbleCornerHeight);
  canvas->DrawBitmapInt(*kInfoBubbleCornerBottomRight,
                        bubble_w - kInfoBubbleCornerWidth,
                        bubble_y + bubble_h - kInfoBubbleCornerHeight);

  // Draw the arrow and the notch of the arrow.
  int arrow_x = IsLeft() ? kArrowXOffset : width() - kArrowXOffset;
  int arrow_y = IsTop() ? bubble_y : bubble_y + bubble_h - 1;
  const int arrow_delta = IsTop() ? -1 : 1;
  for (int i = 0, y = arrow_y; i <= kArrowSize; ++i, y += arrow_delta) {
    if (kArrowSize != i) {
      // Draw the notch formed by the arrow.
      canvas->FillRectInt(kBackgroundColor, arrow_x - (kArrowSize - i) + 1,
                          y, (kArrowSize - i) * 2 - 1, 1);
    }
    // Draw the sides of the arrow.
    canvas->FillRectInt(kBorderColor1, arrow_x - (kArrowSize - i), y, 1, 1);
    canvas->FillRectInt(kBorderColor1, arrow_x + (kArrowSize - i), y, 1, 1);
    if (i != 0) {
      canvas->FillRectInt(kBorderColor2, arrow_x - (kArrowSize - i) - 1, y, 1,
                          1);
      canvas->FillRectInt(kBorderColor2, arrow_x + (kArrowSize - i) + 1, y, 1,
                          1);
    }
  }
}

void InfoBubble::ContentView::ViewHierarchyChanged(bool is_add,
                                                   View* parent,
                                                   View* child) {
  if (is_add && child == this)
    AddChildView(content_);
}

gfx::Rect InfoBubble::ContentView::CalculateWindowBounds(
    const gfx::Rect& position_relative_to) {
  gfx::Size pref = GetPreferredSize();
  int x = position_relative_to.x() + position_relative_to.width() / 2;
  int y;
  if (IsLeft())
    x -= kArrowXOffset;
  else
    x = x + kArrowXOffset - pref.width();
  if (IsTop()) {
    y = position_relative_to.bottom() + kArrowToContentPadding;
  } else {
    y = position_relative_to.y() - kArrowToContentPadding - pref.height();
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}
