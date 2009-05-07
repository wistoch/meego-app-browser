// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/browser.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/profile.h"

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller,
                                       NSWindow* window)
  : window_(window), browser_(browser), controller_(controller) {
  // This pref applies to all windows, so all must watch for it.
  NotificationService* ns = NotificationService::current();
  ns->AddObserver(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                  NotificationService::AllSources());
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
  NotificationService* ns = NotificationService::current();
  ns->RemoveObserver(this,
                     NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                     NotificationService::AllSources());
}

void BrowserWindowCocoa::Show() {
  [window_ makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  NSRect cocoa_bounds = NSMakeRect(bounds.x(), 0, bounds.width(),
                                   bounds.height());
  // flip coordinates
  NSScreen* screen = [window_ screen];
  cocoa_bounds.origin.y =
      [screen frame].size.height - bounds.height() - bounds.y();

  [window_ setFrame:cocoa_bounds display:YES];
}

// Callers assume that this doesn't immediately delete the Browser object.
// The controller implementing the window delegate methods called from
// |-performClose:| must take precautiions to ensure that.
void BrowserWindowCocoa::Close() {
  [window_ orderOut:controller_];
  [window_ performClose:controller_];
}

void BrowserWindowCocoa::Activate() {
  [window_ makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::FlashFrame() {
  [[NSApplication sharedApplication]
      requestUserAttention:NSInformationalRequest];
}

bool BrowserWindowCocoa::IsActive() const {
  return [window_ isKeyWindow];
}

gfx::NativeWindow BrowserWindowCocoa::GetNativeHandle() {
  return [controller_ window];
}

BrowserWindowTesting* BrowserWindowCocoa::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return [controller_ statusBubble];
}

void BrowserWindowCocoa::SelectedTabToolbarSizeChanged(bool is_animating) {
  // According to beng, this is an ugly method that comes from the days when the
  // download shelf was a ChromeView attached to the TabContents, and as its
  // size changed via animation it notified through TCD/etc to the browser view
  // to relayout for each tick of the animation. We don't need anything of the
  // sort on Mac.
}

void BrowserWindowCocoa::UpdateTitleBar() {
  // This is used on windows to update the favicon and title in the window
  // icon, which we don't use on the mac.
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  [controller_ updateLoadingAnimations:should_animate ? YES : NO];
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
  [controller_ setStarredState:is_starred ? YES : NO];
}

gfx::Rect BrowserWindowCocoa::GetNormalBounds() const {
  // TODO(pinkerton): not sure if we can get the non-zoomed bounds, or if it
  // really matters. We may want to let Cocoa handle all this for us.
  NSRect frame = [window_ frame];
  NSScreen* screen = [window_ screen];
  gfx::Rect bounds(frame.origin.x, 0, frame.size.width, frame.size.height);
  bounds.set_y([screen frame].size.height - frame.origin.y - frame.size.height);
  return bounds;
}

bool BrowserWindowCocoa::IsMaximized() const {
  return [window_ isZoomed];
}

void BrowserWindowCocoa::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool BrowserWindowCocoa::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect BrowserWindowCocoa::GetRootWindowResizerRect() const {
  NSRect tabRect = [controller_ selectedTabGrowBoxRect];
  return gfx::Rect(NSRectToCGRect(tabRect));
}

LocationBar* BrowserWindowCocoa::GetLocationBar() const {
  return [controller_ locationBar];
}

void BrowserWindowCocoa::SetFocusToLocationBar() {
  [controller_ focusLocationBar];
}

void BrowserWindowCocoa::UpdateStopGoState(bool is_loading, bool force) {
  [controller_ setIsLoading:is_loading ? YES : NO];
}

void BrowserWindowCocoa::UpdateToolbar(TabContents* contents,
                                       bool should_restore_state) {
  [controller_ updateToolbarWithContents:contents
                      shouldRestoreState:should_restore_state ? YES : NO];
}

void BrowserWindowCocoa::FocusToolbar() {
  NOTIMPLEMENTED();
}

bool BrowserWindowCocoa::IsBookmarkBarVisible() const {
  return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

// This is called from Browser, which in turn is called directly from
// a menu option.  All we do here is set a preference.  The act of
// setting the preference sends notifications to all windows who then
// know what to do.
void BrowserWindowCocoa::ToggleBookmarkBar() {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

void BrowserWindowCocoa::AddFindBar(
    FindBarCocoaController* find_bar_cocoa_controller) {
  return [controller_ addFindBar:find_bar_cocoa_controller];
}

void BrowserWindowCocoa::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowBookmarkManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowReportBugDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowClearBrowsingDataDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowImportDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowSearchEnginesDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowPasswordManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowSelectProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowNewProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  NOTIMPLEMENTED();
  browser_->InProgressDownloadResponse(false);
}

void BrowserWindowCocoa::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                        void* parent_window) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    // Only the key window gets a direct toggle from the menu.
    // Other windows hear about it from the notification.
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      [controller_ toggleBookmarkBar];
      break;
    default:
      NOTREACHED();  // we don't ask for anything else!
      break;
  }
}

void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}
