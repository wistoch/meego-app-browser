// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/font.h"

#include <algorithm>
#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"

namespace gfx {

Font* Font::default_font_ = NULL;

// Find the best match font for |family_name| in the same way as Skia
// to make sure CreateFont() successfully creates a default font.  In
// Skia, it only checks the best match font.  If it failed to find
// one, SkTypeface will be NULL for that font family.  It eventually
// causes a segfault.  For example, family_name = "Sans" and system
// may have various fonts.  The first font family in FcPattern will be
// "DejaVu Sans" but a font family returned by FcFontMatch will be "VL
// PGothic".  In this case, SkTypeface for "Sans" returns NULL even if
// the system has a font for "Sans" font family.  See FontMatch() in
// skia/ports/SkFontHost_fontconfig.cpp for more detail.
static std::wstring FindBestMatchFontFamilyName(const char* family_name) {
  FcPattern* pattern = FcPatternCreate();
  FcValue fcvalue;
  fcvalue.type = FcTypeString;
  char* family_name_copy = strdup(family_name);
  fcvalue.u.s = reinterpret_cast<FcChar8*>(family_name_copy);
  FcPatternAdd(pattern, FC_FAMILY, fcvalue, 0);
  FcConfigSubstitute(0, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  DCHECK(match) << "Could not find font: " << family_name;
  FcChar8* match_family;
  FcPatternGetString(match, FC_FAMILY, 0, &match_family);

  std::wstring font_family = UTF8ToWide(
      reinterpret_cast<char*>(match_family));
  FcPatternDestroy(match);
  FcPatternDestroy(pattern);
  free(family_name_copy);
  return font_family;
}

// Pango scales font sizes. This returns the scale factor. See
// pango_cairo_context_set_resolution for details.
// NOTE: this isn't entirely accurate, in that Pango also consults the
// FC_PIXEL_SIZE first (see get_font_size in pangocairo-fcfont), but this
// seems to give us the same sizes as used by Pango for all our fonts in both
// English and Thai.
float Font::GetPangoScaleFactor() {
  static float scale_factor = 0;
  static bool determined_scale = false;
  if (!determined_scale) {
    PangoContext* context = gdk_pango_context_get();
    scale_factor = pango_cairo_context_get_resolution(context);
    // Until we switch to vector graphics, force the max DPI to 96.0.
    scale_factor = std::min(scale_factor, 96.f);
    g_object_unref(context);
    if (scale_factor <= 0)
      scale_factor = 1;
    else
      scale_factor /= 72.0;
    determined_scale = true;
  }
  return scale_factor;
}

// static
Font Font::CreateFont(PangoFontDescription* desc) {
  gint size = pango_font_description_get_size(desc);
  const char* family_name = pango_font_description_get_family(desc);

  // Find best match font for |family_name| to make sure we can get
  // a SkTypeface for the default font.
  // TODO(agl): remove this.
  std::wstring font_family = FindBestMatchFontFamilyName(family_name);

  Font font = CreateFont(font_family, size / PANGO_SCALE);
  int style = 0;
  if (pango_font_description_get_weight(desc) == PANGO_WEIGHT_BOLD) {
    // TODO(davemoore) What should we do about other weights? We currently
    // only support BOLD.
    style |= BOLD;
  }
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC) {
    // TODO(davemoore) What about PANGO_STYLE_OBLIQUE?
    style |= ITALIC;
  }
  if (style != 0) {
    font = font.DeriveFont(0, style);
  }
  return Font(font);
}

// Get the default gtk system font (name and size).
Font::Font() {
  if (default_font_ == NULL) {
    GtkSettings* settings = gtk_settings_get_default();

    gchar* font_name = NULL;
    g_object_get(settings, "gtk-font-name", &font_name, NULL);

    // Temporary CHECK for helping track down
    // http://code.google.com/p/chromium/issues/detail?id=12530
    CHECK(font_name) << " Unable to get gtk-font-name for default font.";

    PangoFontDescription* desc =
        pango_font_description_from_string(font_name);
    default_font_ = new Font(CreateFont(desc));
    pango_font_description_free(desc);
    g_free(font_name);

    DCHECK(default_font_);
  }

  CopyFont(*default_font_);
}

// static
PangoFontDescription* Font::PangoFontFromGfxFont(
    const gfx::Font& gfx_font) {
  gfx::Font font = gfx_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  // Set the absolute size to avoid overflowing UI elements.
  pango_font_description_set_absolute_size(pfd,
      font.FontSize() * PANGO_SCALE * Font::GetPangoScaleFactor());

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

}  // namespace gfx
