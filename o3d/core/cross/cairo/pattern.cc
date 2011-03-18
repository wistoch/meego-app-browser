/*
 * Copyright 2011, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "core/cross/cairo/pattern.h"

#include <cairo.h>

#include "core/cross/pack.h"
#include "core/cross/cairo/texture_cairo.h"

namespace o3d {

namespace o2d {

O3D_DEFN_CLASS(Pattern, ObjectBase);

// Cairo supports more pattern types than just these three, but we don't expose
// the others.

Pattern* Pattern::CreateTexturePattern(Pack* pack, Texture* texture) {
  TextureCairo* texture_cairo = down_cast<TextureCairo*>(texture);
  return WrapCairoPattern(
      pack,
      cairo_pattern_create_for_surface(texture_cairo->image_surface()),
      texture_cairo);
}

Pattern* Pattern::CreateRgbPattern(Pack* pack,
                                   double red,
                                   double green,
                                   double blue) {
  return WrapCairoPattern(pack,
                          cairo_pattern_create_rgb(red, green, blue),
                          NULL);
}

Pattern* Pattern::CreateRgbaPattern(Pack* pack,
                                    double red,
                                    double green,
                                    double blue,
                                    double alpha) {
  return WrapCairoPattern(pack,
                          cairo_pattern_create_rgba(red, green, blue, alpha),
                          NULL);
}

Pattern::~Pattern() {
  cairo_pattern_destroy(pattern_);
}

void Pattern::SetAffineTransform(double xx,
                                 double yx,
                                 double xy,
                                 double yy,
                                 double x0,
                                 double y0) {
  cairo_matrix_t matrix;
  cairo_matrix_init(&matrix, xx, yx, xy, yy, x0, y0);
  cairo_pattern_set_matrix(pattern_, &matrix);
  set_content_dirty(true);
}

void Pattern::set_extend(ExtendType extend) {
  cairo_extend_t cairo_extend;
  switch (extend) {
    case NONE:
      cairo_extend = CAIRO_EXTEND_NONE;
      break;
    case REPEAT:
      cairo_extend = CAIRO_EXTEND_REPEAT;
      break;
    case REFLECT:
      cairo_extend = CAIRO_EXTEND_REFLECT;
      break;
    case PAD:
      cairo_extend = CAIRO_EXTEND_PAD;
      break;
    default:
      DCHECK(false);
      return;
  }
  cairo_pattern_set_extend(pattern_, cairo_extend);
  set_content_dirty(true);
}

void Pattern::set_filter(FilterType filter) {
  cairo_filter_t cairo_filter;
  switch (filter) {
    case FAST:
      cairo_filter = CAIRO_FILTER_FAST;
      break;
    case GOOD:
      cairo_filter = CAIRO_FILTER_GOOD;
      break;
    case BEST:
      cairo_filter = CAIRO_FILTER_BEST;
      break;
    case NEAREST:
      cairo_filter = CAIRO_FILTER_NEAREST;
      break;
    case BILINEAR:
      cairo_filter = CAIRO_FILTER_BILINEAR;
      break;
    default:
      DCHECK(false);
      return;
  }
  cairo_pattern_set_filter(pattern_, cairo_filter);
  set_content_dirty(true);
}

Pattern::Pattern(ServiceLocator* service_locator,
                 cairo_pattern_t* pattern,
                 TextureCairo* texture)
    : ObjectBase(service_locator),
      pattern_(pattern),
      texture_(texture),
      content_dirty_(false) {
}

Pattern* Pattern::WrapCairoPattern(Pack* pack,
                                   cairo_pattern_t* pattern,
                                   TextureCairo* texture) {
  cairo_status_t status = cairo_pattern_status(pattern);
  if (CAIRO_STATUS_SUCCESS != status) {
    DLOG(ERROR) << "Error creating Cairo pattern: " << status;
    cairo_pattern_destroy(pattern);
    return NULL;
  }
  Pattern* p = new Pattern(pack->service_locator(), pattern, texture);
  pack->RegisterObject(p);
  return p;
}

}  // namespace o2d

}  // namespace o3d
