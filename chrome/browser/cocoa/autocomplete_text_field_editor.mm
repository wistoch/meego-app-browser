// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"

#include "app/l10n_util_mac.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/browser/profile.h"

class Extension;

@interface AutocompleteTextFieldEditor(Private)
// Returns the default context menu to be displayed on a right mouse click.
- (NSMenu*)defaultMenuForEvent:(NSEvent*)event;
@end

@implementation AutocompleteTextFieldEditor

@synthesize profile = profile_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    dropHandler_.reset([[URLDropTargetHandler alloc] initWithView:self]);

    forbiddenCharacters_.reset([[NSCharacterSet controlCharacterSet] retain]);
  }
  return self;
}

- (void)copy:(id)sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer)
    observer->OnCopy();
}

- (void)cut:(id)sender {
  [self copy:sender];
  [self delete:nil];
}

// This class assumes that the delegate is an AutocompleteTextField.
// Enforce that assumption.
- (void)setDelegate:(id)anObject {
  DCHECK(anObject == nil ||
         [anObject isKindOfClass:[AutocompleteTextField class]]);
  [super setDelegate:anObject];
}

// Convenience method for retrieving the observer from the delegate.
- (AutocompleteTextFieldObserver*)observer {
  DCHECK([[self delegate] isKindOfClass:[AutocompleteTextField class]]);
  return [static_cast<AutocompleteTextField*>([self delegate]) observer];
}

- (void)paste:(id)sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer) {
    observer->OnPaste();
  }
}

- (void)pasteAndGo:sender {
  AutocompleteTextFieldObserver* observer = [self observer];
  DCHECK(observer);
  if (observer) {
    observer->OnPasteAndGo();
  }
}

// We have rich text, but it shouldn't be modified by the user, so
// don't update the font panel.  In theory, -setUsesFontPanel: should
// accomplish this, but that gets called frequently with YES when
// NSTextField and NSTextView synchronize their contents.  That is
// probably unavoidable because in most cases having rich text in the
// field you probably would expect it to update the font panel.
- (void)updateFontPanel {}

// No ruler bar, so don't update any of that state, either.
- (void)updateRuler {}

- (NSMenu*)menuForEvent:(NSEvent*)event {
  NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];

  // Was the right click within a Page Action? Show a different menu if so.
  AutocompleteTextField* field = (AutocompleteTextField*)[self delegate];
  NSRect bounds([field bounds]);
  AutocompleteTextFieldCell* cell = [field autocompleteTextFieldCell];
  BOOL flipped = [self isFlipped];

  for (AutocompleteTextFieldIcon* icon in [cell layedOutIcons:bounds]) {
    if (NSMouseInRect(location, [icon rect], flipped)) {
      NSMenu* menu = [icon view]->GetMenu();
      if (menu)
        return menu;
    }
  }

  // Otherwise, simply return the default menu for this instance.
  return [self defaultMenuForEvent:event];
}

- (NSMenu*)defaultMenuForEvent:(NSEvent*)event {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@"TITLE"] autorelease];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CUT)
                  action:@selector(cut:)
           keyEquivalent:@""];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_COPY)
                  action:@selector(copy:)
           keyEquivalent:@""];
  [menu addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_PASTE)
                  action:@selector(paste:)
           keyEquivalent:@""];

  // TODO(shess): If the control is not editable, should we show a
  // greyed-out "Paste and Go"?
  if ([self isEditable]) {
    // Paste and go/search.
    AutocompleteTextFieldObserver* observer = [self observer];
    DCHECK(observer);
    if (observer && observer->CanPasteAndGo()) {
      const int string_id = observer->GetPasteActionStringId();
      NSString* label = l10n_util::GetNSStringWithFixup(string_id);

      // TODO(rohitrao): If the clipboard is empty, should we show a
      // greyed-out "Paste and Go" or nothing at all?
      if ([label length]) {
        [menu addItemWithTitle:label
                        action:@selector(pasteAndGo:)
                 keyEquivalent:@""];
      }
    }

    NSString* label = l10n_util::GetNSStringWithFixup(IDS_EDIT_SEARCH_ENGINES);
    DCHECK([label length]);
    if ([label length]) {
      [menu addItem:[NSMenuItem separatorItem]];
      NSMenuItem* item = [menu addItemWithTitle:label
                                         action:@selector(commandDispatch:)
                                  keyEquivalent:@""];
      [item setTag:IDC_EDIT_SEARCH_ENGINES];
    }
  }

  return menu;
}

// (Overridden from NSResponder)
- (BOOL)becomeFirstResponder {
  BOOL doAccept = [super becomeFirstResponder];
  AutocompleteTextField* field = (AutocompleteTextField*)[self delegate];
  // Only lock visibility if we've been set up with a delegate (the text field).
  if (doAccept && field) {
    DCHECK([field isKindOfClass:[AutocompleteTextField class]]);
    // Give the text field ownership of the visibility lock. (The first
    // responder dance between the field and the field editor is a little
    // weird.)
    [[BrowserWindowController browserWindowControllerForView:field]
        lockBarVisibilityForOwner:field withAnimation:YES delay:NO];
  }
  return doAccept;
}

// (Overridden from NSResponder)
- (BOOL)resignFirstResponder {
  BOOL doResign = [super resignFirstResponder];
  AutocompleteTextField* field = (AutocompleteTextField*)[self delegate];
  // Only lock visibility if we've been set up with a delegate (the text field).
  if (doResign && field) {
    DCHECK([field isKindOfClass:[AutocompleteTextField class]]);
    // Give the text field ownership of the visibility lock.
    [[BrowserWindowController browserWindowControllerForView:field]
        releaseBarVisibilityForOwner:field withAnimation:YES delay:YES];
  }
  return doResign;
}

// (URLDropTarget protocol)
- (id<URLDropTargetController>)urlDropController {
  BrowserWindowController* windowController = [[self window] windowController];
  DCHECK([windowController isKindOfClass:[BrowserWindowController class]]);
  return [windowController toolbarController];
}

// (URLDropTarget protocol)
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  // Make ourself the first responder (even though we're presumably already the
  // first responder), which will select the text to indicate that our contents
  // would be replaced by a drop.
  [[self window] makeFirstResponder:self];
  return [dropHandler_ draggingEntered:sender];
}

// (URLDropTarget protocol)
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingUpdated:sender];
}

// (URLDropTarget protocol)
- (void)draggingExited:(id<NSDraggingInfo>)sender {
  return [dropHandler_ draggingExited:sender];
}

// (URLDropTarget protocol)
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [dropHandler_ performDragOperation:sender];
}

// Prevent control characters from being entered into the Omnibox.
// This is invoked for keyboard entry, not for pasting.
- (void)insertText:(id)aString {
  // This method is documented as received either |NSString| or
  // |NSAttributedString|.  The autocomplete code will restyle the
  // results in any case, so simplify by always using |NSString|.
  if ([aString isKindOfClass:[NSAttributedString class]])
    aString = [aString string];

  // Repeatedly remove control characters.  The loop will only ever
  // execute at allwhen the user enters control characters (using
  // Ctrl-Alt- or Ctrl-Q).  Making this generally efficient would
  // probably be a loss, since the input always seems to be a single
  // character.
  NSRange range = [aString rangeOfCharacterFromSet:forbiddenCharacters_];
  while (range.location != NSNotFound) {
    aString = [aString stringByReplacingCharactersInRange:range withString:@""];
    range = [aString rangeOfCharacterFromSet:forbiddenCharacters_];
  }
  DCHECK_EQ(range.length, 0U);

  // TODO(shess): Beep on empty?  I hate beeps, though.
  // NOTE: If |aString| is empty, this intentionally replaces the
  // selection with empty.  This seems consistent with the case where
  // the input contained a mixture of characters and the string ended
  // up not empty.
  [super insertText:aString];
}

@end
