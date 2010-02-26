// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_exceptions_window_controller.h"

#include "app/l10n_util_mac.h"
#include "app/table_model_observer.h"
#import "base/mac_util.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@interface ContentExceptionsWindowController(Private)
- (id)initWithType:(ContentSettingsType)settingsType
       settingsMap:(HostContentSettingsMap*)settingsMap;
- (void)adjustEditingButtons;
- (void)modelDidChange;
- (size_t)menuItemCount;
- (NSString*)titleForIndex:(size_t)index;
- (ContentSetting)settingForIndex:(size_t)index;
- (size_t)indexForSetting:(ContentSetting)setting;
@end

////////////////////////////////////////////////////////////////////////////////
// UrlFormatter

// A simple formatter that only accepts text that vaguely looks like a hostname.
@interface UrlFormatter : NSFormatter
@end

@implementation UrlFormatter
- (NSString*)stringForObjectValue:(id)object {
  if (![object isKindOfClass:[NSString class]])
    return nil;
  return object;
}

- (BOOL)getObjectValue:(id*)object
             forString:(NSString*)string
      errorDescription:(NSString**)error {
  if ([string length]) {
    url_canon::CanonHostInfo hostInfo;
    if (!net::CanonicalizeHost(
            base::SysNSStringToUTF8(string), &hostInfo).empty()) {
      *object = string;
      return YES;
    }
  }
  if (error)
    *error = @"Invalid hostname";
  return NO;
}

- (NSAttributedString*)attributedStringForObjectValue:(id)object
                                withDefaultAttributes:(NSDictionary*)attribs {
  return nil;
}
@end

////////////////////////////////////////////////////////////////////////////////
// UpdatingContentSettingsObserver

// A UpdatingContentSettingsObserver that tells a window controller to update
// its data on every notification.
class UpdatingContentSettingsObserver : public NotificationObserver {
 public:
  UpdatingContentSettingsObserver(ContentExceptionsWindowController* controller)
      : controller_(controller) {
    // One would think one could register a TableModelObserver to be notified of
    // changes to ContentExceptionsTableModel. One would be wrong: The table
    // model only sends out changes that are made through the model, not for
    // changes made directly to its backing HostContentSettings object (that
    // happens e.g. if the user uses the cookie confirmation dialog). Hence,
    // observe the CONTENT_SETTINGS_CHANGED notification directly.
    registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                   NotificationService::AllSources());
  }
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  NotificationRegistrar registrar_;
  ContentExceptionsWindowController* controller_;
};

void UpdatingContentSettingsObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  [controller_ modelDidChange];
}

////////////////////////////////////////////////////////////////////////////////
// Static functions

namespace  {

NSString* GetWindowTitle(ContentSettingsType settingsType) {
  switch (settingsType) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return l10n_util::GetNSStringWithFixup(IDS_COOKIE_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return l10n_util::GetNSStringWithFixup(IDS_IMAGES_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return l10n_util::GetNSStringWithFixup(IDS_JS_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return l10n_util::GetNSStringWithFixup(IDS_PLUGINS_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return l10n_util::GetNSStringWithFixup(IDS_POPUP_EXCEPTION_TITLE);
    default:
      NOTREACHED();
  }
  return @"";
}

// The settings shown in the combobox if showAsk_ is false;
const ContentSetting kNoAskSettings[] = { CONTENT_SETTING_ALLOW,
                                          CONTENT_SETTING_BLOCK };

// The settings shown in the combobox if showAsk_ is true;
const ContentSetting kAskSettings[] = { CONTENT_SETTING_ALLOW,
                                        CONTENT_SETTING_ASK,
                                        CONTENT_SETTING_BLOCK };

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ContentExceptionsWindowController implementation

static ContentExceptionsWindowController*
    g_exceptionWindows[CONTENT_SETTINGS_NUM_TYPES] = { nil };

@implementation ContentExceptionsWindowController

+ (id)showForType:(ContentSettingsType)settingsType
      settingsMap:(HostContentSettingsMap*)settingsMap {
  if (!g_exceptionWindows[settingsType]) {
    g_exceptionWindows[settingsType] =
        [[ContentExceptionsWindowController alloc] initWithType:settingsType
                                                    settingsMap:settingsMap];
  }
  [g_exceptionWindows[settingsType] showWindow:nil];
  return g_exceptionWindows[settingsType];
}

- (id)initWithType:(ContentSettingsType)settingsType
       settingsMap:(HostContentSettingsMap*)settingsMap {
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"ContentExceptionsWindow"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    settingsType_ = settingsType;
    settingsMap_ = settingsMap;
    model_.reset(new ContentExceptionsTableModel(settingsMap_, settingsType_));
    showAsk_ = settingsType_ == CONTENT_SETTINGS_TYPE_COOKIES;
    tableObserver_.reset(new UpdatingContentSettingsObserver(self));

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

  [[self window] setTitle:GetWindowTitle(settingsType_)];

  // Make sure the button fits its label, but keep it the same height as the
  // other two buttons.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:removeAllButton_];
  NSSize size = [removeAllButton_ frame].size;
  size.height = NSHeight([addButton_ frame]);
  [removeAllButton_ setFrameSize:size];

  [self adjustEditingButtons];

  // Initialize menu for the data cell in the "action" column.
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"exceptionMenu"]);
  for (size_t i = 0; i < [self menuItemCount]; ++i) {
    scoped_nsobject<NSMenuItem> allowItem([[NSMenuItem alloc]
        initWithTitle:[self titleForIndex:i] action:NULL keyEquivalent:@""]);
    [allowItem.get() setTag:[self settingForIndex:i]];
    [menu.get() addItem:allowItem.get()];
  }
  NSCell* menuCell =
      [[tableView_ tableColumnWithIdentifier:@"action"] dataCell];
  [menuCell setMenu:menu.get()];

  NSCell* hostCell = 
      [[tableView_ tableColumnWithIdentifier:@"hostname"] dataCell];
  [hostCell setFormatter:[[[UrlFormatter alloc] init] autorelease]];
}

- (void)windowWillClose:(NSNotification*)notification {
  g_exceptionWindows[settingsType_] = nil;
  [self autorelease];
}

// Let esc close the window.
- (void)cancel:(id)sender {
  if ([tableView_ currentEditor] != nil) {
    [tableView_ abortEditing];
    [[self window] makeFirstResponder:tableView_];  // Re-gain focus.

    if ([tableView_ selectedRow] == model_->RowCount()) {
      // Cancel addition of new row.
      [self removeException:self];
    }
  } else {
    [self close];
  }
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
      case NSCarriageReturnCharacter:
      case NSEnterCharacter:
        // Return enters rename mode.
        if ([[tableView_ selectedRowIndexes] count] == 1) {
          [tableView_ editColumn:0
                             row:[[tableView_ selectedRowIndexes] lastIndex]
                       withEvent:nil
                          select:YES];
        }
        return;
    }
  }
  [super keyDown:event];
}

- (IBAction)addException:(id)sender {
  newException_.reset(new HostContentSettingsMap::HostSettingPair);
  newException_->first = "example.com";
  newException_->second = CONTENT_SETTING_BLOCK;
  [tableView_ reloadData];
  [self adjustEditingButtons];

  int index = model_->RowCount();
  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:index]
          byExtendingSelection:NO];
  [tableView_ editColumn:0 row:index withEvent:nil select:YES];
}

- (IBAction)removeException:(id)sender {
  updatesEnabled_ = NO;
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  DCHECK_GT([selection count], 0U);
  NSUInteger index = [selection lastIndex];
  while (index != NSNotFound) {
    if (index == static_cast<NSUInteger>(model_->RowCount()))
      newException_.reset();
    else
      model_->RemoveException(index);
    index = [selection indexLessThanIndex:index];
  }
  updatesEnabled_ = YES;
  [self modelDidChange];
}

- (IBAction)removeAllExceptions:(id)sender {
  updatesEnabled_ = NO;
  newException_.reset();
  model_->RemoveAll();
  updatesEnabled_ = YES;
  [self modelDidChange];
}

// Table View Data Source -----------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table {
  return model_->RowCount() + (newException_.get() ? 1 : 0);
}

- (id)tableView:(NSTableView*)tv
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)row {
  const HostContentSettingsMap::HostSettingPair* entry;
  if (newException_.get() && row >= model_->RowCount())
    entry = newException_.get();
  else
    entry = &model_->entry_at(row);

  NSObject* result = nil;
  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"hostname"]) {
    result = base::SysUTF8ToNSString(entry->first);
  } else if ([identifier isEqualToString:@"action"]) {
    result = [NSNumber numberWithInt:[self indexForSetting:entry->second]];
  } else {
    NOTREACHED();
  }
  return result;
}

- (void) tableView:(NSTableView*)tv
    setObjectValue:(id)object
    forTableColumn:(NSTableColumn*)tableColumn
               row:(NSInteger)row {
  // Get model object.
  bool isNewRow = newException_.get() && row >= model_->RowCount();
  const HostContentSettingsMap::HostSettingPair* originalEntry =
      isNewRow ? newException_.get() : &model_->entry_at(row);
  HostContentSettingsMap::HostSettingPair entry = *originalEntry;  

  // Modify it.
  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"hostname"]) {
    entry.first = base::SysNSStringToUTF8(object);
  }
  if ([identifier isEqualToString:@"action"]) {
    int index = [object intValue];
    entry.second = [self settingForIndex:index];
  }

  // Commit modification, if any.
  // TODO(thakis): This apparently moves an edited row to the back of the list.
  // It's what windows and linux do, but it's kinda sucky. Fix.
  // http://crbug.com/36904
  if (entry != *originalEntry) {
    updatesEnabled_ = NO;
    if (!isNewRow) {
      model_->RemoveException(row);
    } else {
      newException_.reset();
      if (![identifier isEqualToString:@"hostname"]) {
        [tableView_ reloadData];
        [self adjustEditingButtons];
        return;  // Commit new rows only when the hostname has been set.
      }
    }      

    model_->AddException(entry.first, entry.second);
    updatesEnabled_ = YES;
    [self modelDidChange];

    // For now, at least re-select the edited element.    
    int newIndex = model_->IndexOfExceptionByHost(entry.first);
    DCHECK(newIndex != -1);
    [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:newIndex]
            byExtendingSelection:NO];
  }
}


// Table View Delegate --------------------------------------------------------

// When the selection in the table view changes, we need to adjust buttons.
- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  [self adjustEditingButtons];
}

// Private --------------------------------------------------------------------

// This method appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustEditingButtons {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  [removeButton_ setEnabled:([selection count] > 0)];
  [removeAllButton_ setEnabled:([tableView_ numberOfRows] > 0)];
}

- (void)modelDidChange {
  // Some calls on model_, e.g. RemoveException(), change something on the
  // backing content settings map object (which sends a notification) and then
  // change more stuff in model_. If model_ is deleted when the notification is
  // sent, this second access causes a segmentation violation. Hence, disable
  // resetting model_ while updates can be in progress.
  if (!updatesEnabled_)
    return;

  // Tthe model caches its data, meaning we need to recreate it on every change.
  model_.reset(new ContentExceptionsTableModel(settingsMap_, settingsType_));

  [tableView_ reloadData];
  [self adjustEditingButtons];
}

- (size_t)menuItemCount {
  return showAsk_ ? arraysize(kAskSettings) : arraysize(kNoAskSettings);
}

- (NSString*)titleForIndex:(size_t)index {
  switch ([self settingForIndex:index]) {
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetNSStringWithFixup(IDS_EXCEPTIONS_ALLOW_BUTTON);
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetNSStringWithFixup(IDS_EXCEPTIONS_BLOCK_BUTTON);
    case CONTENT_SETTING_ASK:
      return l10n_util::GetNSStringWithFixup(IDS_EXCEPTIONS_ASK_BUTTON);
    default:
      NOTREACHED();
  }
  return @"";
}

- (ContentSetting)settingForIndex:(size_t)index {
  return showAsk_ ? kAskSettings[index] : kNoAskSettings[index];
}

- (size_t)indexForSetting:(ContentSetting)setting {
  for (size_t i = 0; i < [self menuItemCount]; ++i) {
    if ([self settingForIndex:i] == setting)
      return i;
  }
  NOTREACHED();
  return 0;
}

@end
