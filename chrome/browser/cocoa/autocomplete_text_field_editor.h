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
}

@property(nonatomic) Profile* profile;

// The delegate is always an AutocompleteTextField*.  Override the superclass
// implementations to allow for proper typing.
- (AutocompleteTextField*)delegate;
- (void)setDelegate:(AutocompleteTextField*)delegate;

@end

@interface AutocompleteTextFieldEditor(PrivateTestMethods)
- (AutocompleteTextFieldObserver*)observer;
- (void)pasteAndGo:sender;
@end
