// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GFX_NATIVE_WIDGET_TYPES_H_
#define GFX_NATIVE_WIDGET_TYPES_H_

#include "base/basictypes.h"
#include "build/build_config.h"

// This file provides cross platform typedefs for native widget types.
//   NativeWindow: this is a handle to a native, top-level window
//   NativeView: this is a handle to a native UI element. It may be the
//     same type as a NativeWindow on some platforms.
//   NativeViewId: Often, in our cross process model, we need to pass around a
//     reference to a "window". This reference will, say, be echoed back from a
//     renderer to the browser when it wishes to query its size. On Windows we
//     use an HWND for this.
//
//     As a rule of thumb - if you're in the renderer, you should be dealing
//     with NativeViewIds. This should remind you that you shouldn't be doing
//     direct operations on platform widgets from the renderer process.
//
//     If you're in the browser, you're probably dealing with NativeViews,
//     unless you're in the IPC layer, which will be translating between
//     NativeViewIds from the renderer and NativeViews.
//
//   NativeEditView: a handle to a native edit-box. The Mac folks wanted this
//     specific typedef.
//
// The name 'View' here meshes with OS X where the UI elements are called
// 'views' and with our Chrome UI code where the elements are also called
// 'views'.

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
struct CGContext;
#ifdef __OBJC__
@class NSView;
@class NSWindow;
@class NSTextField;
#else
class NSView;
class NSWindow;
class NSTextField;
#endif  // __OBJC__
#elif defined(TOOLKIT_USES_GTK)
typedef struct _GdkCursor GdkCursor;
typedef struct _GdkRegion GdkRegion;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _cairo cairo_t;
#endif

namespace gfx {

#if defined(OS_WIN)
typedef HWND NativeView;
typedef HWND NativeWindow;
typedef HWND NativeEditView;
typedef HDC NativeDrawingContext;
typedef HCURSOR NativeCursor;
typedef HMENU NativeMenu;
typedef HRGN NativeRegion;
#elif defined(OS_MACOSX)
typedef NSView* NativeView;
typedef NSWindow* NativeWindow;
typedef NSTextField* NativeEditView;
typedef CGContext* NativeDrawingContext;
typedef void* NativeCursor;
typedef void* NativeMenu;
#elif defined(USE_X11)
typedef GtkWidget* NativeView;
typedef GtkWindow* NativeWindow;
typedef GtkWidget* NativeEditView;
typedef cairo_t* NativeDrawingContext;
typedef GdkCursor* NativeCursor;
typedef GtkWidget* NativeMenu;
typedef GdkRegion* NativeRegion;
#endif

// Note: for test_shell we're packing a pointer into the NativeViewId. So, if
// you make it a type which is smaller than a pointer, you have to fix
// test_shell.
//
// See comment at the top of the file for usage.
typedef intptr_t NativeViewId;

#if defined(OS_WIN)
// Convert a NativeViewId to a NativeView.
// This is only used on Windows, where we pass an HWND into the renderer and
// let the renderer operate on it.  On other platforms, the renderer doesn't
// have access to native platform widgets.
static inline NativeView NativeViewFromId(NativeViewId id) {
  return reinterpret_cast<NativeView>(id);
}
#endif

// Convert a NativeView to a NativeViewId.  See the comments at the top of
// this file.
#if defined(OS_WIN) || defined(OS_MACOSX)
static inline NativeViewId IdFromNativeView(NativeView view) {
  return reinterpret_cast<NativeViewId>(view);
}
#elif defined(USE_X11)
// Not inlined because it involves pulling too many headers.
NativeViewId IdFromNativeView(NativeView view);
#endif  // defined(USE_X11)


// PluginWindowHandle is an abstraction wrapping "the types of windows
// used by NPAPI plugins".  On Windows it's an HWND, on X it's an X
// window id.
#if defined(OS_WIN)
  typedef HWND PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = NULL;
#elif defined(USE_X11)
  typedef unsigned long PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = 0;
#else
  // On OS X we don't have windowed plugins.
  // We use a NULL/0 PluginWindowHandle in shared code to indicate there
  // is no window present, so mirror that behavior here.
  //
  // The GPU plugin is currently an exception to this rule. As of this
  // writing it uses some NPAPI infrastructure, and minimally we need
  // to identify the plugin instance via this window handle. When the
  // GPU plugin becomes a full-on GPU process, this typedef can be
  // returned to a bool. For now we use a type large enough to hold a
  // pointer on 64-bit architectures in case we need this capability.
  typedef uint64 PluginWindowHandle;
  const PluginWindowHandle kNullPluginWindow = 0;
#endif

}  // namespace gfx

#endif  // GFX_NATIVE_WIDGET_TYPES_H_
