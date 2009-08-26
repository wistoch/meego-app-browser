// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "app/gfx/font.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {

// Font settings that we initialize once and then use when drawing text in
// DrawStringInt().
static cairo_font_options_t* cairo_font_options = NULL;

// Returns a new pango font, free with pango_font_description_free().
PangoFontDescription* PangoFontFromGfxFont(const gfx::Font& gfx_font) {
  gfx::Font font = gfx_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  pango_font_description_set_size(pfd, font.FontSize() * PANGO_SCALE);

  switch (font.style()) {
    case gfx::Font::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case gfx::Font::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case gfx::Font::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case gfx::Font::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

// Update |cairo_font_options| based on GtkSettings, allocating it if needed.
static void UpdateCairoFontOptions() {
  if (!cairo_font_options)
    cairo_font_options = cairo_font_options_create();

  GtkSettings* gtk_settings = gtk_settings_get_default();
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;
  g_object_get(gtk_settings,
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba_style,
               NULL);

  // g_object_get() doesn't tell us whether the properties were present or not,
  // but if they aren't (because gnome-settings-daemon isn't running), we'll get
  // NULL values for the strings.
  if (hint_style && rgba_style) {
    if (!antialias) {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_NONE);
    } else if (strcmp(rgba_style, "none") == 0) {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_GRAY);
    } else {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_SUBPIXEL);
      cairo_subpixel_order_t cairo_subpixel_order =
          CAIRO_SUBPIXEL_ORDER_DEFAULT;
      if (strcmp(rgba_style, "rgb") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
      } else if (strcmp(rgba_style, "bgr") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
      } else if (strcmp(rgba_style, "vrgb") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
      } else if (strcmp(rgba_style, "vbgr") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
      }
      cairo_font_options_set_subpixel_order(cairo_font_options,
                                            cairo_subpixel_order);
    }

    cairo_hint_style_t cairo_hint_style = CAIRO_HINT_STYLE_DEFAULT;
    if (hinting == 0 || strcmp(hint_style, "hintnone") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_NONE;
    } else if (strcmp(hint_style, "hintslight") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_SLIGHT;
    } else if (strcmp(hint_style, "hintmedium") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_MEDIUM;
    } else if (strcmp(hint_style, "hintfull") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_FULL;
    }
    cairo_font_options_set_hint_style(cairo_font_options, cairo_hint_style);
  }

  if (hint_style)
    g_free(hint_style);
  if (rgba_style)
    g_free(rgba_style);
}

}  // namespace

namespace gfx {

Canvas::Canvas(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

Canvas::Canvas() : skia::PlatformCanvas() {
}

Canvas::~Canvas() {
}

static void SetupPangoLayout(PangoLayout* layout,
                             const gfx::Font& font,
                             int flags) {
  if (!cairo_font_options)
    UpdateCairoFontOptions();
  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);

  // Callers of DrawStringInt handle RTL layout themselves, so tell pango to not
  // scope out RTL characters.
  pango_layout_set_auto_dir(layout, FALSE);

  if (flags & Canvas::NO_ELLIPSIS) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
  } else {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  }

  if (flags & Canvas::TEXT_ALIGN_CENTER) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & Canvas::TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & Canvas::MULTI_LINE) {
    pango_layout_set_wrap(layout,
        (flags & Canvas::CHARACTER_BREAK) ?
            PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
  }

  PangoFontDescription* desc = PangoFontFromGfxFont(font);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);
}

// static
void Canvas::SizeStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           int* width, int* height, int flags) {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
  cairo_t* cr = cairo_create(surface);
  PangoLayout* layout = pango_cairo_create_layout(cr);

  SetupPangoLayout(layout, font, flags);

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());

  pango_layout_get_size(layout, width, height);
  *width /= PANGO_SCALE;
  *height /= PANGO_SCALE;

  g_object_unref(layout);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void Canvas::DrawStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           const SkColor& color, int x, int y, int w, int h,
                           int flags) {
  cairo_t* cr = beginPlatformPaint();
  PangoLayout* layout = pango_cairo_create_layout(cr);

  SetupPangoLayout(layout, font, flags);

  pango_layout_set_width(layout, w * PANGO_SCALE);
  pango_layout_set_height(layout, h * PANGO_SCALE);

  cairo_save(cr);
  cairo_set_source_rgb(cr,
                       SkColorGetR(color) / 255.0,
                       SkColorGetG(color) / 255.0,
                       SkColorGetB(color) / 255.0);

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());

  int width, height;
  pango_layout_get_size(layout, &width, &height);

  if (flags & Canvas::TEXT_VALIGN_TOP) {
    // Cairo should draw from the top left corner already.
  } else if (flags & Canvas::TEXT_VALIGN_BOTTOM) {
    y = y + (h - (height / PANGO_SCALE));
  } else {
    // Vertically centered.
    y = y + ((h - (height / PANGO_SCALE)) / 2);
  }

  // cairo_rectangle creates a rectangle with a border of 1px inclusive.  The
  // clipping region includes this border. cairo places the top-left corner of
  // the rectangle at (x - 1, y - 1).  We need the top-left corner of the
  // rectangle to be at (x + 1, y + 2) so that we fully cover the extent of the
  // text.
  const int kClipXOffset = 1;
  const int kClipYOffset = 2;
  cairo_rectangle(cr, x + kClipXOffset, y + kClipYOffset, w, h);
  cairo_clip(cr);

  cairo_move_to(cr, x, y);
  pango_cairo_show_layout(cr, layout);
  cairo_restore(cr);

  g_object_unref(layout);
  // NOTE: beginPlatformPaint returned its surface, we shouldn't destroy it.
}

void Canvas::DrawGdkPixbuf(GdkPixbuf* pixbuf, int x, int y) {
  if (!pixbuf) {
    NOTREACHED();
    return;
  }

  cairo_t* cr = beginPlatformPaint();
  gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
  cairo_paint(cr);
}

}  // namespace gfx
