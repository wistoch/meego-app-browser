// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"

@class BookmarkTreeBrowserCell;

// A controller for the bookmark editor, opened with Edit... from the
// context menu of a bookmark button.
@interface BookmarkEditorController : NSWindowController<NSMatrixDelegate,
                                                         NSTextFieldDelegate> {
 @private
  IBOutlet NSTextField* nameField_;
  IBOutlet NSTextField* urlField_;
  IBOutlet NSBrowser* folderBrowser_;
  IBOutlet NSButton* okButton_;
  IBOutlet NSButton* newFolderButton_;

  NSWindow* parentWindow_;
  Profile* profile_;  // weak
  const BookmarkNode* parentNode_;  // weak; owned by the model
  const BookmarkNode* node_;  // weak; owned by the model
  BookmarkEditor::Configuration configuration_;
  scoped_ptr<BookmarkEditor::Handler> handler_;  // we take ownership

  scoped_nsobject<NSString> initialName_;
  scoped_nsobject<NSString> initialUrl_;
  scoped_nsobject<BookmarkTreeBrowserCell> currentEditCell_;
}

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
                      node:(const BookmarkNode*)node
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler;

// Run the bookmark editor as a modal sheet.  Does not block.
- (void)runAsModalSheet;

// Create a new folder at the end of the selected parent folder, give it
// an untitled name, and put it into editing mode.
- (IBAction)newFolder:(id)sender;

// Actions for the buttons at the bottom of the window.
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;
@end

@interface BookmarkEditorController(TestingAPI)
@property (assign) NSString* displayName;
@property (assign) NSString* displayURL;
@property (readonly) BOOL okButtonEnabled;
- (void)selectTestNodeInBrowser:(const BookmarkNode*)node;
@end

#endif  /* CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_CONTROLLER_H_ */
