// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/usb_mount_observer.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/dom_ui/filebrowse_ui.h"
#include "chrome/browser/tab_contents/tab_contents.h"

namespace chromeos {

const char* kFilebrowseURLHash = "chrome://filebrowse#";
const char* kFilebrowseScanning = "scanningdevice";
const int kPopupLeft = 0;
const int kPopupTop = 0;
const int kPopupWidth = 250;
const int kPopupHeight = 300;

void USBMountObserver::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_CLOSED);
  for (BrowserIterator i = browsers_.begin(); i != browsers_.end();
       ++i) {
    if (Source<Browser>(source).ptr() == i->browser) {
      browsers_.erase(i);
      registrar_.Remove(this,
                        NotificationType::BROWSER_CLOSED,
                        source);
      return;
    }
  }
}

void USBMountObserver::OpenFileBrowse(const std::string& url,
                                      const std::string& device_path,
                                      bool small) {
  Browser *browser;
  if (small) {
    browser = FileBrowseUI::OpenPopup(profile_,
                                      url,
                                      FileBrowseUI::kSmallPopupWidth,
                                      FileBrowseUI::kSmallPopupHeight);
  } else {
    browser = FileBrowseUI::OpenPopup(profile_,
                                      url,
                                      FileBrowseUI::kPopupWidth,
                                      FileBrowseUI::kPopupHeight);
  }
  registrar_.Add(this,
                 NotificationType::BROWSER_CLOSED,
                 Source<Browser>(browser));
  BrowserWithPath new_browser;
  new_browser.browser = browser;
  new_browser.device_path = device_path;
  browsers_.push_back(new_browser);
}

void USBMountObserver::MountChanged(chromeos::MountLibrary* obj,
                                    chromeos::MountEventType evt,
                                    const std::string& path) {
  if (evt == chromeos::DISK_ADDED) {
    const chromeos::MountLibrary::DiskVector& disks = obj->disks();
    for (size_t i = 0; i < disks.size(); ++i) {
      chromeos::MountLibrary::Disk disk = disks[i];
      if (disk.device_path == path) {
        if (disk.is_parent) {
          if (!disk.has_media) {
            RemoveBrowserFromVector(disk.system_path);
          }
        } else if (!obj->MountPath(path.c_str())) {
          RemoveBrowserFromVector(disk.system_path);
        }
      }
    }
    LOG(INFO) << "Got added mount:" << path;
  } else if (evt == chromeos::DISK_REMOVED ||
             evt == chromeos::DEVICE_REMOVED) {
    RemoveBrowserFromVector(path);
  } else if (evt == chromeos::DISK_CHANGED) {
    BrowserIterator iter = FindBrowserForPath(path);
    LOG(INFO) << "Got changed mount:" << path;
    if (iter == browsers_.end()) {
      // We don't currently have this one, so it must have been
      // mounted
      const chromeos::MountLibrary::DiskVector& disks = obj->disks();
      for (size_t i = 0; i < disks.size(); ++i) {
        if (disks[i].device_path == path) {
          if (!disks[i].mount_path.empty()) {
            // Doing second search to see if the current disk has already
            // been popped up due to its parent device being plugged in.
            iter = FindBrowserForPath(disks[i].system_path);
            if (iter != browsers_.end()) {
              std::string url = kFilebrowseURLHash;
              url += disks[i].mount_path;
              TabContents* tab = iter->browser->GetSelectedTabContents();
              iter->browser->window()->SetBounds(gfx::Rect(
                  0, 0, FileBrowseUI::kPopupWidth, FileBrowseUI::kPopupHeight));
              tab->OpenURL(GURL(url), GURL(), CURRENT_TAB,
                  PageTransition::LINK);
              tab->NavigateToPendingEntry(NavigationController::RELOAD);
              iter->device_path = path;
            } else {
              OpenFileBrowse(disks[i].mount_path, disks[i].device_path, false);
            }
          }
          return;
        }
      }
    }
  } else if (evt == chromeos::DEVICE_ADDED) {
    LOG(INFO) << "Got device added" << path;
    OpenFileBrowse(kFilebrowseScanning, path, true);
  } else if (evt == chromeos::DEVICE_SCANNED) {
    LOG(INFO) << "Got device scanned:" << path;
  }
}

USBMountObserver::BrowserIterator USBMountObserver::FindBrowserForPath(
    const std::string& path) {
  for (BrowserIterator i = browsers_.begin();i != browsers_.end();
       ++i) {
    // Doing a substring match so that we find if this new one is a subdevice
    // of another already inserted device.
    if (path.find(i->device_path) != std::string::npos) {
      return i;
    }
  }
  return browsers_.end();
}

void USBMountObserver::RemoveBrowserFromVector(const std::string& path) {
  BrowserIterator i = FindBrowserForPath(path);
  if (i != browsers_.end()) {
    registrar_.Remove(this,
                      NotificationType::BROWSER_CLOSED,
                      Source<Browser>(i->browser));
    if (i->browser->window()) {
      i->browser->window()->Close();
    }
    browsers_.erase(i);
  }
}

} // namespace chromeos
