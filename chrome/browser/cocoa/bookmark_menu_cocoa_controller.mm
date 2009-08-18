// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/text_elider.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/common/cocoa_utils.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// Menus more than this many pixels wide will get trimmed
// TODO(jrg): ask UI dudes what a good value is.
const NSUInteger kMaximumMenuPixelsWide = 300;

}

@implementation BookmarkMenuCocoaController

+ (NSString*)menuTitleForNode:(const BookmarkNode*)node {
  NSFont* nsfont = [NSFont menuBarFontOfSize:0];  // 0 means "default"
  gfx::Font font = gfx::Font::CreateFont(base::SysNSStringToWide([nsfont
                                                                   fontName]),
                                         (int)[nsfont pointSize]);
  std::wstring title = gfx::ElideText(node->GetTitle(),
                                      font,
                                      kMaximumMenuPixelsWide);
  return base::SysWideToNSString(title);
}

- (id)initWithBridge:(BookmarkMenuBridge *)bridge {
  if ((self = [super init])) {
    bridge_ = bridge;
    DCHECK(bridge_);
  }
  return self;
}

// Return the a BookmarkNode that has the given id (called
// "identifier" here to avoid conflict with objc's concept of "id").
- (const BookmarkNode*)nodeForIdentifier:(int)identifier {
  return bridge_->GetBookmarkModel()->GetNodeByID(identifier);
}

// Open the URL of the given BookmarkNode in the current tab.
- (void)openURLForNode:(const BookmarkNode*)node {
  Browser* browser = BrowserList::GetLastActive();

  if (!browser) {  // No windows open?
    Browser::OpenEmptyWindow(bridge_->GetDefaultProfile());
    browser = BrowserList::GetLastActive();
  }
  DCHECK(browser);
  TabContents* tab_contents = browser->GetSelectedTabContents();
  DCHECK(tab_contents);

  // A TabContents is a PageNavigator, so we can OpenURL() on it.
  WindowOpenDisposition disposition = event_utils::DispositionFromEventFlags(
      [[NSApp currentEvent] modifierFlags]);
  tab_contents->OpenURL(node->GetURL(), GURL(), disposition,
                        PageTransition::AUTO_BOOKMARK);
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  int identifier = tag;
  const BookmarkNode* node = [self nodeForIdentifier:identifier];
  DCHECK(node);
  if (!node)
    return;  // shouldn't be reached

  [self openURLForNode:node];
}

@end  // BookmarkMenuCocoaController

