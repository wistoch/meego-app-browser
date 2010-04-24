// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/url_drop_target.h"

@class AutocompleteTextField;
class AutocompleteTextFieldObserver;
class Profile;

// AutocompleteTextFieldEditor customized the AutocompletTextField
// field editor (helper text-view used in editing).  It intercepts UI
// events for forwarding to the core Omnibox code.  It also undoes
// some of the effects of using styled text in the Omnibox (the text
// is styled but should not appear that way when copied to the
// pasteboard).

// Field editor used for the autocomplete field.
@interface AutocompleteTextFieldEditor : NSTextView<URLDropTarget> {
  // Handles being a drag-and-drop target. We handle DnD directly instead
  // allowing the |AutocompletTextField| to handle it (by making an empty
  // |-updateDragTypeRegistration|), since the latter results in a weird
  // start-up time regression.
  scoped_nsobject<URLDropTargetHandler> dropHandler_;

  // The browser profile for the editor. Weak.
  Profile* profile_;

  scoped_nsobject<NSCharacterSet> forbiddenCharacters_;

  // Indicates if the field editor's interpretKeyEvents: method is being called.
  // If it's YES, then we should postpone the call to the observer's
  // OnDidChange() method after the field editor's interpretKeyEvents: method
  // is finished, rather than calling it in textDidChange: method. Because the
  // input method may update the marked text after inserting some text, but we
  // need the observer be aware of the marked text as well.
  BOOL interpretingKeyEvents_;

  // Indicates if the text has been changed by key events.
  BOOL textChangedByKeyEvents_;
}

@property(nonatomic) Profile* profile;

// The delegate is always an AutocompleteTextField*.  Override the superclass
// implementations to allow for proper typing.
- (AutocompleteTextField*)delegate;
- (void)setDelegate:(AutocompleteTextField*)delegate;

// Sets attributed string programatically through the field editor's text
// storage object.
- (void)setAttributedString:(NSAttributedString*)aString;

@end

@interface AutocompleteTextFieldEditor(PrivateTestMethods)
- (AutocompleteTextFieldObserver*)observer;
- (void)pasteAndGo:sender;
@end
