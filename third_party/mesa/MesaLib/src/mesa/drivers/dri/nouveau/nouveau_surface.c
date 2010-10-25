/*
 * Copyright (C) 2009 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "nouveau_driver.h"
#include "nouveau_context.h"
#include "nouveau_util.h"

void
nouveau_surface_alloc(GLcontext *ctx, struct nouveau_surface *s,
		      enum nouveau_surface_layout layout,
		      unsigned flags, unsigned format,
		      unsigned width, unsigned height)
{
	unsigned tile_mode, cpp = _mesa_get_format_bytes(format);
	int ret;

	nouveau_bo_ref(NULL, &s->bo);

	*s = (struct nouveau_surface) {
		.layout = layout,
		.format = format,
		.width = width,
		.height = height,
		.cpp = cpp,
		.pitch = width * cpp,
	};

	if (layout == TILED) {
		s->pitch = align(s->pitch, 256);
		tile_mode = s->pitch;
	} else {
		s->pitch = align(s->pitch, 64);
		tile_mode = 0;
	}

	ret = nouveau_bo_new_tile(context_dev(ctx), flags, 0, s->pitch * height,
				  tile_mode, 0, &s->bo);
	assert(!ret);
}

void
nouveau_surface_ref(struct nouveau_surface *src,
		    struct nouveau_surface *dst)
{
	if (src) {
		dst->offset = src->offset;
		dst->layout = src->layout;
		dst->format = src->format;
		dst->width = src->width;
		dst->height = src->height;
		dst->cpp = src->cpp;
		dst->pitch = src->pitch;
		nouveau_bo_ref(src->bo, &dst->bo);

	} else {
		nouveau_bo_ref(NULL, &dst->bo);
	}
}
