// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url_model.h"

class EditSearchEngineControllerDelegate;
@class KeywordEditorCocoaController;
class Profile;

// Very thin bridge that simply pushes notifications from C++ to ObjC.
class KeywordEditorModelObserver : public TemplateURLModelObserver,
                                   public EditSearchEngineControllerDelegate {
 public:
  explicit KeywordEditorModelObserver(KeywordEditorCocoaController* controller);
  virtual ~KeywordEditorModelObserver();

  // Notification that the template url model has changed in some way.
  virtual void OnTemplateURLModelChanged();

  // Invoked from the EditSearchEngineController when the user accepts the
  // edits. NOTE: |template_url| is the value supplied to
  // EditSearchEngineController's constructor, and may be NULL. A NULL value
  // indicates a new TemplateURL should be created rather than modifying an
  // existing TemplateURL.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const std::wstring& title,
                               const std::wstring& keyword,
                               const std::wstring& url);

 private:
  KeywordEditorCocoaController* controller_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorModelObserver);
};

// This controller manages a window with a table view of search engines. It
// acts as |tableView_|'s data source and delegate, feeding it data from the
// KeywordEditorController's |table_model()|.

@interface KeywordEditorCocoaController : NSWindowController {
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* addButton_;
  IBOutlet NSButton* removeButton_;
  IBOutlet NSButton* makeDefaultButton_;

  Profile* profile_;  // weak
  scoped_ptr<KeywordEditorController> controller_;
  scoped_ptr<KeywordEditorModelObserver> observer_;
}
@property (readonly) KeywordEditorController* controller;

- (id)initWithProfile:(Profile*)profile;

// Message forwarded by KeywordEditorModelObserver.
- (void)modelChanged;

- (IBAction)addKeyword:(id)sender;
- (IBAction)deleteKeyword:(id)sender;
- (IBAction)makeDefault:(id)sender;

@end
