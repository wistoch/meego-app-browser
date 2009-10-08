// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TASK_MANAGER_MAC_H_
#define CHROME_BROWSER_COCOA_TASK_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"
#include "chrome/browser/task_manager.h"

// This class is responsible for loading the task manager window and for
// managing it.
@interface TaskManagerWindowController : NSWindowController {
 @private
  IBOutlet NSTableView* tableView_;
  TaskManagerModel* model_;  // weak
}

// Creates and shows the task manager's window.
- (id)initWithModel:(TaskManagerModel*)model;

// Refreshes all data in the task manager table.
- (void)reloadData;

- (IBAction)statsLinkClicked:(id)sender;
@end

// This class listens to task changed events sent by chrome.
class TaskManagerMac : public TaskManagerModelObserver {
 public:
  TaskManagerMac();
  virtual ~TaskManagerMac();

  // TaskManagerModelObserver
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static void Show();

 private:
  // The task manager.
  TaskManager* task_manager_;  // weak

  // Our model.
  TaskManagerModel* model_;  // weak

  // Controller of our window.
  scoped_nsobject<TaskManagerWindowController> window_controller_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerMac* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerMac);
};

#endif  // CHROME_BROWSER_COCOA_TASK_MANAGER_MAC_H_
