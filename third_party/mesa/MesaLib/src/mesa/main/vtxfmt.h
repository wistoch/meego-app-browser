/**
 * \file vtxfmt.h
 * 
 * \author Keith Whitwell <keith@tungstengraphics.com>
 * \author Gareth Hughes
 */

/*
 * Mesa 3-D graphics library
 * Version:  6.1
 *
 * Copyright (C) 1999-2004  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef _VTXFMT_H_
#define _VTXFMT_H_

#include "compiler.h"
#include "mtypes.h"

#if FEATURE_beginend

extern void _mesa_init_exec_vtxfmt( GLcontext *ctx );

extern void _mesa_install_exec_vtxfmt( GLcontext *ctx, const GLvertexformat *vfmt );
extern void _mesa_install_save_vtxfmt( GLcontext *ctx, const GLvertexformat *vfmt );

extern void _mesa_restore_exec_vtxfmt( GLcontext *ctx );

#else /* FEATURE_beginend */

static INLINE void
_mesa_init_exec_vtxfmt( GLcontext *ctx )
{
}

static INLINE void
_mesa_install_exec_vtxfmt( GLcontext *ctx, const GLvertexformat *vfmt )
{
}

static INLINE void
_mesa_install_save_vtxfmt( GLcontext *ctx, const GLvertexformat *vfmt )
{
}

static INLINE void
_mesa_restore_exec_vtxfmt( GLcontext *ctx )
{
}

#endif /* FEATURE_beginend */

#endif /* _VTXFMT_H_ */
