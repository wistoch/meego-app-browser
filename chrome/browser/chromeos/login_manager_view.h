// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "chrome/browser/chromeos/ipc_message.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "views/accelerator.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGER_VIEW_H_

class FilePath;

class LoginManagerView : public views::View,
                         public views::WindowDelegate,
                         public views::Textfield::Controller {
 public:
  LoginManagerView();
  virtual ~LoginManagerView();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();


  // Overridden from views::Textfield::Controller
  // Not thread-safe, by virtue of using SetupSession().
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);
  // This method is called whenever the text in the field changes.
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

  // Creates all examples and start UI event loop.
 private:
  views::Textfield* username_field_;
  views::Textfield* password_field_;
  views::Label* os_version_label_;

  // The dialog dimensions.
  gfx::Size dialog_dimensions_;

  GdkPixbuf* background_pixbuf_;
  GdkPixbuf* panel_pixbuf_;

  // Handles asynchronously loading the version.
  chromeos::VersionLoader loader_;

  // Used to request the version.
  CancelableRequestConsumer consumer_;

  // Helper functions to modularize class
  void BuildWindow();

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.
  // Returns true upon success and false on failure.
  bool Authenticate(const std::string& username,
                    const std::string& password);

  // Asynchronously launches the Chrome OS window manager.
  bool RunWindowManager(const std::string& username);

  // This is not threadsafe; as authentication is supposed to happen on the main
  // thread before any other threads are started, so this should be ok.
  // That said, the only reason we're not threadsafe right now is that we're
  // munging the CommandLine::ForCurrentProcess() to enable auto-client-side-ssl
  // for Googlers.  So, if we can do that differently to make this thread-safe,
  // that'd be A Good Thing (tm).
  void SetupSession(const std::string& username);

  // Callback from chromeos::VersionLoader giving the version.
  void OnOSVersion(chromeos::VersionLoader::Handle handle,
                   std::string version);

  DISALLOW_COPY_AND_ASSIGN(LoginManagerView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGER_VIEW_H_
