// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPOST_FORM_WARNING_H__
#define CHROME_BROWSER_REPOST_FORM_WARNING_H__

#include "chrome/common/notification_service.h"
#include "chrome/views/dialog_delegate.h"

class MessageBoxView;
class NavigationController;
namespace ChromeViews {
class Window;
}

class RepostFormWarningDialog : public ChromeViews::DialogDelegate,
                                public NotificationObserver {
 public:
  // Creates and runs a message box which asks the user if they want to resend
  // an HTTP POST.
  static void RunRepostFormWarningDialog(
      NavigationController* navigation_controller);
  virtual ~RepostFormWarningDialog();

  // ChromeViews::DialogDelegate Methods:
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual bool Accept();

  // ChromeViews::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }
  virtual ChromeViews::View* GetContentsView();

 private:
  // Use RunRepostFormWarningDialog to use.
  RepostFormWarningDialog(NavigationController* navigation_controller);

  // NotificationObserver implementation.
  // Watch for a new load or a closed tab and dismiss the dialog if they occur.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  // Navigation controller, used to continue the reload.
  NavigationController* navigation_controller_;

  DISALLOW_EVIL_CONSTRUCTORS(RepostFormWarningDialog);
};

#endif // CHROME_BROWSER_REPOST_FORM_WARNING_H__

