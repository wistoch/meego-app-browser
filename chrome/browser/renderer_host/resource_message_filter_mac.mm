// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"

#import <Cocoa/Cocoa.h>

namespace {

// Adjusts an NSRect in screen coordinates to have an origin in the upper left,
// and stuffs it into a gfx::Rect. This is likely incorrect for a multiple-
// monitor setup.
gfx::Rect NSRectToRect(const NSRect rect, NSScreen* screen) {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y([screen frame].size.height - new_rect.y() - new_rect.height());
  return new_rect;
}

}

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                            IPC::Message* reply_msg) {
  NSView* view = gfx::NativeViewFromId(window_id);
  gfx::Rect rect;

  if (view) {
    NSRect bounds = [view bounds];
    bounds = [view convertRect:bounds toView:nil];
    bounds.origin = [[view window] convertBaseToScreen:bounds.origin];
    rect = NSRectToRect(bounds, [[view window] screen]);
  }

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}

void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                                IPC::Message* reply_msg) {
  NSView* view = gfx::NativeViewFromId(window_id);
  gfx::Rect rect;
  if (view) {
    NSWindow* window = [view window];
    NSRect bounds = [window frame];
    gfx::Rect rect = NSRectToRect(bounds, [window screen]);
  }

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}
