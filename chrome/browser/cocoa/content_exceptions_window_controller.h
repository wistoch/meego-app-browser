// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/common/content_settings_types.h"

class ContentExceptionsTableModel;
class UpdatingContentSettingsObserver;

// Controller for the content exception dialogs.
@interface ContentExceptionsWindowController : NSWindowController
                                               <NSWindowDelegate,
                                               NSTableViewDataSource,
                                               NSTableViewDelegate> {
 @private
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* addButton_;
  IBOutlet NSButton* removeButton_;
  IBOutlet NSButton* removeAllButton_;

  ContentSettingsType settingsType_;
  HostContentSettingsMap* settingsMap_;  // weak
  scoped_ptr<ContentExceptionsTableModel> model_;

  // Set if "Ask" should be a valid option in the "action" popup.
  BOOL showAsk_;

  // Listens for changes to the content settings and reloads the data when they
  // change. See comment in |modelDidChange| in the mm file for details.
  scoped_ptr<UpdatingContentSettingsObserver> tableObserver_;

  // If this is set to NO, notifications by |tableObserver_| are ignored. This
  // is used to suppress updates at bad times.
  BOOL updatesEnabled_;

  // This is non-NULL only while a new element is being added and its host
  // is being edited.
  scoped_ptr<HostContentSettingsMap::HostSettingPair> newException_;
}

+ (id)showForType:(ContentSettingsType)settingsType
      settingsMap:(HostContentSettingsMap*)settingsMap;

- (IBAction)addException:(id)sender;
- (IBAction)removeException:(id)sender;
- (IBAction)removeAllExceptions:(id)sender;

@end

@interface ContentExceptionsWindowController(VisibleForTesting)
- (void)cancel:(id)sender;
- (BOOL)editingNewException;
@end
