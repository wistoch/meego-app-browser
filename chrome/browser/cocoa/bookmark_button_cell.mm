// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@implementation BookmarkButtonCell

- (id)initTextCell:(NSString *)string {
  if ((self = [super initTextCell:string])) {
    [self setButtonType:NSMomentaryPushInButton];
    [self setBezelStyle:NSShadowlessSquareBezelStyle];
    [self setShowsBorderOnlyWhileMouseInside:YES];
    [self setControlSize:NSSmallControlSize];
    [self setAlignment:NSLeftTextAlignment];
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [self setWraps:NO];
    // NSLineBreakByTruncatingMiddle seems more common on OSX but let's
    // try to match Windows for a bit to see what happens.
    [self setLineBreakMode:NSLineBreakByTruncatingTail];

    // Theming doesn't work for bookmark buttons yet (text chucked).
    [super setShouldTheme:NO];

  }
  return self;
}

- (NSSize)cellSizeForBounds:(NSRect)aRect {
  NSSize size = [super cellSizeForBounds:aRect];
  size.width += 2;
  size.height += 4;
  return size;
}

// We share the context menu among all bookmark buttons.  To allow us
// to disambiguate when needed (e.g. "open bookmark"), we set the
// menu's delegate to be us.  We (the cell) have the bookmark encoded
// in our represented object.
// Convention needed in -[BookmarkBarController openBookmarkIn***] calls.
- (NSMenu*)menu {
  NSMenu* menu = [super menu];
  [menu setDelegate:self];
  return menu;
}

@end
