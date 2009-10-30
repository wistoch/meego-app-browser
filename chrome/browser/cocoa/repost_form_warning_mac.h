// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_REPOST_FORM_WARNING_MAC_H_
#define CHROME_BROWSER_COCOA_REPOST_FORM_WARNING_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/common/notification_registrar.h"

class NavigationController;
class RepostFormWarningMac;

@interface RepostDelegate : NSObject {
  RepostFormWarningMac* warning_;  // weak, owns us.
}
- (id)initWithWarning:(RepostFormWarningMac*)warning;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

class RepostFormWarningMac : public NotificationObserver {
 public:
  RepostFormWarningMac(NSWindow* parent,
                       NavigationController* navigation_controller);
  virtual ~RepostFormWarningMac();

  void Confirm();
  void Cancel();

 private:
  // NotificationObserver implementation.
  // Watch for a new load or a closed tab and dismiss the dialog if they occur.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Close the sheet.  This will only be done once, even if Destroy is called
  // multiple times (eg, from both Confirm and Observe.)
  void Destroy();

  NotificationRegistrar registrar_;

  // Navigation controller, used to continue the reload.
  NavigationController* navigation_controller_;

  scoped_nsobject<NSAlert> alert_;
  
  scoped_nsobject<RepostDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningMac);
};

#endif  // CHROME_BROWSER_COCOA_REPOST_FORM_WARNING_MAC_H_
