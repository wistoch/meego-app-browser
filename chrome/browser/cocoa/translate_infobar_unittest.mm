// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/translate_infobar.h"

#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"  // For translate menu command ids.
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// All states the translate toolbar can assume.
TranslateInfoBarDelegate::TranslateState kTranslateToolbarStates[] = {
   TranslateInfoBarDelegate::kBeforeTranslate,
   TranslateInfoBarDelegate::kAfterTranslate,
   TranslateInfoBarDelegate::kTranslateError};

class MockTranslateInfoBarDelegate : public TranslateInfoBarDelegate {
 public:
  MockTranslateInfoBarDelegate() {
    // Start out in the "Before Translate" state.
    UpdateState(kBeforeTranslate, TranslateErrors::NONE);
  }

  virtual string16 GetDisplayNameForLocale(const std::string& language_code) {
    return ASCIIToUTF16("Foo");
  }

  virtual bool IsLanguageBlacklisted() {
    return false;
  }

  virtual bool IsSiteBlacklisted() {
    return false;
  }

  virtual bool ShouldAlwaysTranslate() {
    return false;
  }

  MOCK_METHOD0(Translate, void());
  MOCK_METHOD0(RevertTranslation, void());
  MOCK_METHOD0(TranslationDeclined, void());
  MOCK_METHOD0(ToggleLanguageBlacklist, void());
  MOCK_METHOD0(ToggleSiteBlacklist, void());
  MOCK_METHOD0(ToggleAlwaysTranslate, void());

};

class TranslationBarInfoTest : public CocoaTest {
 public:
  scoped_ptr<MockTranslateInfoBarDelegate> infobar_delegate;
  scoped_nsobject<TranslateInfoBarController> infobar_controller;

 public:
  // Each test gets a single Mock translate delegate for the lifetime of
  // the test.
  virtual void SetUp() {
    CocoaTest::SetUp();
    infobar_delegate.reset(new MockTranslateInfoBarDelegate);
  }

  void CreateInfoBar() {
    CreateInfoBar(TranslateInfoBarDelegate::kBeforeTranslate);
  }

  void CreateInfoBar(TranslateInfoBarDelegate::TranslateState initial_state) {
    infobar_delegate->UpdateState(initial_state, TranslateErrors::NONE);
    [[infobar_controller view] removeFromSuperview];
    infobar_controller.reset(
        [[TranslateInfoBarController alloc]
            initWithDelegate:infobar_delegate.get()]);
    // Need to call this to get the view to load from nib.
    [[test_window() contentView] addSubview:[infobar_controller view]];
  }
};

// Check that we can instantiate a Translate Infobar correctly.
TEST_F(TranslationBarInfoTest, Instantiate) {
  CreateInfoBar();
  ASSERT_TRUE(infobar_controller.get());
}

// Check that clicking the Translate button calls Translate().
TEST_F(TranslationBarInfoTest, TranslateCalledOnButtonPress) {
  CreateInfoBar();

  EXPECT_CALL(*infobar_delegate, Translate()).Times(1);
  [infobar_controller ok:nil];
}

// Check that clicking the "Show Original button calls RevertTranslation().
TEST_F(TranslationBarInfoTest, RevertCalledOnButtonPress) {
  CreateInfoBar();

  EXPECT_CALL(*infobar_delegate, RevertTranslation()).Times(1);
  [infobar_controller showOriginal:nil];
}

// Check that UI is layed out correctly as we transition synchronously through
// toolbar states.
TEST_F(TranslationBarInfoTest, StateTransitions) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);
  CreateInfoBar();

  for (size_t i = 0; i < arraysize(kTranslateToolbarStates); ++i) {
    TranslateInfoBarDelegate::TranslateState state = kTranslateToolbarStates[i];
    TranslateErrors::Type error = TranslateErrors::NONE;

    infobar_delegate->UpdateState(state, error);

    // Pending Translation == false.
    if (i != 0) {
      // First time around, the toolbar should already be layed out.
      [infobar_controller updateState:state
                   translationPending:false
                                error:error];
    }

    bool result =
        [infobar_controller verifyLayout:state
                      translationPending:false];
    EXPECT_TRUE(result) << "Layout wrong, for state " << state <<
        "translatePending=false";

    // Pending Translation == true.
    [infobar_controller updateState:state
                 translationPending:true
                              error:error];

    result = [infobar_controller verifyLayout:state translationPending:true];
    EXPECT_TRUE(result) << "Layout wrong, for state " << state <<
        "translatePending=true";

  }
}

// Check that items in the options menu are hooked up correctly.
TEST_F(TranslationBarInfoTest, OptionsMenuItemsHookedUp) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);
  CreateInfoBar();

  [infobar_controller rebuildOptionsMenu];
  NSMenu* optionsMenu = [infobar_controller optionsMenu];
  NSArray* optionsMenuItems = [optionsMenu itemArray];

  EXPECT_EQ([optionsMenuItems count], 5U);

  // First item is the options menu button's title, so there's no need to test
  // that the target on that is setup correctly.
  for (NSUInteger i = 1; i < [optionsMenuItems count]; ++i) {
    NSMenuItem* item = [optionsMenuItems objectAtIndex:i];
    EXPECT_EQ([item target], infobar_controller.get());
  }
  NSMenuItem* alwaysTranslateLanguateItem = [optionsMenuItems objectAtIndex:1];
  NSMenuItem* neverTranslateLanguateItem = [optionsMenuItems objectAtIndex:2];
  NSMenuItem* neverTranslateSiteItem = [optionsMenuItems objectAtIndex:3];
  NSMenuItem* aboutTranslateItem = [optionsMenuItems objectAtIndex:4];

  {
    EXPECT_CALL(*infobar_delegate, ToggleAlwaysTranslate())
    .Times(1);
    [infobar_controller menuItemSelected:alwaysTranslateLanguateItem];
  }

  {
    EXPECT_CALL(*infobar_delegate, ToggleLanguageBlacklist())
    .Times(1);
    [infobar_controller menuItemSelected:neverTranslateLanguateItem];
  }

  {
    EXPECT_CALL(*infobar_delegate, ToggleSiteBlacklist())
    .Times(1);
    [infobar_controller menuItemSelected:neverTranslateSiteItem];
  }

  {
    // Can't mock this effectively, so just check that the tag is set correctly.
    EXPECT_EQ([aboutTranslateItem tag], IDC_TRANSLATE_OPTIONS_ABOUT);
  }
}

// Check that selecting a new item from the "Source Language" popup in "before
// translate" mode doesn't trigger a translation or change state.
// http://crbug.com/36666
TEST_F(TranslationBarInfoTest, Bug36666) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);

  CreateInfoBar();
  int arbitrary_index = 2;
  [infobar_controller sourceLanguageModified:arbitrary_index];
  EXPECT_EQ(infobar_delegate->state(),
      TranslateInfoBarDelegate::kBeforeTranslate);
  EXPECT_EQ([infobar_controller state],
      TranslateInfoBarDelegate::kBeforeTranslate);
}

// Check that the infobar lays itself out correctly when instantiated in
// each of the states.
// http://crbug.com/36895
TEST_F(TranslationBarInfoTest, Bug36895) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);

  for (size_t i = 0; i < arraysize(kTranslateToolbarStates); ++i) {
    CreateInfoBar(kTranslateToolbarStates[i]);
    EXPECT_TRUE(
        [infobar_controller verifyLayout:kTranslateToolbarStates[i]
                      translationPending:false]) <<
        "Layout wrong, for state #" << i;
  }
}

}  // namespace
