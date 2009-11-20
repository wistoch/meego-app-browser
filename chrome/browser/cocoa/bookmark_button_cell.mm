// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_menu.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@implementation BookmarkButtonCell

- (id)initTextCell:(NSString*)string {
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

- (void)setBookmarkCellText:(NSString*)title
                      image:(NSImage*)image {
  title = [title stringByReplacingOccurrencesOfString:@"\n"
                                           withString:@" "];
  title = [title stringByReplacingOccurrencesOfString:@"\r"
                                           withString:@" "];
  // Center the image if we have a title, or if there already was a
  // title set.
  BOOL hasTitle = (([title length] > 0) ||
                   ([[self title] length] > 0));
  if (image) {
    [self setImage:image];
    if (hasTitle) {
      [self setImagePosition:NSImageLeft];
    } else {
      [self setImagePosition:NSImageOnly];
    }
  }

  if (title)
    [self setTitle:title];
}

// We share the context menu among all bookmark buttons.  To allow us
// to disambiguate when needed (e.g. "open bookmark"), we set the
// menu's associated node to be our represented object.
- (NSMenu*)menu {
  BookmarkMenu* menu = (BookmarkMenu*)[super menu];
  [menu setRepresentedObject:[self representedObject]];
  return menu;
}

// Unfortunately, NSCell doesn't already have something like this.
// TODO(jrg): consider placing in GTM.
- (void)setTextColor:(NSColor*)color {
  scoped_nsobject<NSMutableParagraphStyle> style([NSMutableParagraphStyle new]);
  [style setAlignment:NSCenterTextAlignment];
  NSDictionary* dict = [NSDictionary
                         dictionaryWithObjectsAndKeys:color,
                         NSForegroundColorAttributeName,
                         [self font], NSFontAttributeName,
                         style.get(), NSParagraphStyleAttributeName,
                         nil];
  scoped_nsobject<NSAttributedString> ats([[NSAttributedString alloc]
                                            initWithString:[self title]
                                                attributes:dict]);
  NSButton* button = static_cast<NSButton*>([self controlView]);
  if (button) {
    DCHECK([button isKindOfClass:[NSButton class]]);
    [button setAttributedTitle:ats.get()];
  }
}

@end
