// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/download_shelf_controller.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#include "chrome/browser/cocoa/page_info_window_mac.h"
#include "chrome/browser/cocoa/status_bubble_mac.h"
#include "chrome/browser/cocoa/task_manager_mac.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/temp_scaffolding_stubs.h"
#include "chrome/browser/profile.h"

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller,
                                       NSWindow* window)
  : window_(window),
    browser_(browser),
    controller_(controller) {
  // This pref applies to all windows, so all must watch for it.
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
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
// |-performClose:| must take precautions to ensure that.
void BrowserWindowCocoa::Close() {
  // If there is an overlay window, we contain a tab being dragged between
  // windows. Don't hide the window as it makes the UI extra confused. We can
  // still close the window, as that will happen when the drag completes.
  if ([controller_ overlayWindow])
    [controller_ deferPerformClose];
  else {
    // Make sure we hide the window immediately. Even though performClose:
    // calls orderOut: eventually, it leaves the window on-screen long enough
    // that we start to see tabs shutting down. http://crbug.com/23959
    [window_ orderOut:controller_];
    [window_ performClose:controller_];
  }
}

void BrowserWindowCocoa::Activate() {
  [window_ makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::FlashFrame() {
  [NSApp requestUserAttention:NSInformationalRequest];
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

void BrowserWindowCocoa::SelectedTabExtensionShelfSizeChanged() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateTitleBar() {
  NSString* newTitle =
      base::SysUTF16ToNSString(browser_->GetWindowTitleForCurrentTab());

  [window_ setTitle:newTitle];
}

void BrowserWindowCocoa::ShelfVisibilityChanged() {
  // Mac doesn't yet support showing the bookmark bar at a different size on
  // the new tab page. When it does, this method should attempt to relayout the
  // bookmark bar/extension shelf as their preferred height may have changed.
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateDevTools() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::FocusDevTools() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  // Do nothing on Mac.
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
  [controller_ setStarredState:is_starred ? YES : NO];
}

gfx::Rect BrowserWindowCocoa::GetRestoredBounds() const {
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
  [controller_ setFullscreen:fullscreen];
}

bool BrowserWindowCocoa::IsFullscreen() const {
  return !![controller_ isFullscreen];
}

gfx::Rect BrowserWindowCocoa::GetRootWindowResizerRect() const {
  NSRect tabRect = [controller_ selectedTabGrowBoxRect];
  return gfx::Rect(NSRectToCGRect(tabRect));
}

void BrowserWindowCocoa::ConfirmAddSearchProvider(
    const TemplateURL* template_url,
    Profile* profile) {
  NOTIMPLEMENTED();
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

void BrowserWindowCocoa::ToggleExtensionShelf() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::AddFindBar(
    FindBarCocoaController* find_bar_cocoa_controller) {
  return [controller_ addFindBar:find_bar_cocoa_controller];
}

void BrowserWindowCocoa::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowTaskManager() {
  TaskManagerMac::Show();
}

void BrowserWindowCocoa::ShowBookmarkManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  [controller_ showBookmarkBubbleForURL:url
                      alreadyBookmarked:(already_bookmarked ? YES : NO)];
}

bool BrowserWindowCocoa::IsDownloadShelfVisible() const {
  return [controller_ isDownloadShelfVisible] != NO;
}

DownloadShelf* BrowserWindowCocoa::GetDownloadShelf() {
  DownloadShelfController* shelfController = [controller_ downloadShelf];
  return [shelfController bridge];
}

void BrowserWindowCocoa::ShowReportBugDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowClearBrowsingDataDialog() {
  scoped_nsobject<ClearBrowsingDataController> controller(
      [[ClearBrowsingDataController alloc]
          initWithProfile:browser_->profile()]);
  [controller runModalDialog];
}

void BrowserWindowCocoa::ShowImportDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowSearchEnginesDialog() {
  [KeywordEditorCocoaController showKeywordEditor:browser_->profile()];
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

void BrowserWindowCocoa::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowHistoryTooNewDialog() {
  NOTIMPLEMENTED();
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  browser_->InProgressDownloadResponse(true);
}

void BrowserWindowCocoa::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                        gfx::NativeWindow parent_window) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::UserChangedTheme() {
  [controller_ userChangedTheme];
}

int BrowserWindowCocoa::GetExtraRenderViewHeight() const {
  // Currently this is only used on linux.
  return 0;
}

void BrowserWindowCocoa::TabContentsFocused(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowPageInfo(Profile* profile,
                                      const GURL& url,
                                      const NavigationEntry::SSLStatus& ssl,
                                      bool show_history) {
  PageInfoWindowMac::ShowPageInfo(profile, url, ssl, show_history);
}

void BrowserWindowCocoa::ShowPageMenu() {
  // No-op. Mac doesn't support showing the menus via alt keys.
}

void BrowserWindowCocoa::ShowAppMenu() {
  // No-op. Mac doesn't support showing the menus via alt keys.
}

int BrowserWindowCocoa::GetCommandId(const NativeWebKeyboardEvent& event) {
  // TODO(port): return the command id if this is a keyboard accelerator.
  // CommandForKeyboardShortcut() doesn't have the full list.
  return -1;
}

void BrowserWindowCocoa::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    // Only the key window gets a direct toggle from the menu.
    // Other windows hear about it from the notification.
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      [controller_ updateBookmarkBarVisibility];
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
