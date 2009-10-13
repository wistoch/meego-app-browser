// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "base/gfx/rect.h"
#include "webkit/glue/plugins/fake_plugin_window_tracker_mac.h"

namespace webkit_glue {

void NotifyBrowserOfPluginSelectWindow(uint32 window_id, CGRect bounds);
void NotifyBrowserOfPluginShowWindow(uint32 window_id, CGRect bounds);
void NotifyBrowserOfPluginHideWindow(uint32 window_id, CGRect bounds);
void NotifyBrowserOfPluginDisposeWindow(uint32 window_id, CGRect bounds);

}  // namespace webkit_glue

// The process that was frontmost when a plugin created a new window; generally
// we expect this to be the browser UI process.
static ProcessSerialNumber g_saved_front_process = { 0, 0 };

// Bring the plugin process to the front so that the user can see it.
// TODO: Make this an IPC to order the plugin process above the browser
// process but not necessarily the frontmost.
static void SwitchToPluginProcess() {
  ProcessSerialNumber this_process, front_process;
  if (GetCurrentProcess(&this_process) != noErr)
    return;
  if (GetFrontProcess(&front_process) != noErr)
    return;
  Boolean matched = false;
  if (SameProcess(&this_process, &front_process, &matched) != noErr)
    return;
  if (!matched) {
    // TODO: We may need to keep a stack, or at least check the total window
    // count, since this won't work if a plugin opens more than one window at
    // a time.
    g_saved_front_process = front_process;
    SetFrontProcess(&this_process);
  }
}

// If the plugin process is still the front process, bring the prior
// front process (normally this will be the browser process) back to
// the front.
// TODO: Make this an IPC message so that the browser can properly
// reactivate the window.
static void SwitchToSavedProcess() {
  ProcessSerialNumber this_process, front_process;
  if (GetCurrentProcess(&this_process) != noErr)
    return;
  if (GetFrontProcess(&front_process) != noErr)
    return;
  Boolean matched = false;
  if (SameProcess(&this_process, &front_process, &matched) != noErr)
    return;
  if (matched) {
    SetFrontProcess(&g_saved_front_process);
  }
}

#pragma mark -

static Boolean ChromePluginIsWindowHilited(WindowRef window) {
  // TODO(stuartmorgan): Always returning true (instead of the real answer,
  // which would be false) means that clicking works, but it's not correct
  // either. Ideally we need a way to find out if the delegate corresponds
  // to a browser window that is active.
  const WebPluginDelegateImpl* delegate =
      FakePluginWindowTracker::SharedInstance()->GetDelegateForFakeWindow(
          window);
  Boolean isHilited = delegate ? true : IsWindowHilited(window);
  return isHilited;
}

static CGRect CGRectForWindow(WindowRef window) {
  CGRect bounds = { { 0, 0 }, { 0, 0 } };
  HIWindowGetBounds(window, kWindowContentRgn, kHICoordSpace72DPIGlobal,
                    &bounds);
  return bounds;
}

static void ChromePluginSelectWindow(WindowRef window) {
  SwitchToPluginProcess();
  SelectWindow(window);
  webkit_glue::NotifyBrowserOfPluginSelectWindow(HIWindowGetCGWindowID(window),
                                                 CGRectForWindow(window));
}

static void ChromePluginShowWindow(WindowRef window) {
  SwitchToPluginProcess();
  ShowWindow(window);
  webkit_glue::NotifyBrowserOfPluginShowWindow(HIWindowGetCGWindowID(window),
                                               CGRectForWindow(window));
}

static void ChromePluginDisposeWindow(WindowRef window) {
  SwitchToSavedProcess();
  webkit_glue::NotifyBrowserOfPluginDisposeWindow(HIWindowGetCGWindowID(window),
                                                  CGRectForWindow(window));
  DisposeWindow(window);
}

static void ChromePluginHideWindow(WindowRef window) {
  SwitchToSavedProcess();
  webkit_glue::NotifyBrowserOfPluginHideWindow(HIWindowGetCGWindowID(window),
                                               CGRectForWindow(window));
  HideWindow(window);
}

#pragma mark -

struct interpose_substitution {
  const void* replacement;
  const void* original;
};

#define INTERPOSE_FUNCTION(function) \
    { reinterpret_cast<const void*>(ChromePlugin##function), \
      reinterpret_cast<const void*>(function) }

__attribute__((used)) static const interpose_substitution substitutions[]
    __attribute__((section("__DATA, __interpose"))) = {
  INTERPOSE_FUNCTION(IsWindowHilited),
  INTERPOSE_FUNCTION(SelectWindow),
  INTERPOSE_FUNCTION(ShowWindow),
  INTERPOSE_FUNCTION(DisposeWindow),
  INTERPOSE_FUNCTION(HideWindow),
};
