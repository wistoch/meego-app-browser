// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_GTK_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_GTK_THEME_INSTALLED_INFOBAR_DELEGATE_H_

#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"

#include <string>

class SkBitmap;
class TabContents;

// A specialization of ThemeInstalledInfoBarDelegate to make "Undo" reset to the
// GTK theme if the user was in GTK theme mode before installing the theme.
class GtkThemeInstalledInfoBarDelegate : public ThemeInstalledInfoBarDelegate {
 public:
  GtkThemeInstalledInfoBarDelegate(TabContents* tab_contents,
                                   const std::string& name,
                                   const std::string& previous_theme,
                                   bool previous_use_gtk_theme);
  virtual bool Cancel();

 private:
  bool previous_use_gtk_theme_;
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_GTK_THEME_INSTALLED_INFOBAR_DELEGATE_H_
