// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/color_utils.h"

#include <math.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

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

// These transformations are based on the equations in:
// http://en.wikipedia.org/wiki/Lab_color
// http://en.wikipedia.org/wiki/SRGB_color_space#Specification_of_the_transformation
// See also:
// http://www.brucelindbloom.com/index.html?ColorCalculator.html

const double kCIEConversionAlpha = 0.055;
const double kCIEConversionGamma = 2.2;
const double kE = 0.008856;
const double kK = 903.3;

double CIEConvertNonLinear(uint8 color_component) {
  double color_component_d = static_cast<double>(color_component) / 255.0;
  if (color_component_d > 0.04045) {
    double base = (color_component_d + kCIEConversionAlpha) /
                      (1 + kCIEConversionAlpha);
    return pow(base, kCIEConversionGamma);
  } else {
    return color_component_d / 12.92;
  }
}

uint8 sRGBColorComponentFromLinearComponent(double component) {
  double result;
  if (component <= 0.0031308) {
    result = 12.92 * component;
  } else {
    result = (1 + kCIEConversionAlpha) *
                 pow(component, (static_cast<double>(1) / 2.4)) -
                 kCIEConversionAlpha;
  }
  return std::min(static_cast<uint8>(255), static_cast<uint8>(result * 255));
}

double LabConvertNonLinear(double value) {
  if (value > 0.008856) {
    double goat = pow(value, static_cast<double>(1) / 3);
    return goat;
  }
  return (kK * value + 16) / 116;
}

double gen_yr(const LabColor& lab) {
  if (lab.L > (kE * kK))
    return pow((lab.L + 16.0) / 116, 3.0);
  return static_cast<double>(lab.L) / kK;
}

double fy(const LabColor& lab) {
  double yr = gen_yr(lab);
  if (yr > kE)
    return (lab.L + 16.0) / 116;
  return (kK * yr + 16.0) / 116;
}

double fx(const LabColor& lab) {
  return (static_cast<double>(lab.a) / 500) + fy(lab);
}

double gen_xr(const LabColor& lab) {
  double x = fx(lab);
  double x_cubed = pow(x, 3.0);
  if (x_cubed > kE)
    return x_cubed;
  return (116.0 * x - 16.0) / kK;
}

double fz(const LabColor& lab) {
  return fy(lab) - (static_cast<double>(lab.b) / 200);
}

double gen_zr(const LabColor& lab) {
  double z = fz(lab);
  double z_cubed = pow(z, 3.0);
  if (z_cubed > kE)
    return z_cubed;
  return (116.0 * z - 16.0) / kK;
}

int GetLumaForColor(SkColor* color) {
  int r = SkColorGetR(*color);
  int g = SkColorGetG(*color);
  int b = SkColorGetB(*color);

  int luma = static_cast<int>(0.3*r + 0.59*g + 0.11*b);
  if (luma < 0)
    luma = 0;
  else if (luma > 255)
    luma = 255;

  return luma;
}

// Next three functions' formulas from:
// http://www.w3.org/TR/WCAG20/#relativeluminancedef
// http://www.w3.org/TR/WCAG20/#contrast-ratiodef

double ConvertSRGB(double eight_bit_component) {
  const double component = eight_bit_component / 255.0;
  return (component <= 0.03928) ?
      (component / 12.92) : pow((component + 0.055) / 1.055, 2.4);
}

double RelativeLuminance(SkColor color) {
  return (0.2126 * ConvertSRGB(SkColorGetR(color))) +
      (0.7152 * ConvertSRGB(SkColorGetG(color))) +
      (0.0722 * ConvertSRGB(SkColorGetB(color)));
}

double ContrastRatio(SkColor color1, SkColor color2) {
  const double l1 = RelativeLuminance(color1) + 0.05;
  const double l2 = RelativeLuminance(color2) + 0.05;
  return (l1 > l2) ? (l1 / l2) : (l2 / l1);
}

}  // namespace

// ----------------------------------------------------------------------------

// Note: this works only for sRGB.
void SkColorToCIEXYZ(SkColor c, CIE_XYZ* xyz) {
  uint8 r = SkColorGetR(c);
  uint8 g = SkColorGetG(c);
  uint8 b = SkColorGetB(c);

  xyz->X = 0.4124 * CIEConvertNonLinear(r) +
           0.3576 * CIEConvertNonLinear(g) +
           0.1805 * CIEConvertNonLinear(b);
  xyz->Y = 0.2126 * CIEConvertNonLinear(r) +
           0.7152 * CIEConvertNonLinear(g) +
           0.0722 * CIEConvertNonLinear(g);
  xyz->Z = 0.0193 * CIEConvertNonLinear(r) +
           0.1192 * CIEConvertNonLinear(g) +
           0.9505 * CIEConvertNonLinear(b);
}

SkColor CIEXYZToSkColor(SkAlpha alpha, const CIE_XYZ& xyz) {
  double r_linear = 3.2410 * xyz.X - 1.5374 * xyz.Y - 0.4986 * xyz.Z;
  double g_linear = -0.9692 * xyz.X + 1.8760 * xyz.Y + 0.0416 * xyz.Z;
  double b_linear = 0.0556 * xyz.X - 0.2040 * xyz.Y + 1.0570 * xyz.Z;
  uint8 r = sRGBColorComponentFromLinearComponent(r_linear);
  uint8 g = sRGBColorComponentFromLinearComponent(g_linear);
  uint8 b = sRGBColorComponentFromLinearComponent(b_linear);
  return SkColorSetARGB(alpha, r, g, b);
}

void SkColorToLabColor(SkColor c, LabColor* lab) {
  CIE_XYZ xyz;
  SkColorToCIEXYZ(c, &xyz);
  CIEXYZToLabColor(xyz, lab);
}

SkColor LabColorToSkColor(const LabColor& lab, SkAlpha alpha) {
  CIE_XYZ xyz;
  LabColorToCIEXYZ(lab, &xyz);
  return CIEXYZToSkColor(alpha, xyz);
}

void CIEXYZToLabColor(const CIE_XYZ& xyz, LabColor* lab) {
  CIE_XYZ white_xyz;
  SkColorToCIEXYZ(SkColorSetRGB(255, 255, 255), &white_xyz);
  double fx = LabConvertNonLinear(xyz.X / white_xyz.X);
  double fy = LabConvertNonLinear(xyz.Y / white_xyz.Y);
  double fz = LabConvertNonLinear(xyz.Z / white_xyz.Z);
  lab->L = static_cast<int>(116 * fy) - 16;
  lab->a = static_cast<int>(500 * (fx - fy));
  lab->b = static_cast<int>(200 * (fy - fz));
}

void LabColorToCIEXYZ(const LabColor& lab, CIE_XYZ* xyz) {
  CIE_XYZ result;

  CIE_XYZ white_xyz;
  SkColorToCIEXYZ(SkColorSetRGB(255, 255, 255), &white_xyz);

  result.X = gen_xr(lab) * white_xyz.X;
  result.Y = gen_yr(lab) * white_xyz.Y;
  result.Z = gen_zr(lab) * white_xyz.Z;

  *xyz = result;
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

  SkColor result;
  if (color_count > 0) {
    result = SkColorSetARGB(alpha,
                            r / color_count,
                            g / color_count,
                            b / color_count);
  } else {
    result = SkColorSetARGB(alpha, 0, 0, 0);
  }
  return result;
}

void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]) {
  SkAutoLockPixels bitmap_lock(*bitmap);
  // Assume ARGB_8888 format.
  DCHECK(bitmap->getConfig() == SkBitmap::kARGB_8888_Config);

  int pixel_width = bitmap->width();
  int pixel_height = bitmap->height();
  for (int y = 0; y < pixel_height; ++y) {
    SkColor* current_color = static_cast<uint32_t*>(bitmap->getAddr32(0, y));
    for (int x = 0; x < pixel_width; ++x, ++current_color) {
      histogram[GetLumaForColor(current_color)]++;
    }
  }
}

SkColor AlphaBlend(SkColor foreground, SkColor background, SkAlpha alpha) {
  if (alpha == 0)
    return background;
  else if (alpha == 0xFF)
    return foreground;

  return SkColorSetRGB(
    ((SkColorGetR(foreground) * alpha) +
     (SkColorGetR(background) * (0xFF - alpha))) / 0xFF,
    ((SkColorGetG(foreground) * alpha) +
     (SkColorGetG(background) * (0xFF - alpha))) / 0xFF,
    ((SkColorGetB(foreground) * alpha) +
     (SkColorGetB(background) * (0xFF - alpha))) / 0xFF);
}

SkColor PickMoreReadableColor(SkColor foreground1,
                              SkColor foreground2,
                              SkColor background) {
  return (ContrastRatio(foreground1, background) >=
      ContrastRatio(foreground2, background)) ? foreground1 : foreground2;
}

SkColor GetSysSkColor(int which) {
#if defined(OS_WIN)
  return skia::COLORREFToSkColor(::GetSysColor(which));
#else
  NOTIMPLEMENTED();
  return SK_ColorLTGRAY;
#endif
}

}  // namespace color_utils
