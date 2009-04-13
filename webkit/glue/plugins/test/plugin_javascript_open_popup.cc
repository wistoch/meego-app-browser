// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#if defined(OS_LINUX)
// HACK: we need this #define in place before npapi.h is included for
// plugins to work. However, all sorts of headers include npapi.h, so
// the only way to be certain the define is in place is to put it
// here.  You might ask, "Why not set it in npapi.h directly, or in
// this directory's SConscript, then?"  but it turns out this define
// makes npapi.h include Xlib.h, which in turn defines a ton of symbols
// like None and Status, causing conflicts with the aforementioned
// many headers that include npapi.h.  Ugh.
#define MOZ_X11 1
#endif

#include "webkit/glue/plugins/test/plugin_javascript_open_popup.h"

#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

ExecuteJavascriptOpenPopupWithPluginTest::
    ExecuteJavascriptOpenPopupWithPluginTest(NPP id,
                                             NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      popup_window_test_started_(false) {
}

int16 ExecuteJavascriptOpenPopupWithPluginTest::SetWindow(
    NPWindow* window) {
  if (!popup_window_test_started_) {
    popup_window_test_started_ = true;
    HostFunctions()->geturl(
        id(), "popup_window_with_target_plugin.html", "_blank");
  }
  return PluginTest::SetWindow(window);
}

// ExecuteJavascriptPopupWindowTargetPluginTest member defines.
ExecuteJavascriptPopupWindowTargetPluginTest::
    ExecuteJavascriptPopupWindowTargetPluginTest(
        NPP id, NPNetscapeFuncs* host_functions)
    : PluginTest(id, host_functions),
      test_completed_(false) {
}

int16 ExecuteJavascriptPopupWindowTargetPluginTest::SetWindow(
    NPWindow* window) {
  if (!test_completed_) {
    if (CheckWindow(window)) {
      SignalTestCompleted();
      test_completed_ = true;
    }
  }
  return PluginTest::SetWindow(window);
}

#if defined(OS_WIN)
bool ExecuteJavascriptPopupWindowTargetPluginTest::CheckWindow(
    NPWindow* window) {
  HWND window_handle = reinterpret_cast<HWND>(window->window);

  if (IsWindow(window_handle)) {
    HWND parent_window = GetParent(window_handle);
    if (!IsWindow(parent_window) || parent_window == GetDesktopWindow())
      SetError("Windowed plugin instantiated with NULL parent");
    return true;
  }

  return false;
}

#elif defined(OS_LINUX)
// This code blindly follows the same sorts of verifications done on
// the Windows side.  Does it make sense on X?  Maybe not really, but
// it can't hurt to do extra validations.
bool ExecuteJavascriptPopupWindowTargetPluginTest::CheckWindow(
    NPWindow* window) {
  Window xwindow = reinterpret_cast<Window>(window->window);
  // Grab a pointer to the extra SetWindow data so we can grab the display out.
  NPSetWindowCallbackStruct* extra =
      static_cast<NPSetWindowCallbackStruct*>(window->ws_info);

  if (xwindow) {
    Window root, parent;
    Status status = XQueryTree(extra->display, xwindow, &root, &parent,
                               NULL, NULL);  // NULL children info.
    DCHECK(status != 0);
    if (!parent || parent == root)
      SetError("Windowed plugin instantiated with NULL parent");
    return true;
  }

  return false;
}
#endif

} // namespace NPAPIClient
