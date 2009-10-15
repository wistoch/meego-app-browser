// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"
#import "chrome/browser/cocoa/autocomplete_text_field_unittest_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using ::testing::InSequence;

// OCMock wants to mock a concrete class or protocol.  This should
// provide a correct protocol for newer versions of the SDK, while
// providing something mockable for older versions.

@protocol MockTextEditingDelegate<NSControlTextEditingDelegate>
- (void)controlTextDidBeginEditing:(NSNotification*)aNotification;
- (BOOL)control:(NSControl*)control textShouldEndEditing:(NSText*)fieldEditor;
@end

namespace {

// Mock up an incrementing event number.
NSUInteger eventNumber = 0;

// Create an event of the indicated |type| at |point| within |view|.
// TODO(shess): Would be nice to have a MockApplication which provided
// nifty accessors to create these things and inject them.  It could
// even provide functions for "Click and drag mouse from point A to
// point B".
NSEvent* Event(NSView* view, const NSPoint point, const NSEventType type,
               const NSUInteger clickCount) {
  NSWindow* window([view window]);
  const NSPoint locationInWindow([view convertPoint:point toView:nil]);
  const NSPoint location([window convertBaseToScreen:locationInWindow]);
  return [NSEvent mouseEventWithType:type
                            location:location
                       modifierFlags:0
                           timestamp:0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:eventNumber++
                          clickCount:clickCount
                            pressure:0.0];
}
NSEvent* Event(NSView* view, const NSPoint point, const NSEventType type) {
  return Event(view, point, type, 1);
}

// Width of the field so that we don't have to ask |field_| for it all
// the time.
static const CGFloat kWidth(300.0);

class AutocompleteTextFieldTest : public PlatformTest {
 public:
  AutocompleteTextFieldTest() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    NSRect frame = NSMakeRect(0, 0, kWidth, 30);
    field_.reset([[AutocompleteTextField alloc] initWithFrame:frame]);
    [field_ setStringValue:@"Test test"];
    [field_ setObserver:&field_observer_];
    [cocoa_helper_.contentView() addSubview:field_.get()];

    window_delegate_.reset(
        [[AutocompleteTextFieldWindowTestDelegate alloc] init]);
    [cocoa_helper_.window() setDelegate:window_delegate_.get()];
  }

  // The removeFromSuperview call is needed to prevent crashes in
  // later tests.
  // TODO(shess): -removeromSuperview should not be necessary.  Fix
  // it.  Also in autocomplete_text_field_editor_unittest.mm.
  ~AutocompleteTextFieldTest() {
    [cocoa_helper_.window() setDelegate:nil];
    [field_ removeFromSuperview];
  }

  NSEvent* KeyDownEventWithFlags(NSUInteger flags) {
    return [NSEvent keyEventWithType:NSKeyDown
                            location:NSZeroPoint
                       modifierFlags:flags
                           timestamp:0.0
                        windowNumber:[cocoa_helper_.window() windowNumber]
                             context:nil
                          characters:@"a"
         charactersIgnoringModifiers:@"a"
                           isARepeat:NO
                             keyCode:'a'];
  }

  // Helper to return the field-editor frame being used w/in |field_|.
  NSRect EditorFrame() {
    EXPECT_TRUE([field_.get() currentEditor]);
    EXPECT_EQ([[field_.get() subviews] count], 1U);
    if ([[field_.get() subviews] count] > 0) {
      return [[[field_.get() subviews] objectAtIndex:0] frame];
    } else {
      // Return something which won't work so the caller can soldier
      // on.
      return NSZeroRect;
    }
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<AutocompleteTextField> field_;
  MockAutocompleteTextFieldObserver field_observer_;
  scoped_nsobject<AutocompleteTextFieldWindowTestDelegate> window_delegate_;
};

// Test that we have the right cell class.
TEST_F(AutocompleteTextFieldTest, CellClass) {
  EXPECT_TRUE([[field_ cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
}

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(AutocompleteTextFieldTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [field_ superview]);
  [field_.get() removeFromSuperview];
  EXPECT_FALSE([field_ superview]);
}

// Test that we get the same cell from -cell and
// -autocompleteTextFieldCell.
TEST_F(AutocompleteTextFieldTest, Cell) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  EXPECT_EQ(cell, [field_ cell]);
  EXPECT_TRUE(cell != nil);
}

// Test that becoming first responder sets things up correctly.
TEST_F(AutocompleteTextFieldTest, FirstResponder) {
  EXPECT_EQ(nil, [field_ currentEditor]);
  EXPECT_EQ([[field_ subviews] count], 0U);
  cocoa_helper_.makeFirstResponder(field_);
  EXPECT_FALSE(nil == [field_ currentEditor]);
  EXPECT_EQ([[field_ subviews] count], 1U);
  EXPECT_TRUE([[field_ currentEditor] isDescendantOf:field_.get()]);

  // Check that the window delegate is providing the right editor.
  Class c = [AutocompleteTextFieldEditor class];
  EXPECT_TRUE([[field_ currentEditor] isKindOfClass:c]);
}

TEST_F(AutocompleteTextFieldTest, AvailableDecorationWidth) {
  // A fudge factor to account for how much space the border takes up.
  // The test shouldn't be too dependent on the field's internals, but
  // it also shouldn't let deranged cases fall through the cracks
  // (like nothing available with no text, or everything available
  // with some text).
  const CGFloat kBorderWidth = 20.0;

  // With no contents, almost the entire width is available for
  // decorations.
  [field_ setStringValue:@""];
  CGFloat availableWidth = [field_ availableDecorationWidth];
  EXPECT_LE(availableWidth, kWidth);
  EXPECT_GT(availableWidth, kWidth - kBorderWidth);

  // With minor contents, most of the remaining width is available for
  // decorations.
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:[field_ font]
                                  forKey:NSFontAttributeName];
  NSString* string = @"Hello world";
  const NSSize size([string sizeWithAttributes:attributes]);
  [field_ setStringValue:string];
  availableWidth = [field_ availableDecorationWidth];
  EXPECT_LE(availableWidth, kWidth - size.width);
  EXPECT_GT(availableWidth, kWidth - size.width - kBorderWidth);

  // With huge contents, nothing at all is left for decorations.
  string = @"A long string which is surely wider than field_ can hold.";
  [field_ setStringValue:string];
  availableWidth = [field_ availableDecorationWidth];
  EXPECT_LT(availableWidth, 0.0);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldTest, Display) {
  [field_ display];

  // Test focussed drawing.
  cocoa_helper_.makeFirstResponder(field_);
  [field_ display];
  cocoa_helper_.clearFirstResponder();

  // Test display of various cell configurations.
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  [field_ display];

  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"prefix" image:image suffix:@"suffix"
              availableWidth:kWidth];
  [field_ display];

  [cell setKeywordString:@"Search Engine:"
           partialString:@"Search Eng:"
          availableWidth:kWidth];
  [field_ display];

  [cell clearKeywordAndHint];
  [field_ display];
}

TEST_F(AutocompleteTextFieldTest, FlagsChanged) {
  InSequence dummy;  // Call mock in exactly the order specified.

  // Test without Control key down, but some other modifier down.
  EXPECT_CALL(field_observer_, OnControlKeyChanged(false));
  [field_ flagsChanged:KeyDownEventWithFlags(NSShiftKeyMask)];

  // Test with Control key down.
  EXPECT_CALL(field_observer_, OnControlKeyChanged(true));
  [field_ flagsChanged:KeyDownEventWithFlags(NSControlKeyMask)];
}

// This test is here rather than in the editor's tests because the
// field catches -flagsChanged: because it's on the responder chain,
// the field editor doesn't implement it.
TEST_F(AutocompleteTextFieldTest, FieldEditorFlagsChanged) {
  cocoa_helper_.makeFirstResponder(field_);
  NSResponder* firstResponder = [[field_ window] firstResponder];
  EXPECT_EQ(firstResponder, [field_ currentEditor]);

  InSequence dummy;  // Call mock in exactly the order specified.

  // Test without Control key down, but some other modifier down.
  EXPECT_CALL(field_observer_, OnControlKeyChanged(false));
  [firstResponder flagsChanged:KeyDownEventWithFlags(NSShiftKeyMask)];

  // Test with Control key down.
  EXPECT_CALL(field_observer_, OnControlKeyChanged(true));
  [firstResponder flagsChanged:KeyDownEventWithFlags(NSControlKeyMask)];
}

// Frame size changes are propagated to |observer_|.
TEST_F(AutocompleteTextFieldTest, FrameChanged) {
  EXPECT_CALL(field_observer_, OnFrameChanged());
  NSRect frame = [field_ frame];
  frame.size.width += 10.0;
  [field_ setFrame:frame];
}

// Test that the field editor gets the same bounds when focus is
// delivered by the standard focusing machinery, or by
// -resetFieldEditorFrameIfNeeded.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorBase) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Capture the editor frame resulting from the standard focus
  // machinery.
  cocoa_helper_.makeFirstResponder(field_);
  const NSRect baseEditorFrame(EditorFrame());

  // Setting a hint should result in a strictly smaller editor frame.
  [cell setSearchHintString:@"search hint" availableWidth:kWidth];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_FALSE(NSEqualRects(baseEditorFrame, EditorFrame()));
  EXPECT_TRUE(NSContainsRect(baseEditorFrame, EditorFrame()));

  // Clearing hint string and using -resetFieldEditorFrameIfNeeded
  // should result in the same frame as the standard focus machinery.
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_TRUE(NSEqualRects(baseEditorFrame, EditorFrame()));
}

// Test that the field editor gets the same bounds when focus is
// delivered by the standard focusing machinery, or by
// -resetFieldEditorFrameIfNeeded.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorSearchHint) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  const NSString* kHintString(@"Type to search");

  // Capture the editor frame resulting from the standard focus
  // machinery.
  [cell setSearchHintString:kHintString availableWidth:kWidth];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  cocoa_helper_.makeFirstResponder(field_);
  const NSRect baseEditorFrame(EditorFrame());

  // Clearing the hint should result in a strictly larger editor
  // frame.
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_FALSE(NSEqualRects(baseEditorFrame, EditorFrame()));
  EXPECT_TRUE(NSContainsRect(EditorFrame(), baseEditorFrame));

  // Setting the same hint string and using
  // -resetFieldEditorFrameIfNeeded should result in the same frame as
  // the standard focus machinery.
  [cell setSearchHintString:kHintString availableWidth:kWidth];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_TRUE(NSEqualRects(baseEditorFrame, EditorFrame()));
}

// Test that the field editor gets the same bounds when focus is
// delivered by the standard focusing machinery, or by
// -resetFieldEditorFrameIfNeeded.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorKeywordHint) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  const NSString* kFullString(@"Search Engine:");
  const NSString* kPartialString(@"Search Eng:");

  // Capture the editor frame resulting from the standard focus
  // machinery.
  [cell setKeywordString:kFullString
           partialString:kPartialString
          availableWidth:kWidth];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  cocoa_helper_.makeFirstResponder(field_);
  const NSRect baseEditorFrame(EditorFrame());

  // Clearing the hint should result in a strictly larger editor
  // frame.
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_FALSE(NSEqualRects(baseEditorFrame, EditorFrame()));
  EXPECT_TRUE(NSContainsRect(EditorFrame(), baseEditorFrame));

  // Setting the same hint string and using
  // -resetFieldEditorFrameIfNeeded should result in the same frame as
  // the standard focus machinery.
  [cell setKeywordString:kFullString
           partialString:kPartialString
          availableWidth:kWidth];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_TRUE(NSEqualRects(baseEditorFrame, EditorFrame()));
}

// Test that resetting the field editor bounds does not cause untoward
// messages to the field's delegate.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorBlocksEndEditing) {
  // First, test that -makeFirstResponder: sends
  // -controlTextDidBeginEditing: and -control:textShouldEndEditing at
  // the expected times.
  {
    id mockDelegate =
        [OCMockObject mockForProtocol:@protocol(MockTextEditingDelegate)];

    [field_ setDelegate:mockDelegate];

    // Becoming first responder doesn't begin editing.
    cocoa_helper_.makeFirstResponder(field_);
    NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
    EXPECT_TRUE(nil != editor);
    [mockDelegate verify];

    // This should begin editing.
    [[mockDelegate expect] controlTextDidBeginEditing:OCMOCK_ANY];
    [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
    [mockDelegate verify];

    // Changing first responder ends editing.
    BOOL yes = YES;
    [[[mockDelegate expect] andReturnValue:OCMOCK_VALUE(yes)]
      control:OCMOCK_ANY textShouldEndEditing:OCMOCK_ANY];
    cocoa_helper_.makeFirstResponder(field_);
    [mockDelegate verify];

    [field_ setDelegate:nil];
  }

  // Test that -resetFieldEditorFrameIfNeeded manages to rearrange the
  // editor without ending editing.
  {
    id mockDelegate =
        [OCMockObject mockForProtocol:@protocol(MockTextEditingDelegate)];

    [field_ setDelegate:mockDelegate];

    // Start editing.
    [[mockDelegate expect] controlTextDidBeginEditing:OCMOCK_ANY];
    NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
    [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
    [mockDelegate verify];

    // No more messages to mockDelegate.
    AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
    EXPECT_FALSE([cell fieldEditorNeedsReset]);
    [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
    EXPECT_TRUE([cell fieldEditorNeedsReset]);
    [field_ resetFieldEditorFrameIfNeeded];
    [mockDelegate verify];

    [field_ setDelegate:nil];
  }
}

// Clicking in the search hint should put the caret in the rightmost
// position.
TEST_F(AutocompleteTextFieldTest, ClickSearchHintPutsCaretRightmost) {
  // Set the decoration before becoming responder.
  EXPECT_FALSE([field_ currentEditor]);
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];

  // Can't rely on the window machinery to make us first responder,
  // here.
  cocoa_helper_.makeFirstResponder(field_);
  EXPECT_TRUE([field_ currentEditor]);

  const NSPoint point(NSMakePoint(300.0 - 20.0, 5.0));
  NSEvent* downEvent(Event(field_, point, NSLeftMouseDown));
  NSEvent* upEvent(Event(field_, point, NSLeftMouseUp));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  EXPECT_EQ(selectedRange.location, [[field_ stringValue] length]);
  EXPECT_EQ(selectedRange.length, 0U);
}

// Clicking in the keyword-search should put the caret in the
// leftmost position.
TEST_F(AutocompleteTextFieldTest, ClickKeywordPutsCaretLeftmost) {
  // Set the decoration before becoming responder.
  EXPECT_FALSE([field_ currentEditor]);
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setKeywordString:@"Search Engine:"
           partialString:@"Search:"
          availableWidth:kWidth];

  // Can't rely on the window machinery to make us first responder,
  // here.
  cocoa_helper_.makeFirstResponder(field_);
  EXPECT_TRUE([field_ currentEditor]);

  const NSPoint point(NSMakePoint(20.0, 5.0));
  NSEvent* downEvent(Event(field_, point, NSLeftMouseDown));
  NSEvent* upEvent(Event(field_, point, NSLeftMouseUp));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  EXPECT_EQ(selectedRange.location, 0U);
  EXPECT_EQ(selectedRange.length, 0U);
}

// Clicks not in the text area or the cell's decorations fall through
// to the editor.
TEST_F(AutocompleteTextFieldTest, ClickBorderSelectsAll) {
  // Can't rely on the window machinery to make us first responder,
  // here.
  cocoa_helper_.makeFirstResponder(field_);
  EXPECT_TRUE([field_ currentEditor]);

  const NSPoint point(NSMakePoint(20.0, 1.0));
  NSEvent* downEvent(Event(field_, point, NSLeftMouseDown));
  NSEvent* upEvent(Event(field_, point, NSLeftMouseUp));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];

  // Clicking in the narrow border area around a Cocoa NSTextField
  // does a select-all.  Regardless of whether this is a good call, it
  // works as a test that things get passed down to the editor.
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  EXPECT_EQ(selectedRange.location, 0U);
  EXPECT_EQ(selectedRange.length, [[field_ stringValue] length]);
}

// Single-click with no drag should setup a field editor and
// select all.
TEST_F(AutocompleteTextFieldTest, ClickSelectsAll) {
  EXPECT_FALSE([field_ currentEditor]);

  const NSPoint point(NSMakePoint(20.0, 5.0));
  NSEvent* downEvent(Event(field_, point, NSLeftMouseDown));
  NSEvent* upEvent(Event(field_, point, NSLeftMouseUp));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  EXPECT_TRUE([field_ currentEditor]);
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  EXPECT_EQ(selectedRange.location, 0U);
  EXPECT_EQ(selectedRange.length, [[field_ stringValue] length]);
}

// Click-drag selects text, not select all.
TEST_F(AutocompleteTextFieldTest, ClickDragSelectsText) {
  EXPECT_FALSE([field_ currentEditor]);

  NSEvent* downEvent(Event(field_, NSMakePoint(20.0, 5.0), NSLeftMouseDown));
  NSEvent* upEvent(Event(field_, NSMakePoint(0.0, 5.0), NSLeftMouseUp));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  EXPECT_TRUE([field_ currentEditor]);

  // Expect this to have selected a prefix of the content.  Mostly
  // just don't want the select-all behavior.
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  EXPECT_EQ(selectedRange.location, 0U);
  EXPECT_LT(selectedRange.length, [[field_ stringValue] length]);
}

// TODO(shess): Test that click/pause/click allows cursor placement.
// In this case the first click goes to the field, but the second
// click goes to the field editor, so the current testing pattern
// can't work.  What really needs to happen is to push through the
// NSWindow event machinery so that we can say "two independent clicks
// at the same location have the right effect".  Once that is done, it
// might make sense to revise the other tests to use the same
// machinery.

// Double-click selects word, not select all.
TEST_F(AutocompleteTextFieldTest, DoubleClickSelectsWord) {
  EXPECT_FALSE([field_ currentEditor]);

  const NSPoint point(NSMakePoint(20.0, 5.0));
  NSEvent* downEvent(Event(field_, point, NSLeftMouseDown, 1));
  NSEvent* upEvent(Event(field_, point, NSLeftMouseUp, 1));
  NSEvent* downEvent2(Event(field_, point, NSLeftMouseDown, 2));
  NSEvent* upEvent2(Event(field_, point, NSLeftMouseUp, 2));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  [NSApp postEvent:upEvent2 atStart:YES];
  [field_ mouseDown:downEvent2];
  EXPECT_TRUE([field_ currentEditor]);

  // Selected the first word.
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  const NSRange spaceRange([[field_ stringValue] rangeOfString:@" "]);
  EXPECT_GT(spaceRange.location, 0U);
  EXPECT_LT(spaceRange.length, [[field_ stringValue] length]);
  EXPECT_EQ(selectedRange.location, 0U);
  EXPECT_EQ(selectedRange.length, spaceRange.location);
}

TEST_F(AutocompleteTextFieldTest, TripleClickSelectsAll) {
  EXPECT_FALSE([field_ currentEditor]);

  const NSPoint point(NSMakePoint(20.0, 5.0));
  NSEvent* downEvent(Event(field_, point, NSLeftMouseDown, 1));
  NSEvent* upEvent(Event(field_, point, NSLeftMouseUp, 1));
  NSEvent* downEvent2(Event(field_, point, NSLeftMouseDown, 2));
  NSEvent* upEvent2(Event(field_, point, NSLeftMouseUp, 2));
  NSEvent* downEvent3(Event(field_, point, NSLeftMouseDown, 3));
  NSEvent* upEvent3(Event(field_, point, NSLeftMouseUp, 3));
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  [NSApp postEvent:upEvent2 atStart:YES];
  [field_ mouseDown:downEvent2];
  [NSApp postEvent:upEvent3 atStart:YES];
  [field_ mouseDown:downEvent3];
  EXPECT_TRUE([field_ currentEditor]);

  // Selected the first word.
  const NSRange selectedRange([[field_ currentEditor] selectedRange]);
  EXPECT_EQ(selectedRange.location, 0U);
  EXPECT_EQ(selectedRange.length, [[field_ stringValue] length]);
}

TEST_F(AutocompleteTextFieldTest, SecurityIconMouseDown) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  scoped_nsobject<NSImage> hintIcon(
      [[NSImage alloc] initWithSize:NSMakeSize(20, 20)]);
  [cell setHintIcon:hintIcon.get() label:nil color:nil];
  NSRect iconFrame([cell hintImageFrameForFrame:[field_ bounds]]);
  NSPoint location(NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame)));
  NSEvent* event(Event(field_, location, NSLeftMouseDown, 1));
  EXPECT_CALL(field_observer_, OnSecurityIconClicked());
  [field_ mouseDown:event];
}

}  // namespace
