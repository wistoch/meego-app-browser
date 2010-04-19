// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bubble_border.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "gfx/canvas.h"
#include "gfx/path.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

// static
SkBitmap* BubbleBorder::left_ = NULL;
SkBitmap* BubbleBorder::top_left_ = NULL;
SkBitmap* BubbleBorder::top_ = NULL;
SkBitmap* BubbleBorder::top_right_ = NULL;
SkBitmap* BubbleBorder::right_ = NULL;
SkBitmap* BubbleBorder::bottom_right_ = NULL;
SkBitmap* BubbleBorder::bottom_ = NULL;
SkBitmap* BubbleBorder::bottom_left_ = NULL;
SkBitmap* BubbleBorder::top_arrow_ = NULL;
SkBitmap* BubbleBorder::bottom_arrow_ = NULL;

// static
int BubbleBorder::arrow_x_offset_;

// The height inside the arrow image, in pixels.
static const int kArrowInteriorHeight = 7;

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& position_relative_to,
                                  const gfx::Size& contents_size) const {
  // Desired size is size of contents enlarged by the size of the border images.
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.left() + insets.right(),
                      insets.top() + insets.bottom());

  // Screen position depends on the arrow location.
  // The arrow should overlap the target by some amount since the bottom arrow
  // has lots of shadow below it and the top arrow is given an equivalent amount
  // of padding.
  const int kArrowOverlap = 3;
  int x = position_relative_to.x() + (position_relative_to.width() / 2);
  int arrow_offset = override_arrow_x_offset_ ? override_arrow_x_offset_ :
                                                arrow_x_offset_;
  if (arrow_is_left())
    x -= arrow_offset;
  else if (arrow_location_ == NONE)
    x -= ((contents_size.width() / 2) + insets.left());
  else
    x += (arrow_offset - border_size.width() + 1);
  int y = position_relative_to.y();
  if (arrow_is_bottom())
    y += (kArrowOverlap - border_size.height());
  else if (arrow_location_ == NONE)
    y += position_relative_to.height();
  else
    y += (position_relative_to.height() - kArrowOverlap);

  return gfx::Rect(x, y, border_size.width(), border_size.height());
}

void BubbleBorder::GetInsets(gfx::Insets* insets) const {
  int top, bottom;
  if (arrow_is_bottom()) {
    top = top_->height();
    bottom = std::max(bottom_->height(), bottom_arrow_->height());
  } else {
    top = (arrow_location_ == NONE) ?
        0 : std::max(top_->height(), top_arrow_->height());
    bottom = bottom_->height();
  }
  insets->Set(top, left_->width(), bottom, right_->width());
}

// static
void BubbleBorder::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    // Load images.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    left_ = rb.GetBitmapNamed(IDR_BUBBLE_L);
    top_left_ = rb.GetBitmapNamed(IDR_BUBBLE_TL);
    top_ = rb.GetBitmapNamed(IDR_BUBBLE_T);
    top_right_ = rb.GetBitmapNamed(IDR_BUBBLE_TR);
    right_ = rb.GetBitmapNamed(IDR_BUBBLE_R);
    bottom_right_ = rb.GetBitmapNamed(IDR_BUBBLE_BR);
    bottom_ = rb.GetBitmapNamed(IDR_BUBBLE_B);
    bottom_left_ = rb.GetBitmapNamed(IDR_BUBBLE_BL);
    top_arrow_ = rb.GetBitmapNamed(IDR_BUBBLE_T_ARROW);
    bottom_arrow_ = rb.GetBitmapNamed(IDR_BUBBLE_B_ARROW);

    // Calculate horizontal inset for arrow by ensuring that the widest arrow
    // and corner images will have enough room to avoid overlap.
    arrow_x_offset_ =
        (std::max(top_arrow_->width(), bottom_arrow_->width()) / 2) +
        std::max(std::max(top_left_->width(), top_right_->width()),
                 std::max(bottom_left_->width(), bottom_right_->width()));

    initialized = true;
  }
}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) const {
  // Convenience shorthand variables.
  int width = view.width();
  int tl_width = top_left_->width();
  int tl_height = top_left_->height();
  int t_height = top_->height();
  int tr_width = top_right_->width();
  int tr_height = top_right_->height();
  int r_width = right_->width();
  int br_width = bottom_right_->width();
  int br_height = bottom_right_->height();
  int b_height = bottom_->height();
  int bl_width = bottom_left_->width();
  int bl_height = bottom_left_->height();

  /* The variables below can be confusing; here's what they mean:
   *
   * 0∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙┌───┐      ┌───┐
   * border_top∙∙∙∙∙∙∙┌────┬─┤ ▲ ├──────┤ ▲ ├─┬────┐
   *                  │  / │-│∙ ∙│------│∙ ∙│-│ \  │
   * top∙∙∙∙∙∙∙∙∙∙∙∙∙∙│ /  ├─┴───┴──────┴───┴─┤  \ │
   *   tl_bottom∙∙∙∙∙∙├───┬┘                  └┬───┤∙∙∙∙∙∙tr_bottom
   *                  │ | │                    │ | │
   *                  │ | │                    │ | │
   *                  │ | │                    │ | │
   *   bl_y∙∙∙∙∙∙∙∙∙∙∙├───┴┐                  ┌┴───┤∙∙∙∙∙∙br_y
   * bottom∙∙∙∙∙∙∙∙∙∙∙│ \  ├─┬───┬──────┬───┬─┤  / │
   *                  │  \ │-│. .│------│. .│-│ /  │
   * border_bottom∙∙∙∙└────┴─┤ ▼ ├──────┤ ▼ ├─┴────┘
   * view.height()∙∙∙∙∙∙∙∙∙∙∙└───┘      └───┘
   *
   * If |arrow_location_| == NONE, the entire top edge is ommitted, and
   * |tl_bottom| == |tr_bottom| == 0.  Otherwise, one of the four arrows will be
   * drawn.
   */

  gfx::Insets insets;
  GetInsets(&insets);
  int top = insets.top();
  int bottom = view.height() - insets.bottom();
  int border_bottom = bottom + b_height;
  int bl_y = border_bottom - bl_height;
  int br_y = border_bottom - br_height;

  int border_top, tl_bottom, tr_bottom;
  if (arrow_location_ == NONE) {
    border_top = tl_bottom = tr_bottom = 0;
  } else {
    border_top = top - t_height;
    tl_bottom = border_top + tl_height;
    tr_bottom = border_top + tr_height;

    // Top left corner
    canvas->DrawBitmapInt(*top_left_, 0, border_top);

    // Top right corner
    canvas->DrawBitmapInt(*top_right_, width - tr_width, border_top);
  }

  // Right edge
  canvas->TileImageInt(*right_, width - r_width, tr_bottom, r_width,
                       br_y - tr_bottom);

  // Bottom right corner
  canvas->DrawBitmapInt(*bottom_right_, width - br_width, br_y);


  // Bottom left corner
  canvas->DrawBitmapInt(*bottom_left_, 0, bl_y);

  // Left edge
  canvas->TileImageInt(*left_, 0, tl_bottom, left_->width(), bl_y - tl_bottom);

  // Arrow edge, if necessary
  bool should_draw_top_edge = false;
  bool should_draw_bottom_edge = true;
  if (arrow_location_ != NONE) {
    /* Here's what the variables below mean (without loss of generality):
     *
     *                    arrow_center
     *                 arrow_x │ arrow_r
     *                       │ │ │
     * left_of_edge─┬────┐   │ │ │       ┌────┬─right_of_edge
     * arrow_y∙∙∙∙∙∙∙∙∙∙∙∙∙∙∙┌───┐
     * edge_y∙∙∙∙∙∙∙┌────┬───┤ ▲ ├───────┬────┐  ┐
     *              │  / │---│∙ ∙│-------│ \  │  ├─e_height
     *              │ /  ├───┴───┴───────┤  \ │  ┘
     *              ├───┬┘               └┬───┤
     *              |   |    └─┬─┘        |   |
     *              ∙   ∙└─┬─┘ │ └───┬───┘∙   ∙
     *       left_of_arrow─┘   │     └─right_of_arrow
     *                    arrow_width
     *
     * Not shown: border_y and tip_y contain the base and tip coordinates inside
     * the arrow for use filling the arrow interior with the background color.
     */

    SkBitmap* edge;
    SkBitmap* arrow;
    int left_of_edge, right_of_edge, edge_y, arrow_y;
    SkScalar border_y, tip_y;
    if (arrow_is_bottom()) {
      should_draw_top_edge = true;
      should_draw_bottom_edge = false;
      edge = bottom_;
      arrow = bottom_arrow_;
      left_of_edge = bl_width;
      right_of_edge = br_width;
      edge_y = arrow_y = bottom;
      border_y = SkIntToScalar(bottom);
      tip_y = SkIntToScalar(bottom + kArrowInteriorHeight);
    } else {
      edge = top_;
      arrow = top_arrow_;
      left_of_edge = tl_width;
      right_of_edge = tr_width;
      edge_y = border_top;
      arrow_y = top - top_arrow_->height();
      border_y = SkIntToScalar(top);
      tip_y = SkIntToScalar(top - kArrowInteriorHeight);
    }
    int arrow_offset = override_arrow_x_offset_ ? override_arrow_x_offset_ :
                                                  arrow_x_offset_;
    int arrow_width = arrow->width();
    int arrow_center = arrow_is_left() ?
        arrow_offset : width - arrow_offset - 1;
    int arrow_x = arrow_center - (arrow_width / 2);
    SkScalar arrow_interior_x =
        SkIntToScalar(arrow_center - kArrowInteriorHeight);
    SkScalar arrow_interior_r =
        SkIntToScalar(arrow_center + kArrowInteriorHeight);
    int arrow_r = arrow_x + arrow_width;
    int e_height = edge->height();

    // Edge to the left of the arrow
    int left_of_arrow = arrow_x - left_of_edge;
    if (left_of_arrow) {
      canvas->TileImageInt(*edge, left_of_edge, edge_y, left_of_arrow,
                           e_height);
    }

    // Interior of the arrow (filled with background color)
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(background_color_);
    gfx::Path path;
    path.incReserve(4);
    path.moveTo(arrow_interior_x, border_y);
    path.lineTo(SkIntToScalar(arrow_center), tip_y);
    path.lineTo(arrow_interior_r, border_y);
    path.close();
    canvas->drawPath(path, paint);

    // Arrow border
    canvas->DrawBitmapInt(*arrow, arrow_x, arrow_y);

    // Edge to the right of the arrow
    int right_of_arrow = width - arrow_r - right_of_edge;
    if (right_of_arrow)
      canvas->TileImageInt(*edge, arrow_r, edge_y, right_of_arrow, e_height);
  }

  // Top edge, if not already drawn
  if (should_draw_top_edge) {
    canvas->TileImageInt(*top_, tl_width, border_top,
                         width - tl_width - tr_width, t_height);
  }

  // Bottom edge, if not already drawn
  if (should_draw_bottom_edge) {
    canvas->TileImageInt(*bottom_, bl_width, bottom,
                         width - bl_width - br_width, b_height);
  }
}

/////////////////////////

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  // NOTE: This doesn't handle an arrow location of "NONE", which has square top
  // corners.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(border_->background_color());
  gfx::Path path;
  gfx::Rect bounds(view->GetLocalBounds(false));
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path.addRoundRect(rect, radius, radius);
  canvas->drawPath(path, paint);
}
