// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_WINDOW_SIZE_AUTOSAVER_H_
#define CHROME_BROWSER_COCOA_WINDOW_SIZE_AUTOSAVER_H_
#pragma once

class PrefService;
@class NSWindow;

enum WindowSizeAutosaverState {

  // Autosave only the window's bottom-right corner.
  kSaveWindowPos,

  // Autosave the whole window rect, i.e. both position and size.
  kSaveWindowRect,
};

// WindowSizeAutosaver is a helper class that makes it easy to let windows
// autoremember their position or position and size in a PrefService object.
// To use this, add a |scoped_nsobject<WindowSizeAutosaver>| to your window
// controller and initialize it in the window controller's init method, passing
// a window and an autosave name. The autosaver will register for "window moved"
// and "window resized" notifications and write the current window state to the
// prefs service every time they fire. The window's size is automatically
// restored when the autosaver's |initWithWindow:...| method is called.
//
// Note: Your xib file should have "Visible at launch" UNCHECKED, so that the
// initial repositioning is not visible.
@interface WindowSizeAutosaver : NSObject {
  NSWindow* window_;  // weak
  PrefService* prefService_;  // weak
  const char* path_;
  WindowSizeAutosaverState state_;
}

- (id)initWithWindow:(NSWindow*)window
         prefService:(PrefService*)prefs
                path:(const char*)path
               state:(WindowSizeAutosaverState)state;
@end

#endif  // CHROME_BROWSER_COCOA_WINDOW_SIZE_AUTOSAVER_H_

