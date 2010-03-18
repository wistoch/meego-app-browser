// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"

class Browser;
class Extension;
class ExtensionHost;
class TabContents;

// The InfobarDelegate for creating and managing state for the ExtensionInfobar
// plus monitor when the extension goes away.
class ExtensionInfoBarDelegate : public InfoBarDelegate,
                                 public NotificationObserver {
 public:
  ExtensionInfoBarDelegate(Browser* browser, TabContents* contents,
                           Extension* extension, const GURL& url);
  ~ExtensionInfoBarDelegate();

  Extension* extension() { return extension_; }
  ExtensionHost* extension_host() { return extension_host_.get(); }

  // Overridden from InfoBarDelegate:
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;
  virtual void InfoBarClosed();
  virtual InfoBar* CreateInfoBar();
  virtual ExtensionInfoBarDelegate* AsExtensionInfoBarDelegate() {
    return this;
  }
  virtual Type GetInfoBarType() {
    return PAGE_ACTION_TYPE;
  }

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // The extension host we are showing the InfoBar for. The delegate needs to
  // own this since the InfoBar gets deleted and recreated when you switch tabs
  // and come back (and we don't want the user's interaction with the InfoBar to
  // get lost at that point.
  scoped_ptr<ExtensionHost> extension_host_;

  Extension* extension_;

  TabContents* tab_contents_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFOBAR_DELEGATE_H_
