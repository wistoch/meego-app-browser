// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_NAME_FOLDER_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_NAME_FOLDER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// A controller for dialog to let the user creating a new folder or
// renaming an existing folder.  Accessible from a context menu on a
// bookmark button or the bookmark bar.
@interface BookmarkNameFolderController : NSWindowController {
 @private
  IBOutlet NSTextField* nameField_;

  NSWindow* parentWindow_;  // weak
  Profile* profile_;  // weak
  const BookmarkNode* node_;  // weak; owned by the model
  scoped_nsobject<NSString> initialName_;
}
- (id)initWithParentWindow:(NSWindow*)window
                   profile:(Profile*)profile
                      node:(const BookmarkNode*)node;
- (void)runModal;
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;
@end

@interface BookmarkNameFolderController(TestingAPI)
- (void)setFolderName:(NSString*)name;
@end

#endif  /* CHROME_BROWSER_COCOA_BOOKMARK_NAME_FOLDER_CONTROLLER_H_ */
