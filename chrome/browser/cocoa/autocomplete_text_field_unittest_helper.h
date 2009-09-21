// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_UNITTEST_HELPER_H_
#define CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_UNITTEST_HELPER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#include "testing/gmock/include/gmock/gmock.h"

@class AutocompleteTextFieldEditor;

// Return the right field editor for AutocompleteTextField instance.

@interface AutocompleteTextFieldWindowTestDelegate : NSObject {
  scoped_nsobject<AutocompleteTextFieldEditor> editor_;
}
- (id)windowWillReturnFieldEditor:(NSWindow *)sender toObject:(id)anObject;
@end

namespace {

// Allow monitoring calls into AutocompleteTextField's observer.
// Being in a .h file with an anonymous namespace is strange, but this
// is here so the mock interface doesn't have to change in multiple
// places.

class MockAutocompleteTextFieldObserver : public AutocompleteTextFieldObserver {
 public:
  MOCK_METHOD1(OnControlKeyChanged, void(bool pressed));
  MOCK_METHOD0(OnPaste, void());
};

}  // namespace

#endif  // CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_UNITTEST_HELPER_H_
