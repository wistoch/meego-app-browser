// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_

#include "chrome/browser/chromeos/status/status_area_host.h"
#include "views/view.h"

namespace views {
class Widget;
}

namespace chromeos {

class StatusAreaView;

// View used to render the background during login. BackgroundView contains
// StatusAreaView.
class BackgroundView : public views::View, public StatusAreaHost {
 public:
  BackgroundView();

  // Creates a window containing an instance of WizardContentsView as the root
  // view. The caller is responsible for showing (and closing) the returned
  // widget. The BackgroundView is set in |view|.
  static views::Widget* CreateWindowContainingView(const gfx::Rect& bounds,
                                                   BackgroundView** view);

  // Deletes the current status area and adds a new one.
  void RecreateStatusArea();

  // Overridden from views::View:
  virtual void Layout();

  // Overridden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;

 private:
  // Creates and adds the status_area.
  void InitStatusArea();

  StatusAreaView* status_area_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
