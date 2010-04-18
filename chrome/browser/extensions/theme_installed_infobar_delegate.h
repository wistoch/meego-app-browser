// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/notification_registrar.h"

class Extension;
class SkBitmap;
class TabContents;

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemeInstalledInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public NotificationObserver {
 public:
  ThemeInstalledInfoBarDelegate(TabContents* tab_contents,
                                const Extension* new_theme,
                                const std::string& previous_theme_id);
  virtual ~ThemeInstalledInfoBarDelegate();
  virtual void InfoBarClosed();
  virtual std::wstring GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
  virtual int GetButtons() const;
  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const;
  virtual bool Cancel();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 protected:
  Profile* profile() { return profile_; }

 private:
  Profile* profile_;

  // Name of theme that's just been installed.
  std::string name_;

  // ID of theme that's just been installed.
  std::string theme_id_;

  // Used to undo theme install.
  std::string previous_theme_id_;

  // Tab to which this info bar is associated.
  TabContents* tab_contents_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
