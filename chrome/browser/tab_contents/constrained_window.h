// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
#define CHROME_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_

#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

// The different platform specific subclasses use different delegates for their
// dialogs.
#if defined(OS_WIN)
namespace views {
class WindowDelegate;
}
typedef views::WindowDelegate ConstrainedWindowDelegate;
#elif defined(OS_LINUX)
class ConstrainedWindowGtkDelegate;
typedef ConstrainedWindowGtkDelegate ConstrainedWindowDelegate;
#elif defined(OS_MACOSX)
class ConstrainedWindowMacDelegate;
typedef ConstrainedWindowMacDelegate ConstrainedWindowDelegate;
#endif

class TabContents;

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a TabContents'
//  bounds.
//
class ConstrainedWindow {
 public:
  // Create a Constrained Window that contains a platform specific client
  // area. Typical uses include the HTTP Basic Auth prompt. The caller must
  // provide a delegate to describe the content area and to respond to events.
  static ConstrainedWindow* CreateConstrainedDialog(
      TabContents* owner,
      ConstrainedWindowDelegate* delegate);

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;
 protected:
  ~ConstrainedWindow() {}
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
