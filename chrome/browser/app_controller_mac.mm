// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_shutdown.h"
#import "chrome/browser/cocoa/bookmark_menu_bridge.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/temp_scaffolding_stubs.h"

@interface AppController(PRIVATE)
- (void)initMenuState;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
@end

@implementation AppController

- (void)awakeFromNib {
  // Set up the command updater for when there are no windows open
  [self initMenuState];
  bookmarkMenuBridge_ = new BookmarkMenuBridge();
}

- (void)applicationDidFinishLaunching:(NSNotification*)notify {
  // Hold an extra ref to the BrowserProcess singleton so it doesn't go away
  // when all the browser windows get closed. We'll release it on quit which
  // will be the signal to exit.
  DCHECK(g_browser_process);
  g_browser_process->AddRefModule();

  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:'WWW!'    // A particularly ancient AppleEvent that dates
           andEventID:'OURL'];  // back to the Spyglass days.
}

- (void)dealloc {
  delete bookmarkMenuBridge_;
  delete menuState_;
  [super dealloc];
}

// We can't use the standard terminate: method because it will abruptly exit
// the app and leave things on the stack in an unfinalized state. We need to
// post a quit message to our run loop so the stack can gracefully unwind.
- (IBAction)quit:(id)sender {
  // TODO(pinkerton):
  // since we have to roll it ourselves, ask the delegate (ourselves, really)
  // if we should terminate. For example, we might not want to if the user
  // has ongoing downloads or multiple windows/tabs open. However, this would
  // require posting UI and may require spinning up another run loop to
  // handle it. If it says to continue, post the quit message, otherwise
  // go back to normal.

  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
  [em removeEventHandlerForEventClass:'WWW!'
                           andEventID:'OURL'];

  // TODO(pinkerton): Not sure where this should live, including it here
  // causes all sorts of asserts from the open renderers. On Windows, it
  // lives in Browser::OnWindowClosing, but that's not appropriate on Mac
  // since we don't shut down when we reach zero windows.
  // browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);

  // Close all the windows.
  BrowserList::CloseAllBrowsers(true);

  // Release the reference to the browser process. Once all the browsers get
  // dealloc'd, it will stop the RunLoop and fall back into main().
  g_browser_process->ReleaseModule();
}

// Called to validate menu items when there are no key windows. All the
// items we care about have been set with the |commandDispatch:| action and
// a target of FirstResponder in IB. If it's not one of those, let it
// continue up the responder chain to be handled elsewhere. We pull out the
// tag as the cross-platform constant to differentiate and dispatch the
// various commands.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (menuState_->SupportsCommand(tag))
      enable = menuState_->IsCommandEnabled(tag) ? YES : NO;
  } else if (action == @selector(quit:)) {
    enable = YES;
  }
  return enable;
}

// Called when the user picks a menu item when there are no key windows. Calls
// through to the browser object to execute the command. This assumes that the
// command is supported and doesn't check, otherwise it would have been disabled
// in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  Profile* default_profile = [self defaultProfile];

  NSInteger tag = [sender tag];
  switch (tag) {
    case IDC_NEW_WINDOW:
      Browser::OpenEmptyWindow(default_profile);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      Browser::OpenURLOffTheRecord(default_profile, GURL());
      break;
    case IDC_OPEN_FILE:
      Browser::OpenEmptyWindow(default_profile);
      BrowserList::GetLastActive()->
          ExecuteCommandWithDisposition(IDC_OPEN_FILE, CURRENT_TAB);
      break;
  };
}

// NSApplication delegate method called when someone clicks on the
// dock icon and there are no open windows.  To match standard mac
// behavior, we should open a new window.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)flag {
  // Don't do anything if there are visible windows.  This will cause
  // AppKit to unminimize the most recently minimized window.
  if (flag)
    return YES;

  // Otherwise open a new window.
  Browser::OpenEmptyWindow([self defaultProfile]);

  // We've handled the reopen event, so return NO to tell AppKit not
  // to do anything.
  return NO;
}

- (void)initMenuState {
  menuState_ = new CommandUpdater(NULL);
  menuState_->UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_OPEN_FILE, true);
  // TODO(pinkerton): ...more to come...
}

- (Profile*)defaultProfile {
  // TODO(jrg): Find a better way to get the "default" profile.
  if (g_browser_process->profile_manager())
    return* g_browser_process->profile_manager()->begin();

  return NULL;
}

// Various methods to open URLs that we get in a native fashion. We use
// BrowserInit here because on the other platforms, URLs to open come through
// the ProcessSingleton, and it calls BrowserInit. It's best to bottleneck the
// openings through that for uniform handling.

namespace {

void OpenURLs(const std::vector<GURL>& urls) {
  CommandLine dummy((std::wstring()));
  BrowserInit::LaunchWithProfile launch(std::wstring(), dummy);
  launch.OpenURLsInBrowser(BrowserList::GetLastActive(), false, urls);
}

}

- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply {
  NSString* urlStr = [[event paramDescriptorForKeyword:keyDirectObject]
                      stringValue];

  GURL gurl(base::SysNSStringToUTF8(urlStr));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  OpenURLs(gurlVector);
}

- (void)application:(NSApplication*)sender
          openFiles:(NSArray*)filenames {
  std::vector<GURL> gurlVector;

  for (NSString* filename in filenames) {
    NSURL* fileURL = [NSURL fileURLWithPath:filename];
    GURL gurl(base::SysNSStringToUTF8([fileURL absoluteString]));
    gurlVector.push_back(gurl);
  }

  OpenURLs(gurlVector);
}

@end
