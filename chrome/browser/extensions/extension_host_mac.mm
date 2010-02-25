// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host_mac.h"

#import "chrome/browser/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/common/native_web_keyboard_event.h"

RenderWidgetHostView* ExtensionHostMac::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  // A RenderWidgetHostViewMac has lifetime scoped to the view. We'll retain it
  // to allow it to survive the trip without being hosed.
  RenderWidgetHostView* widget_view =
      ExtensionHost::CreateNewWidgetInternal(route_id, activatable);
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_view);
  [widget_view_mac->native_view() retain];

  // |widget_view_mac| needs to know how to position itself in our view.
  widget_view_mac->set_parent_view(view()->native_view());

  return widget_view;
}

void ExtensionHostMac::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  ExtensionHost::ShowCreatedWidgetInternal(widget_host_view, initial_pos);

  // A RenderWidgetHostViewMac has lifetime scoped to the view. Now that it's
  // properly embedded (or purposefully ignored) we can release the reference we
  // took in CreateNewWidgetInternal().
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_host_view);
  [widget_view_mac->native_view() release];
}

void ExtensionHostMac::UnhandledKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char ||
      extension_host_type() != ViewType::EXTENSION_POPUP) {
    return;
  }

  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>([view()->native_view() window]);
  DCHECK([event_window isKindOfClass:[ChromeEventProcessingWindow class]]);
  [event_window redispatchKeyEvent:event.os_event];
}
