// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class BookmarkMenuBridge;
class CommandUpdater;
@class PreferencesWindowController;
class Profile;

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController : NSObject<NSUserInterfaceValidations> {
 @private
  scoped_ptr<CommandUpdater> menuState_;
  // Management of the bookmark menu which spans across all windows
  // (and Browser*s).
  scoped_ptr<BookmarkMenuBridge> bookmarkMenuBridge_;
  scoped_nsobject<PreferencesWindowController> prefsController_;
}

- (IBAction)quit:(id)sender;
- (Profile*)defaultProfile;

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender;

@end

#endif
