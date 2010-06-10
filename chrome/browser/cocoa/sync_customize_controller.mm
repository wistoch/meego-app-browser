// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/sync_customize_controller.h"
#import "chrome/browser/cocoa/sync_customize_controller_cppsafe.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#import "base/mac_util.h"
#import "base/stl_util-inl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/model_type.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"


@implementation SyncCustomizeController

@synthesize bookmarksRegistered = bookmarksRegistered_;
@synthesize preferencesRegistered = preferencesRegistered_;
@synthesize autofillRegistered = autofillRegistered_;
@synthesize themesRegistered = themesRegistered_;
@synthesize extensionsRegistered = extensionsRegistered_;

// If you add another ***Preferred variable, you must update okEnabled and
// keyPathsForValuesAffectingOkEnabled below.
@synthesize bookmarksPreferred = bookmarksPreferred_;
@synthesize preferencesPreferred = preferencesPreferred_;
@synthesize autofillPreferred = autofillPreferred_;
@synthesize themesPreferred = themesPreferred_;
@synthesize extensionsPreferred = extensionsPreferred_;

// The OK button should be clickable if and only if there's at least one
// datatype chosen to sync.
- (BOOL)okEnabled {
  return bookmarksPreferred_ || preferencesPreferred_ || autofillPreferred_ ||
      themesPreferred_ || extensionsPreferred_;
}

// Naming convention; makes okEnabled get updated whenever any of the below
// "Preferred" variables are updated.
+ (NSSet*)keyPathsForValuesAffectingOkEnabled {
  return [NSSet setWithObjects:@"bookmarksPreferred", @"preferencesPreferred",
                @"autofillPreferred", @"themesPreferred",
                @"extensionsPreferred", nil];
}

- (id)initWithProfileSyncService:(ProfileSyncService*)syncService {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"SyncCustomize"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    CHECK(syncService);
    syncService_ = syncService;
  }
  return self;
}

// Called when the sheet containing our window is dismissed.
- (void)endSheet:(NSWindow*)sheet
      returnCode:(NSInteger)returnCode
     contextInfo:(void*)context {
  NSWindow* parentWindow = static_cast<NSWindow*>(context);
  [sheet close];
  [sheet orderOut:parentWindow];
}

- (void)runAsModalSheet:(NSWindow*)parentWindow {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow
      modalDelegate:self
     didEndSelector:@selector(endSheet:returnCode:contextInfo:)
        contextInfo:parentWindow];
}

- (void)awakeFromNib {
  DCHECK([self window]);
  [[self window] setDelegate:self];

  CGFloat viewHeightChange =
      [GTMUILocalizerAndLayoutTweaker
       sizeToFitFixedWidthTextField:customizeSyncDescriptionTextField_];
  if (viewHeightChange > 0) {
    // Resize the window.  No need to move the controls as they're all
    // bottom-anchored.
    NSSize viewSizeChange = NSMakeSize(0, viewHeightChange);
    NSSize windowSizeChange =
        [customizeSyncDescriptionTextField_ convertSize:viewSizeChange
                                                 toView:nil];
    CGFloat windowHeightChange = windowSizeChange.height;
    NSRect frame = [[self window] frame];
    frame.origin.y -= windowHeightChange;
    frame.size.height += windowHeightChange;
    [[self window] setFrame:frame display:NO];
  }

  syncable::ModelTypeSet registered_types;
  syncService_->GetRegisteredDataTypes(&registered_types);
  const syncable::ModelType expected_types[] = {
    syncable::BOOKMARKS,
    syncable::PREFERENCES,
    syncable::AUTOFILL,
    syncable::THEMES,
    syncable::EXTENSIONS,
  };
  DCHECK(std::includes(expected_types,
                       expected_types + arraysize(expected_types),
                       registered_types.begin(), registered_types.end()));
  DCHECK(ContainsKey(registered_types, syncable::BOOKMARKS));

  [self setBookmarksRegistered:ContainsKey(registered_types,
                                           syncable::BOOKMARKS)];
  [self setPreferencesRegistered:ContainsKey(registered_types,
                                             syncable::PREFERENCES)];
  [self setAutofillRegistered:ContainsKey(registered_types,
                                          syncable::AUTOFILL)];
  [self setThemesRegistered:ContainsKey(registered_types,
                                        syncable::THEMES)];
  [self setExtensionsRegistered:ContainsKey(registered_types,
                                            syncable::EXTENSIONS)];

  syncable::ModelTypeSet preferred_types;
  syncService_->GetPreferredDataTypes(&preferred_types);
  DCHECK(std::includes(registered_types.begin(), registered_types.end(),
                       preferred_types.begin(), preferred_types.end()));

  [self setBookmarksPreferred:ContainsKey(preferred_types,
                                          syncable::BOOKMARKS)];
  [self setPreferencesPreferred:ContainsKey(preferred_types,
                                            syncable::PREFERENCES)];
  [self setAutofillPreferred:ContainsKey(preferred_types,
                                         syncable::AUTOFILL)];
  [self setThemesPreferred:ContainsKey(preferred_types,
                                       syncable::THEMES)];
  [self setExtensionsPreferred:ContainsKey(preferred_types,
                                           syncable::EXTENSIONS)];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

// Dismiss the sheet containing our window.
- (void)endSheet {
  [NSApp endSheet:[self window]];
}

- (IBAction)endSheetWithCancel:(id)sender {
  [self endSheet];
}

// Commit the changes made by the user to the ProfileSyncService.
- (void)changePreferredDataTypes {
  syncable::ModelTypeSet preferred_types;
  if ([self bookmarksPreferred]) {
    preferred_types.insert(syncable::BOOKMARKS);
  }
  if ([self preferencesPreferred]) {
    preferred_types.insert(syncable::PREFERENCES);
  }
  if ([self autofillPreferred]) {
    preferred_types.insert(syncable::AUTOFILL);
  }
  if ([self themesPreferred]) {
    preferred_types.insert(syncable::THEMES);
  }
  if ([self extensionsPreferred]) {
    preferred_types.insert(syncable::EXTENSIONS);
  }
  syncService_->ChangePreferredDataTypes(preferred_types);
  [self endSheet];
}

- (IBAction)endSheetWithOK:(id)sender {
  [self changePreferredDataTypes];
  [self endSheet];
}

@end

void ShowSyncCustomizeDialog(gfx::NativeWindow parent_window,
                             ProfileSyncService* sync_service) {
  // syncCustomizeController releases itself on close.
  SyncCustomizeController* syncCustomizeController =
      [[SyncCustomizeController alloc]
        initWithProfileSyncService:sync_service];
  [syncCustomizeController runAsModalSheet:parent_window];
}
