// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/color_utils.h"

#include <math.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "skia/ext/skia_utils_win.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"

namespace color_utils {

// Helper functions -----------------------------------------------------------

namespace {

double calcHue(double temp1, double temp2, double hue) {
  if (hue < 0.0)
    ++hue;
  else if (hue > 1.0)
    --hue;

  if (hue * 6.0 < 1.0)
    return temp1 + (temp2 - temp1) * hue * 6.0;
  if (hue * 2.0 < 1.0)
    return temp2;
  if (hue * 3.0 < 2.0)
    return temp1 + (temp2 - temp1) * (2.0 / 3.0 - hue) * 6.0;

  return temp1;
}

int GetLumaForColor(SkColor* color) {
  int luma = static_cast<int>((0.3 * SkColorGetR(*color)) +
                              (0.59 * SkColorGetG(*color)) +
                              (0.11 * SkColorGetB(*color)));
  return std::max(std::min(luma, 255), 0);
}

// Next two functions' formulas from:
// http://www.w3.org/TR/WCAG20/#relativeluminancedef
// http://www.w3.org/TR/WCAG20/#contrast-ratiodef

double ConvertSRGB(double eight_bit_component) {
  const double component = eight_bit_component / 255.0;
  return (component <= 0.03928) ?
      (component / 12.92) : pow((component + 0.055) / 1.055, 2.4);
}

SkColor LumaInvertColor(const SkColor& color) {
  HSL hsl;
  SkColorToHSL(color, &hsl);
  hsl.l = 1.0 - hsl.l;
  return HSLToSkColor(hsl, 255);
}

double ContrastRatio(double foreground_luminance, double background_luminance) {
  // NOTE: Only pass in numbers obtained from RelativeLuminance(), since those
  // are guaranteed to be > 0 and thus not cause a divide-by-zero error here.
  return (foreground_luminance > background_luminance) ?
      (foreground_luminance / background_luminance) :
      (background_luminance / foreground_luminance);
}

}  // namespace

// ----------------------------------------------------------------------------

double RelativeLuminance(SkColor color) {
  return (0.2126 * ConvertSRGB(SkColorGetR(color))) +
      (0.7152 * ConvertSRGB(SkColorGetG(color))) +
      (0.0722 * ConvertSRGB(SkColorGetB(color))) + 0.05;
}

void SkColorToHSL(SkColor c, HSL* hsl) {
  double r = static_cast<double>(SkColorGetR(c)) / 255.0;
  double g = static_cast<double>(SkColorGetG(c)) / 255.0;
  double b = static_cast<double>(SkColorGetB(c)) / 255.0;
  double vmax = std::max(std::max(r, g), b);
  double vmin = std::min(std::min(r, g), b);
  double delta = vmax - vmin;
  hsl->l = (vmax + vmin) / 2;
  if (delta) {
    double dr = (((vmax - r) / 6.0) + (delta / 2.0)) / delta;
    double dg = (((vmax - g) / 6.0) + (delta / 2.0)) / delta;
    double db = (((vmax - b) / 6.0) + (delta / 2.0)) / delta;
    // We need to compare for the max value because comparing vmax to r,
    // g or b can sometimes result in values overflowing registers.
    if (r >= g && r >= b)
      hsl->h = db - dg;
    else if (g >= r && g >= b)
      hsl->h = (1.0 / 3.0) + dr - db;
    else  // (b >= r && b >= g)
      hsl->h = (2.0 / 3.0) + dg - dr;

    if (hsl->h < 0.0)
      ++hsl->h;
    else if (hsl->h > 1.0)
      --hsl->h;

    hsl->s = delta / ((hsl->l < 0.5) ? (vmax + vmin) : (2 - vmax - vmin));
  } else {
    hsl->h = hsl->s = 0;
  }
}

SkColor HSLToSkColor(const HSL& hsl, SkAlpha alpha) {
  double hue = hsl.h;
  double saturation = hsl.s;
  double lightness = hsl.l;

  // If there's no color, we don't care about hue and can do everything based
  // on brightness.
  if (!saturation) {
    uint8 light;

    if (lightness < 0)
      light = 0;
    else if (lightness >= 1.0)
      light = 255;
    else
      light = SkDoubleToFixed(lightness) >> 8;

    return SkColorSetARGB(alpha, light, light, light);
  }

  double temp2 = (lightness < 0.5) ?
      (lightness * (1.0 + saturation)) :
      (lightness + saturation - (lightness * saturation));
  double temp1 = 2.0 * lightness - temp2;
  return SkColorSetARGB(alpha,
      static_cast<int>(calcHue(temp1, temp2, hue + 1.0 / 3.0) * 255),
      static_cast<int>(calcHue(temp1, temp2, hue) * 255),
      static_cast<int>(calcHue(temp1, temp2, hue - 1.0 / 3.0) * 255));
}

SkColor HSLShift(SkColor color, const HSL& shift) {
  HSL hsl;
  int alpha = SkColorGetA(color);
  SkColorToHSL(color, &hsl);

  // Replace the hue with the tint's hue.
  if (shift.h >= 0)
    hsl.h = shift.h;

  // Change the saturation.
  if (shift.s >= 0) {
    if (shift.s <= 0.5)
      hsl.s *= shift.s * 2.0;
    else
      hsl.s += (1.0 - hsl.s) * ((shift.s - 0.5) * 2.0);
  }

  SkColor result = HSLToSkColor(hsl, alpha);

  if (shift.l < 0)
    return result;

  // Lightness shifts in the style of popular image editors aren't
  // actually represented in HSL - the L value does have some effect
  // on saturation.
  double r = static_cast<double>(SkColorGetR(result));
  double g = static_cast<double>(SkColorGetG(result));
  double b = static_cast<double>(SkColorGetB(result));
  if (shift.l <= 0.5) {
    r *= (shift.l * 2.0);
    g *= (shift.l * 2.0);
    b *= (shift.l * 2.0);
  } else {
    r += (255.0 - r) * ((shift.l - 0.5) * 2.0);
    g += (255.0 - g) * ((shift.l - 0.5) * 2.0);
    b += (255.0 - b) * ((shift.l - 0.5) * 2.0);
  }
  return SkColorSetARGB(alpha,
                        static_cast<int>(r),
                        static_cast<int>(g),
                        static_cast<int>(b));
}

bool IsColorCloseToTransparent(SkAlpha alpha) {
  const int kCloseToBoundary = 64;
  return alpha < kCloseToBoundary;
}

bool IsColorCloseToGrey(int r, int g, int b) {
  const int kAverageBoundary = 15;
  int average = (r + g + b) / 3;
  return (abs(r - average) < kAverageBoundary) &&
         (abs(g - average) < kAverageBoundary) &&
         (abs(b - average) < kAverageBoundary);
}

SkColor GetAverageColorOfFavicon(SkBitmap* favicon, SkAlpha alpha) {
  int r = 0, g = 0, b = 0;

  SkAutoLockPixels favicon_lock(*favicon);
  SkColor* pixels = static_cast<SkColor*>(favicon->getPixels());
  // Assume ARGB_8888 format.
  DCHECK(favicon->getConfig() == SkBitmap::kARGB_8888_Config);
  SkColor* current_color = pixels;

  DCHECK(favicon->width() <= 16 && favicon->height() <= 16);

  int pixel_count = favicon->width() * favicon->height();
  int color_count = 0;
  for (int i = 0; i < pixel_count; ++i, ++current_color) {
    // Disregard this color if it is close to black, close to white, or close
    // to transparent since any of those pixels do not contribute much to the
    // color makeup of this icon.
    int cr = SkColorGetR(*current_color);
    int cg = SkColorGetG(*current_color);
    int cb = SkColorGetB(*current_color);

    if (IsColorCloseToTransparent(SkColorGetA(*current_color)) ||
        IsColorCloseToGrey(cr, cg, cb))
      continue;

    r += cr;
    g += cg;
    b += cb;
    ++color_count;
  }

  return color_count ?
      SkColorSetARGB(alpha, r / color_count, g / color_count, b / color_count) :
      SkColorSetARGB(alpha, 0, 0, 0);
}

void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]) {
  SkAutoLockPixels bitmap_lock(*bitmap);
  // Assume ARGB_8888 format.
  DCHECK(bitmap->getConfig() == SkBitmap::kARGB_8888_Config);

  int pixel_width = bitmap->width();
  int pixel_height = bitmap->height();
  for (int y = 0; y < pixel_height; ++y) {
    SkColor* current_color = static_cast<uint32_t*>(bitmap->getAddr32(0, y));
    for (int x = 0; x < pixel_width; ++x, ++current_color)
      histogram[GetLumaForColor(current_color)]++;
  }
}

SkColor AlphaBlend(SkColor foreground, SkColor background, SkAlpha alpha) {
  if (alpha == 0)
    return background;
  if (alpha == 255)
    return foreground;

  int f_alpha = SkColorGetA(foreground);
  int b_alpha = SkColorGetA(background);

  double normalizer = (f_alpha * alpha + b_alpha * (255 - alpha)) / 255.0;
  if (normalizer == 0.0)
    return SkColorSetARGB(0, 0, 0, 0);

  double f_weight = f_alpha * alpha / normalizer;
  double b_weight = b_alpha * (255 - alpha) / normalizer;

  double r = (SkColorGetR(foreground) * f_weight +
              SkColorGetR(background) * b_weight) / 255.0;
  double g = (SkColorGetG(foreground) * f_weight +
              SkColorGetG(background) * b_weight) / 255.0;
  double b = (SkColorGetB(foreground) * f_weight +
              SkColorGetB(background) * b_weight) / 255.0;

  return SkColorSetARGB(static_cast<int>(normalizer),
                        static_cast<int>(r),
                        static_cast<int>(g),
                        static_cast<int>(b));
}

SkColor GetReadableColor(SkColor foreground, SkColor background) {
  const SkColor foreground2 = LumaInvertColor(foreground);
  const double background_luminance = RelativeLuminance(background);
  return (ContrastRatio(RelativeLuminance(foreground), background_luminance) >=
          ContrastRatio(RelativeLuminance(foreground2), background_luminance)) ?
      foreground : foreground2;
}

SkColor GetSysSkColor(int which) {
#if defined(OS_WIN)
  return skia::COLORREFToSkColor(GetSysColor(which));
#else
  NOTIMPLEMENTED();
  return SK_ColorLTGRAY;
#endif
}

}  // namespace color_utils