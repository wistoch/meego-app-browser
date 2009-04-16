// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/resource_bundle.h"
#include "grit/theme_resources.h"

namespace {

const GdkColor kBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kSelectedBackgroundColor = GDK_COLOR_RGB(0xdf, 0xe6, 0xf6);

const GdkColor kNormalTextColor = GDK_COLOR_RGB(0x00, 0x00, 0x00);
const GdkColor kURLTextColor = GDK_COLOR_RGB(0x00, 0x88, 0x00);
const GdkColor kDescriptionTextColor = GDK_COLOR_RGB(0x80, 0x80, 0x80);
const GdkColor kDescriptionSelectedTextColor = GDK_COLOR_RGB(0x78, 0x82, 0xb1);

// TODO(deanm): This is added to extend past just the location box, and to
// be below the the star and go button.  Really this means that this should
// probably plumb all the way back to the location bar view.
const int kExtraSpace = 28;
// We have a 1 pixel border around the entire results popup.
const int kBorderThickness = 1;
// The vertical height of each result.
const int kHeightPerResult = 24;
// Additional distance below the edit control.
const int kTopMargin = 3;
// Width of the icons.
const int kIconWidth = 16;
// We want to vertically center the image in the result space.
const int kIconTopPadding = 4;
// Space between the left edge (including the border) and the text.
const int kIconLeftPadding = 6;
// Space between the image and the text.  Would be 6 to line up with the
// entry, but nudge it a bit more to match with the text in the entry.
const int kIconRightPadding = 10;
// Space between the left edge (including the border) and the text.
const int kIconAreaWidth =
    kIconLeftPadding + kIconWidth + kIconRightPadding;
// Space between the right edge (including the border) and the text.
const int kRightPadding = 3;

// TODO(deanm): We should put this on ChromeFont so it can be shared.
// Returns a new pango font, free with pango_font_description_free().
PangoFontDescription* PangoFontFromChromeFont(const ChromeFont& chrome_font) {
  ChromeFont font = chrome_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  pango_font_description_set_size(pfd, font.FontSize() * PANGO_SCALE);

  switch (font.style()) {
    case ChromeFont::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case ChromeFont::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case ChromeFont::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case ChromeFont::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

// Return a GdkRectangle covering the whole area of |window|.
GdkRectangle GetWindowRect(GdkWindow* window) {
  gint width, height;
  gdk_drawable_get_size(GDK_DRAWABLE(window), &width, &height);
  GdkRectangle rect = {0, 0, width, height};
  return rect;
}

// Return a rectangle for the space for a result.  This excludes the border,
// but includes the padding.  This is the area that is colored for a selection.
GdkRectangle GetRectForLine(size_t line, int width) {
  GdkRectangle rect = {kBorderThickness,
                       (line * kHeightPerResult) + kBorderThickness,
                       width - (kBorderThickness * 2),
                       kHeightPerResult};
  return rect;
}

// Helper for drawing an entire pixbuf without dithering.
void DrawFullPixbuf(GdkDrawable* drawable, GdkGC* gc, GdkPixbuf* pixbuf,
                    gint dest_x, gint dest_y) {
  gdk_draw_pixbuf(drawable, gc, pixbuf,
                  0, 0,                        // Source.
                  dest_x, dest_y,              // Dest.
                  -1, -1,                      // Width/height (auto).
                  GDK_RGB_DITHER_NONE, 0, 0);  // Don't dither.
}

PangoAttribute* ForegroundAttrFromColor(const GdkColor& color) {
  return pango_attr_foreground_new(color.red, color.green, color.blue);
}

}  // namespace

AutocompletePopupViewGtk::AutocompletePopupViewGtk(
    AutocompleteEditViewGtk* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : font_(NULL),
      model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      window_(gtk_window_new(GTK_WINDOW_POPUP)),
      opened_(false) {
  GTK_WIDGET_UNSET_FLAGS(window_, GTK_CAN_FOCUS);
  // Don't allow the window to be resized.  This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_app_paintable(window_, TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(window_, TRUE);
  // Set the background color, so we don't need to paint it manually.
  gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kBackgroundColor);

  // TODO(deanm): We might want to eventually follow what Windows does and
  // plumb a ChromeFont through.  This is because popup windows have a
  // different font size, although we could just derive that font here.
  ChromeFont default_font;
  font_ = PangoFontFromChromeFont(default_font);

  g_signal_connect(window_, "expose-event", 
                   G_CALLBACK(&HandleExposeThunk), this);
}

AutocompletePopupViewGtk::~AutocompletePopupViewGtk() {
  // Explicitly destroy our model here, before we destroy our GTK widgets.
  // This is because the model destructor can call back into us, and we need
  // to make sure everything is still valid when it does.
  model_.reset();
  gtk_widget_destroy(window_);
  pango_font_description_free(font_);
}

void AutocompletePopupViewGtk::InvalidateLine(size_t line) {
  GdkRectangle rect = GetWindowRect(window_->window);
  rect = GetRectForLine(line, rect.width);
  gdk_window_invalidate_rect(window_->window, &rect, FALSE);
}

void AutocompletePopupViewGtk::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    Hide();
    return;
  }

  Show(result.size());
  gtk_widget_queue_draw(window_);
}

void AutocompletePopupViewGtk::OnHoverEnabledOrDisabled(bool disabled) {
  NOTIMPLEMENTED();
}

void AutocompletePopupViewGtk::PaintUpdatesNow() {
  // Paint our queued invalidations now, synchronously.
  gdk_window_process_updates(window_->window, FALSE);
}

AutocompletePopupModel* AutocompletePopupViewGtk::GetModel() {
  return model_.get();
}

void AutocompletePopupViewGtk::Show(size_t num_results) {
  gint x, y, width;
  edit_view_->BottomLeftPosWidth(&x, &y, &width);
  x -= kExtraSpace;
  width += kExtraSpace * 2;

  gtk_window_move(GTK_WINDOW(window_), x, y + kTopMargin);
  gtk_widget_set_size_request(window_, width,
      (num_results * kHeightPerResult) + (kBorderThickness * 2));
  gtk_widget_show(window_);
  opened_ = true;
}

void AutocompletePopupViewGtk::Hide() {
  gtk_widget_hide(window_);
  opened_ = false;
}

gboolean AutocompletePopupViewGtk::HandleExpose(GtkWidget* widget,
                                                GdkEventExpose* event) {
  const AutocompleteResult& result = model_->result();

  // TODO(deanm): These would be better as pixmaps someday.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* o2_globe = rb.LoadPixbuf(IDR_O2_GLOBE);
  static GdkPixbuf* o2_history = rb.LoadPixbuf(IDR_O2_HISTORY);
  static GdkPixbuf* o2_more = rb.LoadPixbuf(IDR_O2_MORE);
  static GdkPixbuf* o2_search = rb.LoadPixbuf(IDR_O2_SEARCH);
  static GdkPixbuf* o2_star = rb.LoadPixbuf(IDR_O2_STAR);

  GdkRectangle window_rect = GetWindowRect(event->window);
  // Handle when our window is super narrow.  A bunch of the calculations
  // below would go negative, and really we're not going to fit anything
  // useful in such a small window anyway.  Just don't paint anything.
  // This means we won't draw the border, but, yeah, whatever.
  // TODO(deanm): Make the code more robust and remove this check.
  if (window_rect.width < (kIconAreaWidth * 3))
    return TRUE;

  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);

  // kBorderColor is unallocated, so use the GdkRGB routine.
  gdk_gc_set_rgb_fg_color(gc, &kBorderColor);

  // This assert is kinda ugly, but it would be more currently unneeded work
  // to support painting a border that isn't 1 pixel thick.  There is no point
  // in writing that code now, and explode if that day ever comes.
  COMPILE_ASSERT(kBorderThickness == 1, border_1px_implied);
  // Draw the 1px border around the entire window.
  gdk_draw_rectangle(drawable, gc, FALSE,
                     0, 0,
                     window_rect.width - 1, window_rect.height - 1);

  // TODO(deanm): Cache the layout?  How expensive is it to create?
  PangoLayout* layout = gtk_widget_create_pango_layout(window_, NULL);

  pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_height(layout, kHeightPerResult * PANGO_SCALE);
  pango_layout_set_font_description(layout, font_);

  // TODO(deanm): Intersect the line and damage rects, and only repaint and
  // layout the lines that are actually damaged.  For now paint everything.
  for (size_t i = 0; i < result.size(); ++i) {
    const AutocompleteMatch& match = result.match_at(i);
    GdkRectangle result_rect = GetRectForLine(i, window_rect.width);

    bool is_selected = (model_->selected_line() == i);
    if (is_selected) {
      gdk_gc_set_rgb_fg_color(gc, &kSelectedBackgroundColor);
      // This entry is selected, fill a rect with the selection color.
      gdk_draw_rectangle(drawable, gc, TRUE,
                         result_rect.x, result_rect.y,
                         result_rect.width, result_rect.height);
    }

    GdkPixbuf* icon = NULL;
    bool is_url = false;
    if (match.starred) {
      icon = o2_star;
      is_url = true;
    } else {
      switch (match.type) {
        case AutocompleteMatch::URL_WHAT_YOU_TYPED:
        case AutocompleteMatch::NAVSUGGEST:
          icon = o2_globe;
          is_url = true;
          break;
        case AutocompleteMatch::HISTORY_URL:
        case AutocompleteMatch::HISTORY_TITLE:
        case AutocompleteMatch::HISTORY_BODY:
        case AutocompleteMatch::HISTORY_KEYWORD:
          icon = o2_history;
          is_url = true;
          break;
        case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
        case AutocompleteMatch::SEARCH_HISTORY:
        case AutocompleteMatch::SEARCH_SUGGEST:
        case AutocompleteMatch::SEARCH_OTHER_ENGINE:
          icon = o2_search;
          break;
        case AutocompleteMatch::OPEN_HISTORY_PAGE:
          icon = o2_more;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
    
    // Draw the icon for this result time.
    DrawFullPixbuf(drawable, gc, icon,
                   kIconLeftPadding, result_rect.y + kIconTopPadding);

    // TODO(deanm): Bold the matched portions of text.
    // TODO(deanm): I couldn't get the weight adjustment to be granular enough
    // to match the mocks.  It was basically super bold or super thin.
    
    // Draw the results text vertically centered in the results space.
    // First draw the contents / url, but don't let it take up the whole width
    // if there is also a description to be shown.
    bool has_description = !match.description.empty();
    int text_area_width = window_rect.width - (kIconAreaWidth + kRightPadding);
    if (has_description)
      text_area_width *= 0.7;
    pango_layout_set_width(layout, text_area_width * PANGO_SCALE);

    PangoAttrList* contents_attrs = pango_attr_list_new();
    PangoAttribute* fg_attr = ForegroundAttrFromColor(
        is_url ? kURLTextColor : kNormalTextColor);
    pango_attr_list_insert(contents_attrs, fg_attr);  // Owernship taken.
    pango_layout_set_attributes(layout, contents_attrs);  // Ref taken.
    pango_attr_list_unref(contents_attrs);

    std::string contents = WideToUTF8(match.contents);
    pango_layout_set_text(layout, contents.data(), contents.size());

    int content_width, content_height;
    pango_layout_get_size(layout, &content_width, &content_height);
    content_width /= PANGO_SCALE;
    content_height /= PANGO_SCALE;

    DCHECK_LT(content_height, kHeightPerResult);  // Font too tall.
    int content_y = std::max(result_rect.y,
        result_rect.y + ((kHeightPerResult - content_height) / 2));

    gdk_draw_layout(drawable, gc,
                    kIconAreaWidth, content_y,
                    layout);

    if (has_description) {
      PangoAttrList* description_attrs = pango_attr_list_new();
      PangoAttribute* fg_attr = ForegroundAttrFromColor(
          is_selected ? kDescriptionSelectedTextColor : kDescriptionTextColor);
      pango_attr_list_insert(description_attrs, fg_attr);  // Owernship taken.
      pango_layout_set_attributes(layout, description_attrs);  // Ref taken.
      pango_attr_list_unref(description_attrs);

      std::string description(" - ");
      description.append(WideToUTF8(match.description));
      pango_layout_set_text(layout, description.data(), description.size());

      gdk_draw_layout(drawable, gc,
                      kIconAreaWidth + content_width, content_y,
                      layout);
    }
  }

  g_object_unref(layout);
  g_object_unref(gc);

  return TRUE;
}
