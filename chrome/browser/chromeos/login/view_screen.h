// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_

#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"

template <class V>
class ViewScreen : public WizardScreen {
 public:
  explicit ViewScreen(WizardScreenDelegate* delegate);
  virtual ~ViewScreen();

  // Overridden from WizardScreen:
  virtual void Show();
  virtual void Hide();

  V* view() { return view_; }

 protected:
  // Creates view object and adds it to views hierarchy.
  virtual void CreateView();
  // Creates view object.
  virtual V* AllocateView() = 0;

 private:
  // For testing automation
  friend class AutomationProvider;

  V* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewScreen);
};

template <class V>
class DefaultViewScreen : public ViewScreen<V> {
 public:
  explicit DefaultViewScreen(WizardScreenDelegate* delegate)
      : ViewScreen<V>(delegate) {}
  V* AllocateView() {
    return new V(ViewScreen<V>::delegate()->GetObserver(this));
  }
};

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, public:
template <class V>
ViewScreen<V>::ViewScreen(WizardScreenDelegate* delegate)
    : WizardScreen(delegate),
      view_(NULL) {
}

template <class V>
ViewScreen<V>::~ViewScreen() {
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, WizardScreen implementation:
template <class V>
void ViewScreen<V>::Show() {
  if (!view_) {
    CreateView();
  }
  view_->SetVisible(true);
  // After view is initialized and shown refresh it's state.
  view_->Refresh();
}

template <class V>
void ViewScreen<V>::Hide() {
  if (view_) {
    delegate()->GetWizardView()->RemoveChildView(view_);
    // RemoveChildView doesn't delete the view and we also can't delete it here
    // becuase we are in message processing for the view.
    MessageLoop::current()->DeleteSoon(FROM_HERE, view_);
    view_ = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, protected:
template <class V>
void ViewScreen<V>::CreateView() {
  view_ = AllocateView();
  delegate()->GetWizardView()->AddChildView(view_);
  view_->Init();
  view_->SetVisible(false);
}

typedef DefaultViewScreen<chromeos::LoginManagerView> LoginScreen;
typedef DefaultViewScreen<chromeos::NetworkSelectionView> NetworkScreen;

class UpdateScreen: public DefaultViewScreen<chromeos::UpdateView> {
 public:
  explicit UpdateScreen(WizardScreenDelegate* delegate)
      : DefaultViewScreen<chromeos::UpdateView>(delegate) {
  }
  void StartUpdate() { view()->StartUpdate(); }
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
