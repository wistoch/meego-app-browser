// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/resource_bundle.h"
#import "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"
#import "chrome/browser/cocoa/autocomplete_text_field_unittest_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
class MockLocationIconView : public LocationBarViewMac::LocationIconView {
 public:
  MockLocationIconView(LocationBarViewMac* owner)
      : LocationBarViewMac::LocationIconView(owner) {}

  // |LocationBarViewMac::LocationIconView| dragging support needs
  // more setup than this test provides.
  bool IsDraggable() {
    return false;
  }
  virtual NSPasteboard* GetDragPasteboard() {
    return [NSPasteboard pasteboardWithUniqueName];
  }

  // We can't use gmock's MOCK_METHOD macro, because it doesn't like the
  // NSRect argument to OnMousePressed.
  virtual void OnMousePressed(NSRect bounds) {
    mouse_was_pressed_ = true;
  }
  bool mouse_was_pressed_;
};

class MockPageActionImageView : public LocationBarViewMac::PageActionImageView {
 public:
  MockPageActionImageView() {}
  virtual ~MockPageActionImageView() {}

  // We can't use gmock's MOCK_METHOD macro, because it doesn't like the
  // NSRect argument to OnMousePressed.
  virtual void OnMousePressed(NSRect bounds) {
    mouse_was_pressed_ = true;
  }

  bool MouseWasPressed() { return mouse_was_pressed_; }

  void SetMenu(NSMenu* aMenu) {
    menu_.reset([aMenu retain]);
  }
  virtual NSMenu* GetMenu() { return menu_; }

private:
  scoped_nsobject<NSMenu> menu_;
  bool mouse_was_pressed_;
};

// TODO(shess): Consider lifting this to
// autocomplete_text_field_unittest_helper.h to share this with the
// cell tests.
class TestPageActionViewList : public LocationBarViewMac::PageActionViewList {
 public:
  TestPageActionViewList()
      : LocationBarViewMac::PageActionViewList(NULL, NULL, NULL) {}
  ~TestPageActionViewList() {
    // |~PageActionViewList()| calls delete on the contents of
    // |views_|, which here are refs to stack objects.
    views_.clear();
  }

  void Add(LocationBarViewMac::PageActionImageView* view) {
    views_.push_back(view);
  }
};

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

class AutocompleteTextFieldTest : public CocoaTest {
 public:
  AutocompleteTextFieldTest() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    NSRect frame = NSMakeRect(0, 0, kWidth, 30);
    scoped_nsobject<AutocompleteTextField> field(
        [[AutocompleteTextField alloc] initWithFrame:frame]);
    field_ = field.get();
    [field_ setStringValue:@"Test test"];
    [[test_window() contentView] addSubview:field_];

    window_delegate_.reset(
        [[AutocompleteTextFieldWindowTestDelegate alloc] init]);
    [test_window() setDelegate:window_delegate_.get()];
  }

  NSEvent* KeyDownEventWithFlags(NSUInteger flags) {
    return [NSEvent keyEventWithType:NSKeyDown
                            location:NSZeroPoint
                       modifierFlags:flags
                           timestamp:0.0
                        windowNumber:[test_window() windowNumber]
                             context:nil
                          characters:@"a"
         charactersIgnoringModifiers:@"a"
                           isARepeat:NO
                             keyCode:'a'];
  }

  // Helper to return the field-editor frame being used w/in |field_|.
  NSRect EditorFrame() {
    EXPECT_TRUE([field_ currentEditor]);
    EXPECT_EQ([[field_ subviews] count], 1U);
    if ([[field_ subviews] count] > 0) {
      return [[[field_ subviews] objectAtIndex:0] frame];
    } else {
      // Return something which won't work so the caller can soldier
      // on.
      return NSZeroRect;
    }
  }

  AutocompleteTextField* field_;
  scoped_nsobject<AutocompleteTextFieldWindowTestDelegate> window_delegate_;
};

TEST_VIEW(AutocompleteTextFieldTest, field_);

// Base class for testing AutocompleteTextFieldObserver messages.
class AutocompleteTextFieldObserverTest : public AutocompleteTextFieldTest {
 public:
  virtual void SetUp() {
    AutocompleteTextFieldTest::SetUp();
    [field_ setObserver:&field_observer_];
  }

  virtual void TearDown() {
    // Clear the observer so that we don't show output for
    // uninteresting messages to the mock (for instance, if |field_| has
    // focus at the end of the test).
    [field_ setObserver:NULL];

    AutocompleteTextFieldTest::TearDown();
  }

  StrictMock<MockAutocompleteTextFieldObserver> field_observer_;
};

// Test that we have the right cell class.
TEST_F(AutocompleteTextFieldTest, CellClass) {
  EXPECT_TRUE([[field_ cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
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
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  EXPECT_FALSE(nil == [field_ currentEditor]);
  EXPECT_EQ([[field_ subviews] count], 1U);
  EXPECT_TRUE([[field_ currentEditor] isDescendantOf:field_]);

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
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  [field_ display];
  [test_window() clearPretendKeyWindowAndFirstResponder];

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

TEST_F(AutocompleteTextFieldObserverTest, FlagsChanged) {
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
TEST_F(AutocompleteTextFieldObserverTest, FieldEditorFlagsChanged) {
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
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
TEST_F(AutocompleteTextFieldObserverTest, FrameChanged) {
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

  // Capture the editor frame resulting from the standard focus
  // machinery.
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  const NSRect baseEditorFrame(EditorFrame());

  // Setting a hint should result in a strictly smaller editor frame.
  EXPECT_FALSE([cell hintString]);
  [cell setSearchHintString:@"search hint" availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE(NSEqualRects(baseEditorFrame, EditorFrame()));
  EXPECT_TRUE(NSContainsRect(baseEditorFrame, EditorFrame()));

  // Clearing hint string and using -resetFieldEditorFrameIfNeeded
  // should result in the same frame as the standard focus machinery.
  [cell clearKeywordAndHint];
  EXPECT_FALSE([cell hintString]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_TRUE(NSEqualRects(baseEditorFrame, EditorFrame()));
}

// Test that the field editor gets the same bounds when focus is
// delivered by the standard focusing machinery, or by
// -resetFieldEditorFrameIfNeeded.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorSearchHint) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  const NSString* kHintString(@"Type to search");

  // Capture the editor frame resulting from the standard focus
  // machinery.
  [cell setSearchHintString:kHintString availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  const NSRect baseEditorFrame(EditorFrame());

  // Clearing the hint should result in a strictly larger editor
  // frame.
  [cell clearKeywordAndHint];
  EXPECT_FALSE([cell hintString]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE(NSEqualRects(baseEditorFrame, EditorFrame()));
  EXPECT_TRUE(NSContainsRect(EditorFrame(), baseEditorFrame));

  // Setting the same hint string and using
  // -resetFieldEditorFrameIfNeeded should result in the same frame as
  // the standard focus machinery.
  [cell setSearchHintString:kHintString availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_TRUE(NSEqualRects(baseEditorFrame, EditorFrame()));
}

// Test that the field editor gets the same bounds when focus is
// delivered by the standard focusing machinery, or by
// -resetFieldEditorFrameIfNeeded.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorKeywordHint) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  const NSString* kFullString(@"Search Engine:");
  const NSString* kPartialString(@"Search Eng:");

  // Capture the editor frame resulting from the standard focus
  // machinery.
  [cell setKeywordString:kFullString
           partialString:kPartialString
          availableWidth:kWidth];
  EXPECT_TRUE([cell keywordString]);
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  const NSRect baseEditorFrame(EditorFrame());

  // Clearing the hint should result in a strictly larger editor
  // frame.
  [cell clearKeywordAndHint];
  EXPECT_FALSE([cell keywordString]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_FALSE(NSEqualRects(baseEditorFrame, EditorFrame()));
  EXPECT_TRUE(NSContainsRect(EditorFrame(), baseEditorFrame));

  // Setting the same hint string and using
  // -resetFieldEditorFrameIfNeeded should result in the same frame as
  // the standard focus machinery.
  [cell setKeywordString:kFullString
           partialString:kPartialString
          availableWidth:kWidth];
  EXPECT_TRUE([cell keywordString]);
  [field_ resetFieldEditorFrameIfNeeded];
  EXPECT_TRUE(NSEqualRects(baseEditorFrame, EditorFrame()));
}

// Test that resetting the field editor bounds does not cause untoward
// messages to the field's observer.
TEST_F(AutocompleteTextFieldObserverTest, ResetFieldEditorContinuesEditing) {
  // Becoming first responder doesn't begin editing.
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
  EXPECT_TRUE(nil != editor);

  // This should begin editing and indicate a change.
  EXPECT_CALL(field_observer_, OnDidBeginEditing());
  EXPECT_CALL(field_observer_, OnDidChange());
  [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
  [editor didChangeText];

  // No messages to |field_observer_| when resetting the frame.
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  [field_ resetFieldEditorFrameIfNeeded];
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
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
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
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
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
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
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

// Clicking the security icon should call its OnMousePressed.
TEST_F(AutocompleteTextFieldTest, LocationIconMouseDown) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  MockLocationIconView location_icon_view(NULL);
  [cell setLocationIconView:&location_icon_view];
  location_icon_view.SetImage(
      ResourceBundle::GetSharedInstance().GetNSImageNamed(
          IDR_OMNIBOX_HTTPS_VALID));
  location_icon_view.SetVisible(true);

  NSRect iconFrame([cell locationIconFrameForFrame:[field_ bounds]]);
  NSPoint location(NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame)));
  NSEvent* downEvent(Event(field_, location, NSLeftMouseDown, 1));
  NSEvent* upEvent(Event(field_, location, NSLeftMouseUp, 1));

  // Since location icon can be dragged, the mouse-press is sent on
  // mouse-up.
  [NSApp postEvent:upEvent atStart:YES];
  [field_ mouseDown:downEvent];
  EXPECT_TRUE(location_icon_view.mouse_was_pressed_);

  // TODO(shess): Test that mouse drags are initiated if the next
  // event is a drag, or if the mouse-up takes too long to arrive.
}

// Clicking a Page Action icon should call its OnMousePressed.
TEST_F(AutocompleteTextFieldTest, PageActionMouseDown) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  MockPageActionImageView page_action_view;
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  page_action_view.SetImage(image);

  MockPageActionImageView page_action_view2;
  page_action_view2.SetImage(image);

  TestPageActionViewList list;
  list.Add(&page_action_view);
  list.Add(&page_action_view2);
  [cell setPageActionViewList:&list];

  // One page action.
  page_action_view.SetVisible(true);
  page_action_view2.SetVisible(false);
  NSRect iconFrame([cell pageActionFrameForIndex:0 inFrame:[field_ bounds]]);
  NSPoint location(NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame)));
  NSEvent* event(Event(field_, location, NSLeftMouseDown, 1));

  [field_ mouseDown:event];
  EXPECT_TRUE(page_action_view.MouseWasPressed());

  // Two page actions, no lock.
  page_action_view2.SetVisible(true);
  iconFrame = [cell pageActionFrameForIndex:0 inFrame:[field_ bounds]];
  location = NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame));
  event = Event(field_, location, NSLeftMouseDown, 1);

  [field_ mouseDown:event];
  EXPECT_TRUE(page_action_view.MouseWasPressed());

  iconFrame = [cell pageActionFrameForIndex:1 inFrame:[field_ bounds]];
  location = NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame));
  event = Event(field_, location, NSLeftMouseDown, 1);

  [field_ mouseDown:event];
  EXPECT_TRUE(page_action_view.MouseWasPressed());

  // Two page actions.
  iconFrame = [cell pageActionFrameForIndex:0 inFrame:[field_ bounds]];
  location = NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame));
  event = Event(field_, location, NSLeftMouseDown, 1);

  [field_ mouseDown:event];
  EXPECT_TRUE(page_action_view.MouseWasPressed());

  iconFrame = [cell pageActionFrameForIndex:1 inFrame:[field_ bounds]];
  location = NSMakePoint(NSMidX(iconFrame), NSMidY(iconFrame));
  event = Event(field_, location, NSLeftMouseDown, 1);

  [field_ mouseDown:event];
  EXPECT_TRUE(page_action_view.MouseWasPressed());
}

// Test that page action menus are properly returned.
// TODO(shess): Really, this should test that things are forwarded to
// the cell, and the cell tests should test that the right things are
// selected.  It's easier to mock the event here, though.  This code's
// event-mockers might be worth promoting to |test_event_utils.h| or
// |cocoa_test_helper.h|.
TEST_F(AutocompleteTextFieldTest, PageActionMenu) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  const NSRect bounds([field_ bounds]);

  const CGFloat edge = NSHeight(bounds) - 4.0;
  const NSSize size = NSMakeSize(edge, edge);
  scoped_nsobject<NSImage> image([[NSImage alloc] initWithSize:size]);

  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"Menu"]);

  MockPageActionImageView page_action_views[2];
  page_action_views[0].SetImage(image);
  page_action_views[0].SetMenu(menu);
  page_action_views[0].SetVisible(true);

  page_action_views[1].SetImage(image);
  page_action_views[1].SetVisible(true);

  TestPageActionViewList list;
  list.Add(&page_action_views[0]);
  list.Add(&page_action_views[1]);
  [cell setPageActionViewList:&list];

  // The item with a menu returns it.
  NSRect actionFrame = [cell pageActionFrameForIndex:0 inFrame:bounds];
  NSPoint location = NSMakePoint(NSMidX(actionFrame), NSMidY(actionFrame));
  NSEvent* event = Event(field_, location, NSRightMouseDown, 1);

  NSMenu *actionMenu = [field_ actionMenuForEvent:event];
  EXPECT_EQ(actionMenu, menu);

  // The item without a menu returns nil.
  actionFrame = [cell pageActionFrameForIndex:1 inFrame:bounds];
  location = NSMakePoint(NSMidX(actionFrame), NSMidY(actionFrame));
  event = Event(field_, location, NSRightMouseDown, 1);
  EXPECT_FALSE([field_ actionMenuForEvent:event]);

  // Something not in an action returns nil.
  location = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  event = Event(field_, location, NSRightMouseDown, 1);
  EXPECT_FALSE([field_ actionMenuForEvent:event]);
}

// Verify that -setAttributedStringValue: works as expected when
// focussed or when not focussed.  Our code mostly depends on about
// whether -stringValue works right.
TEST_F(AutocompleteTextFieldTest, SetAttributedStringBaseline) {
  EXPECT_EQ(nil, [field_ currentEditor]);

  // So that we can set rich text.
  [field_ setAllowsEditingTextAttributes:YES];

  // Set an attribute different from the field's default so we can
  // tell we got the same string out as we put in.
  NSFont* font = [NSFont fontWithDescriptor:[[field_ font] fontDescriptor]
                                       size:[[field_ font] pointSize] + 2];
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:font
                                  forKey:NSFontAttributeName];
  static const NSString* kString = @"This is a test";
  scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc] initWithString:kString
                                      attributes:attributes]);

  // Check that what we get back looks like what we put in.
  EXPECT_FALSE([[field_ stringValue] isEqualToString:kString]);
  [field_ setAttributedStringValue:attributedString];
  EXPECT_TRUE([[field_ attributedStringValue]
                isEqualToAttributedString:attributedString]);
  EXPECT_TRUE([[field_ stringValue] isEqualToString:kString]);

  // Try that again with focus.
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];

  EXPECT_TRUE([field_ currentEditor]);

  // Check that what we get back looks like what we put in.
  [field_ setStringValue:@""];
  EXPECT_FALSE([[field_ stringValue] isEqualToString:kString]);
  [field_ setAttributedStringValue:attributedString];
  EXPECT_TRUE([[field_ attributedStringValue]
                isEqualToAttributedString:attributedString]);
  EXPECT_TRUE([[field_ stringValue] isEqualToString:kString]);
}

// -setAttributedStringValue: shouldn't reset the undo state if things
// are being editted.
TEST_F(AutocompleteTextFieldTest, SetAttributedStringUndo) {
  NSColor* redColor = [NSColor redColor];
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:redColor
                                  forKey:NSForegroundColorAttributeName];
  static const NSString* kString = @"This is a test";
  scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc] initWithString:kString
                                      attributes:attributes]);
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  EXPECT_TRUE([field_ currentEditor]);
  NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
  NSUndoManager* undoManager = [editor undoManager];
  EXPECT_TRUE(undoManager);

  // Nothing to undo, yet.
  EXPECT_FALSE([undoManager canUndo]);

  // Starting an editing action creates an undoable item.
  [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
  [editor didChangeText];
  EXPECT_TRUE([undoManager canUndo]);

  // -setStringValue: resets the editor's undo chain.
  [field_ setStringValue:kString];
  EXPECT_FALSE([undoManager canUndo]);

  // Verify that -setAttributedStringValue: does not reset the
  // editor's undo chain.
  [field_ setStringValue:@""];
  [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
  [editor didChangeText];
  EXPECT_TRUE([undoManager canUndo]);
  [field_ setAttributedStringValue:attributedString];
  EXPECT_TRUE([undoManager canUndo]);

  // Verify that calling -clearUndoChain clears the undo chain.
  [field_ clearUndoChain];
  EXPECT_FALSE([undoManager canUndo]);
}

TEST_F(AutocompleteTextFieldTest, EditorGetsCorrectUndoManager) {
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];

  NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
  EXPECT_TRUE(editor);
  EXPECT_EQ([field_ undoManagerForTextView:editor], [editor undoManager]);
}

TEST_F(AutocompleteTextFieldObserverTest, SendsEditingMessages) {
  // Becoming first responder doesn't begin editing.
  [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
  NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
  EXPECT_TRUE(nil != editor);

  // This should begin editing and indicate a change.
  EXPECT_CALL(field_observer_, OnDidBeginEditing());
  EXPECT_CALL(field_observer_, OnDidChange());
  [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
  [editor didChangeText];

  // Further changes don't send the begin message.
  EXPECT_CALL(field_observer_, OnDidChange());
  [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
  [editor didChangeText];

  // -doCommandBySelector: should forward to observer via |field_|.
  // TODO(shess): Test with a fake arrow-key event?
  const SEL cmd = @selector(moveDown:);
  EXPECT_CALL(field_observer_, OnDoCommandBySelector(cmd))
      .WillOnce(Return(true));
  [editor doCommandBySelector:cmd];

  // Finished with the changes.
  EXPECT_CALL(field_observer_, OnDidEndEditing());
  [test_window() clearPretendKeyWindowAndFirstResponder];
}

// Test that the resign-key notification is forwarded right, and that
// the notification is registered and unregistered when the view moves
// in and out of the window.
// TODO(shess): Should this test the key window for realz?  That would
// be really annoying to whoever is running the tests.
TEST_F(AutocompleteTextFieldObserverTest, SendsOnResignKey) {
  EXPECT_CALL(field_observer_, OnDidResignKey());
  [test_window() resignKeyWindow];

  scoped_nsobject<AutocompleteTextField> pin([field_ retain]);
  [field_ removeFromSuperview];
  [test_window() resignKeyWindow];

  [[test_window() contentView] addSubview:field_];
  EXPECT_CALL(field_observer_, OnDidResignKey());
  [test_window() resignKeyWindow];
}

}  // namespace
