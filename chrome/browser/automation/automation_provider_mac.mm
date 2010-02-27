// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/tab_window_controller.h"
#include "chrome/test/automation/automation_messages.h"
#include "grit/generated_resources.h"

void AutomationProvider::SetWindowBounds(int handle, const gfx::Rect& bounds,
                                         bool* success) {
  *success = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    NSRect new_bounds = NSRectFromCGRect(bounds.ToCGRect());

    // This is likely incorrect for a multiple-monitor setup; OK because this
    // is used only for testing purposes.
    new_bounds.origin.y = [[window screen] frame].size.height -
        new_bounds.origin.y - new_bounds.size.height;

    [window setFrame:new_bounds display:NO];
    *success = true;
  }
}

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) {
  *result = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    if (visible) {
      [window orderFront:nil];
    } else {
      [window orderOut:nil];
    }
    *result = true;
  }
}

void AutomationProvider::WindowGetViewBounds(int handle, int view_id,
                                             bool screen_coordinates,
                                             bool* success,
                                             gfx::Rect* bounds) {
  // AutomationProxyVisibleTest claims that this is used only by Chrome Views
  // which we don't use on the Mac. Is this true?

  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::ActivateWindow(int handle) { NOTIMPLEMENTED(); }

void AutomationProvider::IsWindowMaximized(int handle, bool* is_maximized,
                                           bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  NOTIMPLEMENTED();
}

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

void AutomationProvider::GetAutocompleteEditText(
    int autocomplete_edit_handle, bool* success, std::wstring* text) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::SetAutocompleteEditText(int autocomplete_edit_handle,
                                                 const std::wstring& text,
                                                 bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditIsQueryInProgress(
    int autocomplete_edit_handle, bool* success, bool* query_in_progress) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditGetMatches(
    int autocomplete_edit_handle, bool* success,
    std::vector<AutocompleteMatchData>* matches) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::OnMessageFromExternalHost(int handle,
                                                   const std::string& message,
                                                   const std::string& origin,
                                                   const std::string& target) {
  NOTIMPLEMENTED();
}

void AutomationProvider::GetAutocompleteEditForBrowser(
    int browser_handle, bool* success, int* autocomplete_edit_handle) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::SetInitialFocus(const IPC::Message& message,
                                         int handle, bool reverse) {
  NOTIMPLEMENTED();
}

void AutomationProvider::GetBookmarkBarVisibility(int handle, bool* visible,
                                                  bool* animating) {
  *visible = false;
  *animating = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::WindowSimulateDrag(int handle,
                                            std::vector<gfx::Point> drag_path,
                                            int flags,
                                            bool press_escape_en_route,
                                            IPC::Message* reply_message) {
  NOTIMPLEMENTED();
  AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void AutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetWindowBounds(int handle, gfx::Rect* bounds,
                                         bool* result) {
  *result = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  NSString* title = nil;
  if ([[window delegate] isKindOfClass:[TabWindowController class]]) {
    TabWindowController* delegate =
        reinterpret_cast<TabWindowController*>([window delegate]);
    title = [delegate selectedTabTitle];
  } else {
    title = [window title];
  }
  // If we don't yet have a title, use "Untitled".
  if (![title length]) {
    text->assign(WideToUTF16(l10n_util::GetString(
        IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED)));
    return;
  }

  text->assign(base::SysNSStringToUTF16(title));
}

