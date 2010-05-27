// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/geolocation_exceptions_window_controller.h"

#include <set>

#include "app/l10n_util_mac.h"
#include "app/table_model_observer.h"
#import "base/mac_util.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@interface GeolocationExceptionsWindowController (Private)
- (id)initWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap;
- (void)selectedRows:(GeolocationExceptionsTableModel::Rows*)rows;
- (void)adjustEditingButtons;
- (void)modelDidChange;
@end

// Observer for the geolocation table model.
class GeolocationObserverBridge : public TableModelObserver {
 public:
  GeolocationObserverBridge(GeolocationExceptionsWindowController* controller)
      : controller_(controller) {}
  virtual ~GeolocationObserverBridge() {}

  virtual void OnModelChanged() {
    [controller_ modelDidChange];
  }
  virtual void OnItemsChanged(int start, int length) {
    [controller_ modelDidChange];
  }
  virtual void OnItemsAdded(int start, int length) {
    [controller_ modelDidChange];
  }
  virtual void OnItemsRemoved(int start, int length) {
    [controller_ modelDidChange];
  }

 private:
  GeolocationExceptionsWindowController* controller_;  // weak
};

namespace  {

const CGFloat kButtonBarHeight = 35.0;

GeolocationExceptionsWindowController* g_exceptionWindow = nil;

}  // namespace

@implementation GeolocationExceptionsWindowController

+ (id)controllerWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap {
  if (!g_exceptionWindow) {
    g_exceptionWindow = [[GeolocationExceptionsWindowController alloc]
        initWithSettingsMap:settingsMap];
  }
  return g_exceptionWindow;
}

- (id)initWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap {
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"GeolocationExceptionsWindow"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    settingsMap_ = settingsMap;
    model_.reset(new GeolocationExceptionsTableModel(settingsMap_));
    tableObserver_.reset(new GeolocationObserverBridge(self));
    model_->SetObserver(tableObserver_.get());

    // TODO(thakis): autoremember window rect.
    // TODO(thakis): sorting support.
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
  DCHECK(tableView_);
  DCHECK_EQ(self, [tableView_ dataSource]);
  DCHECK_EQ(self, [tableView_ delegate]);

  CGFloat minWidth = [[removeButton_ superview] bounds].size.width +
                     [[doneButton_ superview] bounds].size.width;
  [[self window] setMinSize:NSMakeSize(minWidth,
                                       [[self window] minSize].height)];

  [self adjustEditingButtons];
}

- (void)setMinWidth:(CGFloat)minWidth {
  NSWindow* window = [self window];
  [window setMinSize:NSMakeSize(minWidth, [window minSize].height)];
  if ([window frame].size.width < minWidth) {
    NSRect frame = [window frame];
    frame.size.width = minWidth;
    [window setFrame:frame display:NO];
  }
}

- (void)windowWillClose:(NSNotification*)notification {
  // Without this, some of the unit tests fail on 10.6:
  [tableView_ setDataSource:nil];

  g_exceptionWindow = nil;
  [self autorelease];
}

// Let esc close the window.
- (void)cancel:(id)sender {
  [self closeSheet:self];
}

- (void)keyDown:(NSEvent*)event {
  NSString* chars = [event charactersIgnoringModifiers];
  if ([chars length] == 1) {
    switch ([chars characterAtIndex:0]) {
      case NSDeleteCharacter:
      case NSDeleteFunctionKey:
        // Delete deletes.
        if ([[tableView_ selectedRowIndexes] count] > 0)
          [self removeException:self];
        return;
    }
  }
  [super keyDown:event];
}

- (void)attachSheetTo:(NSWindow*)window {
  [NSApp beginSheet:[self window]
     modalForWindow:window
      modalDelegate:self
     didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
        contextInfo:nil];
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)context {
  [sheet close];
  [sheet orderOut:self];
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}

- (IBAction)removeException:(id)sender {
  GeolocationExceptionsTableModel::Rows rows;
  [self selectedRows:&rows];
  model_->RemoveExceptions(rows);
}

- (IBAction)removeAllExceptions:(id)sender {
  model_->RemoveAll();
}

// Table View Data Source -----------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table {
  return model_->RowCount();
}

- (id)tableView:(NSTableView*)tv
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)row {
  NSObject* result = nil;
  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"hostname"]) {
    std::wstring host = model_->GetText(row, IDS_EXCEPTIONS_HOSTNAME_HEADER);
    result = base::SysWideToNSString(host);
  } else if ([identifier isEqualToString:@"action"]) {
    std::wstring action = model_->GetText(row, IDS_EXCEPTIONS_ACTION_HEADER);
    result = base::SysWideToNSString(action);
  } else {
    NOTREACHED();
  }
  return result;
}

// Table View Delegate --------------------------------------------------------

// When the selection in the table view changes, we need to adjust buttons.
- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  [self adjustEditingButtons];
}

// Private --------------------------------------------------------------------

// Returns the selected rows.
- (void)selectedRows:(GeolocationExceptionsTableModel::Rows*)rows {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  for (NSUInteger index = [selection lastIndex]; index != NSNotFound;
       index = [selection indexLessThanIndex:index])
    rows->insert(index);
}

// This method appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustEditingButtons {
  GeolocationExceptionsTableModel::Rows rows;
  [self selectedRows:&rows];
  [removeButton_ setEnabled:model_->CanRemoveExceptions(rows)];
  [removeAllButton_ setEnabled:([tableView_ numberOfRows] > 0)];
}

- (void)modelDidChange {
  [tableView_ reloadData];
  [self adjustEditingButtons];
}

@end
