// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "app_controller_mac.h"

#import "base/message_loop.h"
#import "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/browser.h"
#import "chrome/browser/browser_list.h"
#import "chrome/browser/command_updater.h"
#import "chrome/common/temp_scaffolding_stubs.h"

@interface AppController(PRIVATE)
- (void)initMenuState;
@end

@implementation AppController

- (void)awakeFromNib {
  // set up the command updater for when there are no windows open
  [self initMenuState];
}

- (void)dealloc {
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

  // Close all the windows.
  // TODO(pinkerton): the close code assumes that teardown happens 
  // synchronously, however with autorelease pools and ref-counting, we can't
  // guarantee the window controller hits 0 inside this call, and thus the
  // number of Browsers still alive will certainly be non-zero. Not sure yet
  // how to handle this case.
  // BrowserList::CloseAllBrowsers(false);
  
  MessageLoopForUI::current()->Quit();
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
  NSInteger tag = [sender tag];
  switch (tag) {
    case IDC_NEW_WINDOW:
      Browser::OpenEmptyWindow(ProfileManager::FakeProfile());
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      Browser::OpenURLOffTheRecord(ProfileManager::FakeProfile(), GURL());
      break;
  };
}

- (void)initMenuState {
  menuState_ = new CommandUpdater(NULL);
  menuState_->UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW, true);
  // TODO(pinkerton): ...more to come...
}

@end
