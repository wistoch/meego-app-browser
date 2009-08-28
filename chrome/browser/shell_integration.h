// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_H__
#define CHROME_BROWSER_SHELL_INTEGRATION_H__

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

class FilePath;
class MessageLoop;

class ShellIntegration {
 public:
  // Sets Chrome as default browser (only for current user). Returns false if
  // this operation fails.
  static bool SetAsDefaultBrowser();

  // Returns true if this instance of Chrome is the default browser. (Defined
  // as being the handler for the http/https protocols... we don't want to
  // report false here if the user has simply chosen to open HTML files in a
  // text editor and ftp links with a FTP client).
  static bool IsDefaultBrowser();

  // Returns true if Firefox is likely to be the default browser for the current
  // user. This method is very fast so it can be invoked in the UI thread.
  static bool IsFirefoxDefaultBrowser();

#if defined(OS_LINUX)
  // Returns filename for .desktop file based on |url|, sanitized for security.
  static FilePath GetDesktopShortcutFilename(const GURL& url);

  // Returns contents for .desktop file based on |template_contents|, |url|
  // and |title|. The |template_contents| should be contents of .desktop file
  // used to launch Chrome.
  static std::string GetDesktopFileContents(
      const std::string& template_contents, const GURL& url,
      const string16& title);

  struct ShortcutInfo {
    GURL url;
    string16 title;

    bool create_on_desktop;
    bool create_in_applications_menu;
  };

  // Creates a desktop shortcut. It is not guaranteed to exist immediately after
  // returning from this function, because actual file operation is done on the
  // file thread.
  static void CreateDesktopShortcut(const ShortcutInfo& shortcut_info);
#endif  // defined(OS_LINUX)

  // The current default browser UI state
  enum DefaultBrowserUIState {
    STATE_PROCESSING,
    STATE_DEFAULT,
    STATE_NOT_DEFAULT
  };

  class DefaultBrowserObserver {
   public:
    // Updates the UI state to reflect the current default browser state.
    virtual void SetDefaultBrowserUIState(DefaultBrowserUIState state) = 0;
    virtual ~DefaultBrowserObserver() {}
  };
  //  A helper object that handles checking if Chrome is the default browser on
  //  Windows and also setting it as the default browser. These operations are
  //  performed asynchronously on the file thread since registry access is
  //  involved and this can be slow.
  //
  class DefaultBrowserWorker
      : public base::RefCountedThreadSafe<DefaultBrowserWorker> {
   public:
    explicit DefaultBrowserWorker(DefaultBrowserObserver* observer);
    virtual ~DefaultBrowserWorker() {}

    // Checks if Chrome is the default browser.
    void StartCheckDefaultBrowser();

    // Sets Chrome as the default browser.
    void StartSetAsDefaultBrowser();

    // Called to notify the worker that the view is gone.
    void ObserverDestroyed();

   private:
    // Functions that track the process of checking if Chrome is the default
    // browser.  |ExecuteCheckDefaultBrowser| checks the registry on the file
    // thread.  |CompleteCheckDefaultBrowser| notifies the view to update on the
    // UI thread.
    void ExecuteCheckDefaultBrowser();
    void CompleteCheckDefaultBrowser(bool is_default);

    // Functions that track the process of setting Chrome as the default
    // browser.  |ExecuteSetAsDefaultBrowser| updates the registry on the file
    // thread.  |CompleteSetAsDefaultBrowser| notifies the view to update on the
    // UI thread.
    void ExecuteSetAsDefaultBrowser();
    void CompleteSetAsDefaultBrowser();

    // Updates the UI in our associated view with the current default browser
    // state.
    void UpdateUI(bool is_default);

    DefaultBrowserObserver* observer_;

    MessageLoop* ui_loop_;
    MessageLoop* file_loop_;

    DISALLOW_COPY_AND_ASSIGN(DefaultBrowserWorker);
  };
};

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_H__
