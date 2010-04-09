// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/repost_form_warning_controller.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"

RepostFormWarningController::RepostFormWarningController(
    TabContents* tab_contents)
    : tab_contents_(tab_contents),
      window_(NULL) {
  NavigationController* controller = &tab_contents->controller();
  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(controller));
  registrar_.Add(this, NotificationType::REPOST_WARNING_SHOWN,
                 Source<NavigationController>(controller));
}

RepostFormWarningController::~RepostFormWarningController() {
}

void RepostFormWarningController::Show(
    ConstrainedWindowDelegate* window_delegate) {
  window_ = tab_contents_->CreateConstrainedDialog(window_delegate);
}

void RepostFormWarningController::Cancel() {
  if (tab_contents_) {
    tab_contents_->controller().CancelPendingReload();
    CloseDialog();
  }
}

void RepostFormWarningController::Continue() {
  if (tab_contents_) {
    tab_contents_->controller().ContinuePendingReload();
    // If we reload the page, the dialog will be closed anyway.
  }
}

void RepostFormWarningController::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if ((type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING ||
       type == NotificationType::REPOST_WARNING_SHOWN)) {
    DCHECK_EQ(Source<NavigationController>(source).ptr(),
              &tab_contents_->controller());
    Cancel();
  }
}

void RepostFormWarningController::CloseDialog() {
  tab_contents_ = NULL;
  if (window_) {
    window_->CloseConstrainedWindow();
  }
  delete this;
}
