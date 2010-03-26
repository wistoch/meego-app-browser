// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/url_drop_target.h"

@implementation AutocompleteTextField

@synthesize observer = observer_;

+ (Class)cellClass {
  return [AutocompleteTextFieldCell class];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)awakeFromNib {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
  currentToolTips_.reset([[NSMutableArray alloc] init]);
}

- (void)flagsChanged:(NSEvent*)theEvent {
  if (observer_) {
    const bool controlFlag = ([theEvent modifierFlags]&NSControlKeyMask) != 0;
    observer_->OnControlKeyChanged(controlFlag);
  }
}

- (AutocompleteTextFieldCell*)autocompleteTextFieldCell {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
  return static_cast<AutocompleteTextFieldCell*>([self cell]);
}

// Reroute events for the decoration area to the field editor.  This
// will cause the cursor to be moved as close to the edge where the
// event was seen as possible.
//
// The reason for this code's existence is subtle.  NSTextField
// implements text selection and editing in terms of a "field editor".
// This is an NSTextView which is installed as a subview of the
// control when the field becomes first responder.  When the field
// editor is installed, it will get -mouseDown: events and handle
// them, rather than the text field - EXCEPT for the event which
// caused the change in first responder, or events which fall in the
// decorations outside the field editor's area.  In that case, the
// default NSTextField code will setup the field editor all over
// again, which has the side effect of doing "select all" on the text.
// This effect can be observed with a normal NSTextField if you click
// in the narrow border area, and is only really a problem because in
// our case the focus ring surrounds decorations which look clickable.
//
// When the user first clicks on the field, after installing the field
// editor the default NSTextField code detects if the hit is in the
// field editor area, and if so sets the selection to {0,0} to clear
// the selection before forwarding the event to the field editor for
// processing (it will set the cursor position).  This also starts the
// click-drag selection machinery.
//
// This code does the same thing for cases where the click was in the
// decoration area.  This allows the user to click-drag starting from
// a decoration area and get the expected selection behaviour,
// likewise for multiple clicks in those areas.
- (void)mouseDown:(NSEvent*)theEvent {
  const NSPoint location =
      [self convertPoint:[theEvent locationInWindow] fromView:nil];
  const NSRect bounds([self bounds]);

  AutocompleteTextFieldCell* cell = [self autocompleteTextFieldCell];
  const NSRect textFrame([cell textFrameForFrame:bounds]);

  // A version of the textFrame which extends across the field's
  // entire width.

  const NSRect fullFrame(NSMakeRect(bounds.origin.x, textFrame.origin.y,
                                    bounds.size.width, textFrame.size.height));

  // If the mouse is in the editing area, or above or below where the
  // editing area would be if we didn't add decorations, forward to
  // NSTextField -mouseDown: because it does the right thing.  The
  // above/below test is needed because NSTextView treats mouse events
  // above/below as select-to-end-in-that-direction, which makes
  // things janky.
  BOOL flipped = [self isFlipped];
  if (NSMouseInRect(location, textFrame, flipped) ||
      !NSMouseInRect(location, fullFrame, flipped)) {
    [super mouseDown:theEvent];

    // After the event has been handled, if the current event is a
    // mouse up and no selection was created (the mouse didn't move),
    // select the entire field.
    // NOTE(shess): This does not interfere with single-clicking to
    // place caret after a selection is made.  An NSTextField only has
    // a selection when it has a field editor.  The field editor is an
    // NSText subview, which will receive the -mouseDown: in that
    // case, and this code will never fire.
    NSText* editor = [self currentEditor];
    if (editor) {
      NSEvent* currentEvent = [NSApp currentEvent];
      if ([currentEvent type] == NSLeftMouseUp &&
          ![editor selectedRange].length) {
        [editor selectAll:nil];
      }
    }

    return;
  }

  // If the user clicked on one of the icons (security icon, Page Actions, etc),
  // let the icon handle the click.
  const BOOL ctrlKey = ([theEvent modifierFlags] & NSControlKeyMask) != 0;
  for (AutocompleteTextFieldIcon* icon in [cell layedOutIcons:bounds]) {
    if (NSMouseInRect(location, [icon rect], flipped)) {
      if (ctrlKey) {
        // If the click was a Ctrl+Click, then imitate a right click and open
        // the contextual menu.
        NSText* editor = [self currentEditor];
        NSMenu* menu = [editor menuForEvent:theEvent];
        [NSMenu popUpContextMenu:menu withEvent:theEvent forView:editor];
      } else {
        [icon view]->OnMousePressed([icon rect]);
      }
      return;
    }
  }

  NSText* editor = [self currentEditor];

  // We should only be here if we accepted first-responder status and
  // have a field editor.  If one of these fires, it means some
  // assumptions are being broken.
  DCHECK(editor != nil);
  DCHECK([editor isDescendantOf:self]);

  // -becomeFirstResponder does a select-all, which we don't want
  // because it can lead to a dragged-text situation.  Clear the
  // selection (any valid empty selection will do).
  [editor setSelectedRange:NSMakeRange(0, 0)];

  // If the event is to the right of the editing area, scroll the
  // field editor to the end of the content so that the selection
  // doesn't initiate from somewhere in the middle of the text.
  if (location.x > NSMaxX(textFrame)) {
    [editor scrollRangeToVisible:NSMakeRange([[self stringValue] length], 0)];
  }

  [editor mouseDown:theEvent];
}

// Overridden to pass OnFrameChanged() notifications to |observer_|.
// Additionally, cursor and tooltip rects need to be updated.
- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (observer_) {
    observer_->OnFrameChanged();
  }
  [self updateCursorAndToolTipRects];
}

// Due to theming, parts of the field are transparent.
- (BOOL)isOpaque {
  return NO;
}

- (void)setAttributedStringValue:(NSAttributedString*)aString {
  NSTextView* editor = static_cast<NSTextView*>([self currentEditor]);
  if (!editor) {
    [super setAttributedStringValue:aString];
  } else {
    // -currentEditor is defined to return NSText*, make sure our
    // assumptions still hold, here.
    DCHECK([editor isKindOfClass:[NSTextView class]]);

    NSTextStorage* textStorage = [editor textStorage];
    DCHECK(textStorage);
    [textStorage setAttributedString:aString];
  }
}

- (NSUndoManager*)undoManagerForTextView:(NSTextView*)textView {
  if (!undoManager_.get())
    undoManager_.reset([[NSUndoManager alloc] init]);
  return undoManager_.get();
}

- (void)clearUndoChain {
  [undoManager_ removeAllActions];
}

// Show the I-beam cursor unless the mouse is over an image within the field
// (Page Actions or the security icon) in which case show the arrow cursor.
- (void)resetCursorRects {
  NSRect fieldBounds = [self bounds];
  [self addCursorRect:fieldBounds cursor:[NSCursor IBeamCursor]];

  AutocompleteTextFieldCell* cell = [self autocompleteTextFieldCell];
  for (AutocompleteTextFieldIcon* icon in [cell layedOutIcons:fieldBounds])
    [self addCursorRect:[icon rect] cursor:[NSCursor arrowCursor]];
}

- (void)updateCursorAndToolTipRects {
  // This will force |resetCursorRects| to be called, as it is not to be called
  // directly.
  [[self window] invalidateCursorRectsForView:self];

  // |removeAllToolTips| only removes those set on the current NSView, not any
  // subviews. Unless more tooltips are added to this view, this should suffice
  // in place of managing a set of NSToolTipTag objects.
  [self removeAllToolTips];
  [currentToolTips_ removeAllObjects];

  AutocompleteTextFieldCell* cell = [self autocompleteTextFieldCell];
  for (AutocompleteTextFieldIcon* icon in [cell layedOutIcons:[self bounds]]) {
    NSRect iconRect = [icon rect];
    NSString* tooltip = [icon view]->GetToolTip();
    if (!tooltip)
      continue;

    // -[NSView addToolTipRect:owner:userData] does _not_ retain its |owner:|.
    // Put the string in a collection so it can't be dealloced while in use.
    [currentToolTips_ addObject:tooltip];
    [self addToolTipRect:iconRect owner:tooltip userData:nil];
  }
}

// NOTE(shess): http://crbug.com/19116 describes a weird bug which
// happens when the user runs a Print panel on Leopard.  After that,
// spurious -controlTextDidBeginEditing notifications are sent when an
// NSTextField is firstResponder, even though -currentEditor on that
// field returns nil.  That notification caused significant problems
// in AutocompleteEditViewMac.  -textDidBeginEditing: was NOT being
// sent in those cases, so this approach doesn't have the problem.
- (void)textDidBeginEditing:(NSNotification*)aNotification {
  [super textDidBeginEditing:aNotification];
  if (observer_) {
    observer_->OnDidBeginEditing();
  }
}

- (void)textDidChange:(NSNotification *)aNotification {
  [super textDidChange:aNotification];
  if (observer_) {
    observer_->OnDidChange();
  }
}

- (void)textDidEndEditing:(NSNotification *)aNotification {
  [super textDidEndEditing:aNotification];
  if (observer_) {
    observer_->OnDidEndEditing();
  }
}

- (BOOL)textView:(NSTextView*)textView doCommandBySelector:(SEL)cmd {
  // TODO(shess): Review code for cases where we're fruitlessly attempting to
  // work in spite of not having an observer_.
  if (observer_ && observer_->OnDoCommandBySelector(cmd)) {
    return YES;
  }

  // If the escape key was pressed and no revert happened and we're in
  // fullscreen mode, make it resign key.
  if (cmd == @selector(cancelOperation:)) {
    BrowserWindowController* windowController =
        [BrowserWindowController browserWindowControllerForView:self];
    if ([windowController isFullscreen]) {
      [windowController focusTabContents];
      return YES;
    }
  }

  return NO;
}

- (void)windowDidResignKey:(NSNotification*)notification {
  DCHECK_EQ([self window], [notification object]);
  if (observer_) {
    observer_->OnDidResignKey();
  }
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  if ([self window]) {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc removeObserver:self
                  name:NSWindowDidResignKeyNotification
                object:[self window]];
  }
}

- (void)viewDidMoveToWindow {
  if ([self window]) {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self
           selector:@selector(windowDidResignKey:)
               name:NSWindowDidResignKeyNotification
             object:[self window]];
    // Only register for drops if not in a popup window. Lazily create the
    // drop handler when the type of window is known.
    BrowserWindowController* windowController =
        [BrowserWindowController browserWindowControllerForView:self];
    if ([windowController isNormalWindow])
      dropHandler_.reset([[URLDropTargetHandler alloc] initWithView:self]);
  }
}

// (Overridden from NSResponder)
- (BOOL)becomeFirstResponder {
  BOOL doAccept = [super becomeFirstResponder];
  if (doAccept) {
    [[BrowserWindowController browserWindowControllerForView:self]
        lockBarVisibilityForOwner:self withAnimation:YES delay:NO];
  }
  return doAccept;
}

// (Overridden from NSResponder)
- (BOOL)resignFirstResponder {
  BOOL doResign = [super resignFirstResponder];
  if (doResign) {
    [[BrowserWindowController browserWindowControllerForView:self]
        releaseBarVisibilityForOwner:self withAnimation:YES delay:YES];
  }
  return doResign;
}

// (URLDropTarget protocol)
- (id<URLDropTargetController>)urlDropController {
  BrowserWindowController* windowController =
      [BrowserWindowController browserWindowControllerForView:self];
  return [windowController toolbarController];
}

// (URLDropTarget protocol)
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  // Make ourself the first responder, which will select the text to indicate
  // that our contents would be replaced by a drop.
  // TODO(viettrungluu): crbug.com/30809 -- this is a hack since it steals focus
  // and doesn't return it.
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

@end
