// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {

class Checkbox;
class Label;
class Link;
class NativeButton;

}  // namespace views

class DOMView;

namespace chromeos {

// Delegate for TabContents that will show EULA.
// Blocks context menu and other actions.
class EULATabContentsDelegate : public TabContentsDelegate {
 public:
  EULATabContentsDelegate() {}
  virtual ~EULATabContentsDelegate() {}

 protected:
  // TabContentsDelegate implementation:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool ShouldAddNavigationToHistory() const { return false; }
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EULATabContentsDelegate);
};

class EulaView
    : public views::View,
      public views::ButtonListener,
      public views::LinkController,
      public EULATabContentsDelegate {
 public:
  explicit EulaView(chromeos::ScreenObserver* observer);
  virtual ~EulaView();

  // Initialize view controls and layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

 protected:
  // views::View implementation.
  virtual void OnLocaleChanged();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController implementation.
  void LinkActivated(views::Link* source, int event_flags);

 private:
  // TabContentsDelegate implementation.
  virtual void NavigationStateChanged(const TabContents* contents,
                                      unsigned changed_flags);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Loads specified URL to the specified DOMView and updates specified
  // label with its title.
  void LoadEulaView(DOMView* eula_view,
                    views::Label* eula_label,
                    const GURL& eula_url);

  // Dialog controls.
  views::Label* google_eula_label_;
  DOMView* google_eula_view_;
  views::Checkbox* usage_statistics_checkbox_;
  views::Link* learn_more_link_;
  views::Label* oem_eula_label_;
  DOMView* oem_eula_view_;
  views::Link* system_security_settings_link_;
  views::NativeButton* cancel_button_;
  views::NativeButton* continue_button_;

  chromeos::ScreenObserver* observer_;

  GURL oem_eula_page_;

  BooleanPrefMember metrics_reporting_enabled_;

  DISALLOW_COPY_AND_ASSIGN(EulaView);
};

class EulaScreen : public DefaultViewScreen<EulaView> {
 public:
  explicit EulaScreen(WizardScreenDelegate* delegate)
      : DefaultViewScreen<EulaView>(delegate) {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
