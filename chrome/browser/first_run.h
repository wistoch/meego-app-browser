// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_H__
#define CHROME_BROWSER_FIRST_RUN_H__

#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process_impl.h"

class Profile;

// This class contains the chrome first-run installation actions needed to
// fully test the custom installer. It also contains the opposite actions to
// execute during uninstall. When the first run UI is ready we won't
// do the actions unconditionally. Currently the only action is to create a
// desktop shortcut.
//
// The way we detect first-run is by looking at a 'sentinel' file.
// If it does not exists we understand that we need to do the first time
// install work for this user. After that the sentinel file is created.
class FirstRun {
 public:
  // Returns true if this is the first time chrome is run for this user.
  static bool IsChromeFirstRun();
  // Creates the desktop shortcut to chrome for the current user. Returns
  // false if it fails. It will overwrite the shortcut if it exists.
  static bool CreateChromeDesktopShortcut();
  // Creates the quick launch shortcut to chrome for the current user. Returns
  // false if it fails. It will overwrite the shortcut if it exists.
  static bool CreateChromeQuickLaunchShortcut();
  // Creates the sentinel file that signals that chrome has been configured.
  static bool CreateSentinel();
  // Removes the desktop shortcut to chrome. Returns false if it could not
  // be removed.
  static bool RemoveChromeDesktopShortcut();
  // Removes the quick launch shortcut to chrome. Returns false if it could not
  // be removed.
  static bool RemoveChromeQuickLaunchShortcut();
  // Removes the sentinel file created in ConfigDone(). Returns false if the
  // sentinel file could not be removed.
  static bool RemoveSentinel();
  // Imports settings in a separate process. It spawns a second dedicated
  // browser process that just does the import with the import progress UI.
  static bool ImportSettings(Profile* profile, int browser,
                             int items_to_import, HWND parent_window);
  // Import browser items with a progress UI. The browser and the items to
  // import are encoded int the command line. This function is paired with
  // FirstRun::ImportSettings().
  static int ImportWithUI(Profile* profile, const CommandLine& cmdline);

 private:
  // This class is for scoping purposes.
  DISALLOW_EVIL_CONSTRUCTORS(FirstRun);
};

// This class contains the actions that need to be performed when an upgrade
// is required. This involves mainly swapping the chrome exe and relaunching
// the new browser.
class Upgrade {
 public:
  // If the new_chrome.exe exists (placed by the installer then is swapped
  // to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
  // is no new_chrome.exe or the swap fails the return is false;
  static bool SwapNewChromeExeIfPresent();
  // Launches chrome again simulating a 'user' launch. If chrome could not
  // be launched the return is false.
  static bool RelaunchChromeBrowser(const CommandLine& command_line);
};

// A subclass of BrowserProcessImpl that does not have a GoogleURLTracker
// so we don't fetch as we have no IO thread (see bug #1292702).
class FirstRunBrowserProcess : public BrowserProcessImpl {
 public:
  FirstRunBrowserProcess(CommandLine& command_line)
      : BrowserProcessImpl(command_line) {
  }
  virtual ~FirstRunBrowserProcess() { }

  virtual GoogleURLTracker* google_url_tracker() { return NULL; }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FirstRunBrowserProcess);
};

// Show the First Run UI to the user, allowing them to create shortcuts for
// the app, import their bookmarks and other data from another browser into
// |profile| and perhaps some other tasks.
void OpenFirstRunDialog(Profile* profile);

#endif  // CHROME_BROWSER_FIRST_RUN_H__

