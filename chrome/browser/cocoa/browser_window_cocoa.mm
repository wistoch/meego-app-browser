// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/bug_report_window_controller.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/download_shelf_controller.h"
#import "chrome/browser/cocoa/html_dialog_window_controller.h"
#import "chrome/browser/cocoa/import_settings_dialog.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#import "chrome/browser/cocoa/nsmenuitem_additions.h"
#include "chrome/browser/cocoa/page_info_window_mac.h"
#include "chrome/browser/cocoa/repost_form_warning_mac.h"
#include "chrome/browser/cocoa/status_bubble_mac.h"
#include "chrome/browser/cocoa/task_manager_mac.h"
#import "chrome/browser/cocoa/theme_install_bubble_view.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/temp_scaffolding_stubs.h"
#include "chrome/browser/profile.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

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
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behaviour under
  // Windows, but |-makeKeyAndOrderFront:| won't send |-windowDidBecomeMain:|
  // until we return to the runloop. Therefore any calls to
  // |BrowserList::GetLastActive()| (for example, in bookmark_util), will return
  // the previous browser instead if we don't explicitly set it here.
  BrowserList::SetLastActive(browser_);

  [window_ makeKeyAndOrderFront:controller_];
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  NSRect cocoa_bounds = NSMakeRect(bounds.x(), 0, bounds.width(),
                                   bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
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
  [controller_ activate];
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
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [window_ frame];
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

bool BrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  return false;
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

bool BrowserWindowCocoa::IsToolbarVisible() const {
  NOTIMPLEMENTED();
  return true;
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
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (current_tab && current_tab->controller().GetActiveEntry()) {
    BugReportWindowController* controller =
        [[BugReportWindowController alloc]
        initWithTabContents:current_tab
                    profile:browser_->profile()];
    [controller runModalDialog];
  }
}

void BrowserWindowCocoa::ShowClearBrowsingDataDialog() {
  scoped_nsobject<ClearBrowsingDataController> controller(
      [[ClearBrowsingDataController alloc]
          initWithProfile:browser_->profile()]);
  [controller runModalDialog];
}

void BrowserWindowCocoa::ShowImportDialog() {
  // Note that the dialog controller takes care of cleaning itself up
  // upon dismissal so auto-scoping here is not necessary.
  [[[ImportSettingsDialogController alloc]
      initWithProfile:browser_->profile() parentWindow:window_] runModalDialog];
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
  new RepostFormWarningMac(GetNativeHandle(), &tab_contents->controller());
}

void BrowserWindowCocoa::ShowProfileErrorDialog(int message_id) {
  scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(IDS_OK)];
  [alert setMessageText:l10n_util::GetNSStringWithFixup(IDS_PRODUCT_NAME)];
  [alert setInformativeText:l10n_util::GetNSStringWithFixup(message_id)];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert runModal];
}

void BrowserWindowCocoa::ShowThemeInstallBubble() {
  ThemeInstallBubbleView::Show(window_);
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  browser_->InProgressDownloadResponse(true);
}

void BrowserWindowCocoa::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                        gfx::NativeWindow parent_window) {
  [HtmlDialogWindowController showHtmlDialog:delegate
                                     profile:browser_->profile()];
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

@interface MenuWalker : NSObject
+ (NSMenuItem*)itemForKeyEquivalent:(NSEvent*)key
                               menu:(NSMenu*)menu;
@end

@implementation MenuWalker
+ (NSMenuItem*)itemForKeyEquivalent:(NSEvent*)key
                               menu:(NSMenu*)menu {
  NSMenuItem* result = nil;

  for (NSMenuItem *item in [menu itemArray]) {
    NSMenu* submenu = [item submenu];
    if (submenu) {
      if (submenu != [NSApp servicesMenu])
        result = [self itemForKeyEquivalent:key
                                       menu:submenu];
    } else if ([item cr_firesForKeyEvent:key]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
}
@end

int BrowserWindowCocoa::GetCommandId(const NativeWebKeyboardEvent& event) {
  if ([event.os_event type] != NSKeyDown)
    return -1;

  // Look in menu.
  NSMenuItem* item = [MenuWalker itemForKeyEquivalent:event.os_event
                                                 menu:[NSApp mainMenu]];

  if (item && [item action] == @selector(commandDispatch:) && [item tag] > 0)
    return [item tag];

  // "Close window" doesn't use the |commandDispatch:| mechanism. Menu items
  // that do not correspond to IDC_ constants need no special treatment however,
  // as they can't be blacklisted in |Browser::IsReservedAccelerator()| anyhow.
  if (item && [item action] == @selector(performClose:))
    return IDC_CLOSE_WINDOW;

  // Look in secondary keyboard shortcuts.
  NSUInteger modifiers = [event.os_event modifierFlags];
  const bool cmdKey = (modifiers & NSCommandKeyMask) != 0;
  const bool shiftKey = (modifiers & NSShiftKeyMask) != 0;
  const bool cntrlKey = (modifiers & NSControlKeyMask) != 0;
  const bool optKey = (modifiers & NSAlternateKeyMask) != 0;
  const int keyCode = [event.os_event keyCode];

  int cmdNum = CommandForWindowKeyboardShortcut(
      cmdKey, shiftKey, cntrlKey, optKey, keyCode);
  if (cmdNum != -1)
    return cmdNum;

  cmdNum = CommandForBrowserKeyboardShortcut(
      cmdKey, shiftKey, cntrlKey, optKey, keyCode);
  if (cmdNum != -1)
    return cmdNum;

  return -1;
}

void BrowserWindowCocoa::ShowCreateShortcutsDialog(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    // Only the key window gets a direct toggle from the menu.
    // Other windows hear about it from the notification.
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      [controller_ updateBookmarkBarVisibilityWithAnimation:YES];
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
