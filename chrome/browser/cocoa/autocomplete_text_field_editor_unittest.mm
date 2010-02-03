// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#import "chrome/browser/cocoa/autocomplete_text_field_unittest_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace {

int NumTypesOnPasteboard(NSPasteboard* pb) {
  return [[pb types] count];
}

void ClearPasteboard(NSPasteboard* pb) {
  [pb declareTypes:[NSArray array] owner:nil];
}

// TODO(shess): Very similar to AutocompleteTextFieldTest.  Maybe
// those can be shared.

class AutocompleteTextFieldEditorTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    scoped_nsobject<AutocompleteTextField> field(
        [[AutocompleteTextField alloc] initWithFrame:frame]);
    field_ = field.get();
    [field_ setStringValue:@"Testing"];
    [[test_window() contentView] addSubview:field_];

    // Arrange for |field_| to get the right field editor.
    window_delegate_.reset(
        [[AutocompleteTextFieldWindowTestDelegate alloc] init]);
    [test_window() setDelegate:window_delegate_.get()];

    // Get the field editor setup.
    [test_window() makePretendKeyWindowAndSetFirstResponder:field_];
    editor_ = static_cast<AutocompleteTextFieldEditor*>([field_ currentEditor]);

    EXPECT_TRUE(editor_);
    EXPECT_TRUE([editor_ isKindOfClass:[AutocompleteTextFieldEditor class]]);
  }

  AutocompleteTextFieldEditor* editor_;
  AutocompleteTextField* field_;
  scoped_nsobject<AutocompleteTextFieldWindowTestDelegate> window_delegate_;
};

TEST_VIEW(AutocompleteTextFieldEditorTest, field_);

// Base class for testing AutocompleteTextFieldObserver messages.
class AutocompleteTextFieldEditorObserverTest
    : public AutocompleteTextFieldEditorTest {
 public:
  virtual void SetUp() {
    AutocompleteTextFieldEditorTest::SetUp();
    [field_ setObserver:&field_observer_];
  }

  virtual void TearDown() {
    // Clear the observer so that we don't show output for
    // uninteresting messages to the mock (for instance, if |field_| has
    // focus at the end of the test).
    [field_ setObserver:NULL];

    AutocompleteTextFieldEditorTest::TearDown();
  }

  StrictMock<MockAutocompleteTextFieldObserver> field_observer_;
};

// Test that the field editor is linked in correctly.
TEST_F(AutocompleteTextFieldEditorTest, FirstResponder) {
  EXPECT_EQ(editor_, [field_ currentEditor]);
  EXPECT_TRUE([editor_ isDescendantOf:field_]);
  EXPECT_EQ([editor_ delegate], field_);
  EXPECT_EQ([editor_ observer], [field_ observer]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldEditorTest, Display) {
  [field_ display];
  [editor_ display];
}

// Test that -paste: is correctly delegated to the observer.
TEST_F(AutocompleteTextFieldEditorObserverTest, Paste) {
  EXPECT_CALL(field_observer_, OnPaste());
  [editor_ paste:nil];
}

// Test that -copy: is correctly delegated to the observer.
TEST_F(AutocompleteTextFieldEditorObserverTest, Copy) {
  EXPECT_CALL(field_observer_, OnCopy());
  [editor_ copy:nil];
}

// Test that -cut: is correctly delegated to the observer and clears
// the text field.
TEST_F(AutocompleteTextFieldEditorObserverTest, Cut) {
  // Sets a string in the field.
  NSString* test_string = @"astring";
  EXPECT_CALL(field_observer_, OnDidBeginEditing());
  EXPECT_CALL(field_observer_, OnDidChange());
  [editor_ setString:test_string];
  [editor_ selectAll:nil];

  // Calls cut.
  EXPECT_CALL(field_observer_, OnCopy());
  [editor_ cut:nil];

  // Check if the field is cleared.
  ASSERT_EQ([[editor_ textStorage] length], 0U);
}

// Test that -pasteAndGo: is correctly delegated to the observer.
TEST_F(AutocompleteTextFieldEditorObserverTest, PasteAndGo) {
  EXPECT_CALL(field_observer_, OnPasteAndGo());
  [editor_ pasteAndGo:nil];
}

// Test that the menu is constructed correctly when CanPasteAndGo().
TEST_F(AutocompleteTextFieldEditorObserverTest, CanPasteAndGoMenu) {
  EXPECT_CALL(field_observer_, CanPasteAndGo())
      .WillOnce(Return(true));
  EXPECT_CALL(field_observer_, GetPasteActionStringId())
      .WillOnce(Return(IDS_PASTE_AND_GO));

  NSMenu* menu = [editor_ menuForEvent:nil];
  NSArray* items = [menu itemArray];
  ASSERT_EQ([items count], 6U);
  // TODO(shess): Check the titles, too?
  NSUInteger i = 0;  // Use an index to make future changes easier.
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(cut:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(copy:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(paste:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(pasteAndGo:));
  EXPECT_TRUE([[items objectAtIndex:i++] isSeparatorItem]);

  EXPECT_EQ([[items objectAtIndex:i] action], @selector(commandDispatch:));
  EXPECT_EQ([[items objectAtIndex:i] tag], IDC_EDIT_SEARCH_ENGINES);
  i++;
}

// Test that the menu is constructed correctly when !CanPasteAndGo().
TEST_F(AutocompleteTextFieldEditorObserverTest, CannotPasteAndGoMenu) {
  EXPECT_CALL(field_observer_, CanPasteAndGo())
      .WillOnce(Return(false));

  NSMenu* menu = [editor_ menuForEvent:nil];
  NSArray* items = [menu itemArray];
  ASSERT_EQ([items count], 5U);
  // TODO(shess): Check the titles, too?
  NSUInteger i = 0;  // Use an index to make future changes easier.
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(cut:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(copy:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(paste:));
  EXPECT_TRUE([[items objectAtIndex:i++] isSeparatorItem]);

  EXPECT_EQ([[items objectAtIndex:i] action], @selector(commandDispatch:));
  EXPECT_EQ([[items objectAtIndex:i] tag], IDC_EDIT_SEARCH_ENGINES);
  i++;
}

// Test that the menu is constructed correctly when field isn't
// editable.
TEST_F(AutocompleteTextFieldEditorObserverTest, CanPasteAndGoMenuNotEditable) {
  [field_ setEditable:NO];
  [editor_ setEditable:NO];

  // Never call these when not editable.
  EXPECT_CALL(field_observer_, CanPasteAndGo())
      .Times(0);
  EXPECT_CALL(field_observer_, GetPasteActionStringId())
      .Times(0);

  NSMenu* menu = [editor_ menuForEvent:nil];
  NSArray* items = [menu itemArray];
  ASSERT_EQ([items count], 3U);
  // TODO(shess): Check the titles, too?
  NSUInteger i = 0;  // Use an index to make future changes easier.
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(cut:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(copy:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(paste:));
}

}  // namespace
