// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "gfx/canvas_skia.h"

#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "gfx/font.h"
#include "gfx/rect.h"
#include "third_party/skia/include/core/SkShader.h"

namespace gfx {

CanvasSkia::CanvasSkia(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

CanvasSkia::CanvasSkia() : skia::PlatformCanvas() {
}

CanvasSkia::~CanvasSkia() {
}

// static
void CanvasSkia::SizeStringInt(const std::wstring& text,
                               const gfx::Font& font,
                               int *width, int *height, int flags) {
  NSFont* native_font = font.nativeFont();
  NSString* ns_string = base::SysWideToNSString(text);
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:native_font
                                  forKey:NSFontAttributeName];
  NSSize string_size = [ns_string sizeWithAttributes:attributes];
  *width = string_size.width;
  *height = font.height();
}

void CanvasSkia::DrawStringInt(const std::wstring& text, const gfx::Font& font,
                               const SkColor& color, int x, int y, int w, int h,
                               int flags) {
  if (!IntersectsClipRectInt(x, y, w, h))
    return;

  CGContextRef context = beginPlatformPaint();
  CGContextSaveGState(context);

  NSColor* ns_color = [NSColor colorWithDeviceRed:SkColorGetR(color) / 255.0
                                            green:SkColorGetG(color) / 255.0
                                             blue:SkColorGetB(color) / 255.0
                                            alpha:SkColorGetA(color) / 255.0];
  NSMutableParagraphStyle *ns_style =
      [[[NSParagraphStyle alloc] init] autorelease];
  if (flags & TEXT_ALIGN_CENTER)
    [ns_style setAlignment:NSCenterTextAlignment];
  // TODO(awalker): Implement the rest of the Canvas text flags

  NSDictionary* attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
          font.nativeFont(), NSFontAttributeName,
          ns_color, NSForegroundColorAttributeName,
          ns_style, NSParagraphStyleAttributeName,
          nil];

  NSAttributedString* ns_string =
      [[[NSAttributedString alloc] initWithString:base::SysWideToNSString(text)
                                        attributes:attributes] autorelease];
  scoped_cftyperef<CTFramesetterRef> framesetter(
      CTFramesetterCreateWithAttributedString(reinterpret_cast<CFAttributedStringRef>(ns_string)));

  CGRect text_bounds = CGRectMake(x, y, w, h);
  CGMutablePathRef path = CGPathCreateMutable();
  CGPathAddRect(path, NULL, text_bounds);

  scoped_cftyperef<CTFrameRef> frame(
      CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, NULL));
  CTFrameDraw(frame, context);
  CGContextRestoreGState(context);
  endPlatformPaint();
}

}  // namespace gfx
