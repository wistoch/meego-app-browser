// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/find_bar_bridge.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"

FindBarBridge::FindBarBridge(BrowserWindowCocoa* window) {
  cocoa_controller_.reset(
      [[FindBarCocoaController alloc] initWithBrowserWindow:window]);
  [cocoa_controller_ setFindBarBridge:this];
}

FindBarBridge::~FindBarBridge() {
}

void FindBarBridge::Show() {
  [cocoa_controller_ showFindBar];
}

void FindBarBridge::Hide(bool animate) {
  [cocoa_controller_ hideFindBar];
}

void FindBarBridge::SetFocusAndSelection() {
  [cocoa_controller_ setFocusAndSelection];
}

void FindBarBridge::ClearResults(const FindNotificationDetails& results) {
  [cocoa_controller_ clearResults:results];
}

void FindBarBridge::SetFindText(const string16& find_text) {
  [cocoa_controller_ setFindText:find_text];
}

void FindBarBridge::UpdateUIForFindResult(const FindNotificationDetails& result,
                                          const string16& find_text) {
  [cocoa_controller_ updateUIForFindResult:result withText:find_text];
}

bool FindBarBridge::IsFindBarVisible() {
  return [cocoa_controller_ isFindBarVisible] ? true : false;
}

void FindBarBridge::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                          bool no_redraw) {
  NOTIMPLEMENTED();
}

void FindBarBridge::StopAnimation() {
  NOTIMPLEMENTED();
}

gfx::Rect FindBarBridge::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void FindBarBridge::SetDialogPosition(const gfx::Rect& new_pos,
                                      bool no_redraw) {
  // TODO(rohitrao): Do something useful here.  For now, just show the findbar.
  NOTIMPLEMENTED();
  Show();
}

void FindBarBridge::RestoreSavedFocus() {
  NOTIMPLEMENTED();
}
