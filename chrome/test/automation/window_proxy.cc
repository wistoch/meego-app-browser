// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/window_proxy.h"

#include <vector>
#include <algorithm>

#include "base/logging.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"

bool WindowProxy::SimulateOSClick(const gfx::Point& click, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowClick(0, handle_, click, flags));
}

bool WindowProxy::SimulateOSMouseMove(const gfx::Point& location) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowMouseMove(0, handle_, location));
}

bool WindowProxy::GetWindowTitle(string16* text) {
  if (!is_valid()) return false;

  if (!text) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_WindowTitle(0, handle_, text));
}

bool WindowProxy::SimulateOSKeyPress(base::KeyboardCode key, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowKeyPress(0, handle_, key, flags));
}

bool WindowProxy::SetVisible(bool visible) {
  if (!is_valid()) return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetWindowVisible(0, handle_, visible,
                                                   &result));
  return result;
}

bool WindowProxy::IsActive(bool* active) {
  if (!is_valid()) return false;

  bool result = false;

  sender_->Send(new AutomationMsg_IsWindowActive(0, handle_, &result, active));
  return result;
}

bool WindowProxy::Activate() {
  if (!is_valid()) return false;

  return sender_->Send(new AutomationMsg_ActivateWindow(0, handle_));
}

bool WindowProxy::GetViewBounds(int view_id, gfx::Rect* bounds,
                                bool screen_coordinates) {
  if (!is_valid())
    return false;

  if (!bounds) {
    NOTREACHED();
    return false;
  }

  bool result = false;

  if (!sender_->Send(new AutomationMsg_WindowViewBounds(
          0, handle_, view_id, screen_coordinates, &result, bounds))) {
    return false;
  }

  return result;
}

bool WindowProxy::GetBounds(gfx::Rect* bounds) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_GetWindowBounds(0, handle_, bounds,
                                                  &result));
  return result;
}

bool WindowProxy::SetBounds(const gfx::Rect& bounds) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_SetWindowBounds(0, handle_, bounds,
                                                  &result));
  return result;
}

bool WindowProxy::GetFocusedViewID(int* view_id) {
  if (!is_valid()) return false;

  if (!view_id) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_GetFocusedViewID(0, handle_,
                                                          view_id));
}

scoped_refptr<BrowserProxy> WindowProxy::GetBrowser() {
  return GetBrowserWithTimeout(base::kNoTimeout, NULL);
}

scoped_refptr<BrowserProxy> WindowProxy::GetBrowserWithTimeout(
    uint32 timeout_ms, bool* is_timeout) {
  if (!is_valid())
    return NULL;

  bool handle_ok = false;
  int browser_handle = 0;

  sender_->Send(new AutomationMsg_BrowserForWindow(0, handle_, &handle_ok,
                                                   &browser_handle));
  if (!handle_ok)
    return NULL;

  BrowserProxy* browser =
      static_cast<BrowserProxy*>(tracker_->GetResource(browser_handle));
  if (!browser) {
    browser = new BrowserProxy(sender_, tracker_, browser_handle);
    browser->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<BrowserProxy> result;
  result.swap(&browser);
  return result;
}

bool WindowProxy::IsMaximized(bool* maximized) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_IsWindowMaximized(0, handle_, maximized,
                                                    &result));
  return result;
}
