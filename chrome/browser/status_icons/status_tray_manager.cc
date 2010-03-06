// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray_manager.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"

class StatusIconFactoryImpl : public StatusIconFactory {
 public:
  virtual StatusIcon* CreateIcon();
};

StatusIcon* StatusIconFactoryImpl::CreateIcon() {
#ifdef OS_MACOSX
  return StatusIcon::Create();
#else
  // TODO(atwilson): Add support for non-Mac platforms.
  return 0;
#endif
}


StatusTrayManager::StatusTrayManager() {
}

StatusTrayManager::~StatusTrayManager() {
}

void StatusTrayManager::Init(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  status_tray_.reset(new StatusTray(new StatusIconFactoryImpl()));
  StatusIcon* icon = status_tray_->GetStatusIcon(ASCIIToUTF16("chrome_main"));
  if (icon) {
    // Create an icon and add ourselves as a click observer on it
    SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUS_TRAY_ICON);
    icon->SetImage(*bitmap);
    icon->AddObserver(this);
  }
}

void StatusTrayManager::OnClicked() {
  // When the tray icon is clicked, bring up the extensions page for now.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser) {
    // Bring up the existing browser window and show the extensions tab.
    browser->window()->Activate();
    browser->ShowExtensionsTab();
  } else {
    // No windows are currently open, so open a new one.
    Browser::OpenExtensionsWindow(profile_);
  }
}
