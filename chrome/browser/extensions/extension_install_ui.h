// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "gfx/native_widget_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

#include <string>

class Extension;
class ExtensionsService;
class MessageLoop;
class Profile;
class InfoBarDelegate;
class SandboxedExtensionUnpacker;
class TabContents;

// Displays all the UI around extension installation and uninstallation.
class ExtensionInstallUI {
 public:
  enum PromptType {
    INSTALL_PROMPT = 0,
    UNINSTALL_PROMPT,
    ENABLE_INCOGNITO_PROMPT,
    NUM_PROMPT_TYPES
  };

  // A mapping from PromptType to message ID for various dialog content.
  static const int kTitleIds[NUM_PROMPT_TYPES];
  static const int kHeadingIds[NUM_PROMPT_TYPES];
  static const int kButtonIds[NUM_PROMPT_TYPES];

  class Delegate {
   public:
    // We call this method after ConfirmInstall()/ConfirmUninstall() to signal
    // that the installation/uninstallation should continue.
    virtual void InstallUIProceed(bool create_app_shortcut) = 0;

    // We call this method after ConfirmInstall()/ConfirmUninstall() to signal
    // that the installation/uninstallation should stop.
    virtual void InstallUIAbort() = 0;
  };

  explicit ExtensionInstallUI(Profile* profile);

  virtual ~ExtensionInstallUI() {}

  // This is called by the installer to verify whether the installation should
  // proceed. This is declared virtual for testing.
  //
  // We *MUST* eventually call either Proceed() or Abort()
  // on |delegate|.
  virtual void ConfirmInstall(Delegate* delegate, Extension* extension,
                              SkBitmap* icon);

  // This is called by the extensions management page to verify whether the
  // uninstallation should proceed. This is declared virtual for testing.
  //
  // We *MUST* eventually call either Proceed() or Abort()
  // on |delegate|.
  virtual void ConfirmUninstall(Delegate* delegate, Extension* extension,
                                SkBitmap* icon);

  // This is called by the extensions management page to verify whether the
  // uninstallation should proceed. This is declared virtual for testing.
  //
  // We *MUST* eventually call either Proceed() or Abort()
  // on |delegate|.
  virtual void ConfirmEnableIncognito(Delegate* delegate, Extension* extension,
                                      SkBitmap* icon);

  // Installation was successful. This is declared virtual for testing.
  virtual void OnInstallSuccess(Extension* extension);

  // Installation failed. This is declared virtual for testing.
  virtual void OnInstallFailure(const std::string& error);

  // The install was rejected because the same extension/version is already
  // installed. This is declared virtual for testing.
  virtual void OnOverinstallAttempted(Extension* extension);

 private:
  // When a Theme is downloaded it is applied and an info bar is shown to give
  // the user a choice to keep it or undo the installation.
  void ShowThemeInfoBar(Extension* new_theme);

#if defined(OS_MACOSX)
  // When an extension is installed on Mac with neither browser action nor
  // page action icons, show an infobar instead of a popup bubble.
  void ShowGenericExtensionInstalledInfoBar(Extension* new_extension);
#endif

  // Returns the delegate to control the browser's info bar. This is within its
  // own function due to its platform-specific nature.
  InfoBarDelegate* GetNewInfoBarDelegate(Extension* new_theme,
                                         TabContents* tab_contents);

  // Implements the showing of the install/uninstall dialog prompt.
  // NOTE: The implementations of this function is platform-specific.
  static void ShowExtensionInstallUIPromptImpl(
      Profile* profile, Delegate* delegate, Extension* extension,
      SkBitmap* icon, const string16& warning_text, PromptType type);

  Profile* profile_;
  MessageLoop* ui_loop_;
  std::string previous_theme_id_;  // Used to undo theme installation.
  SkBitmap icon_;  // The extensions installation icon.

#if defined(TOOLKIT_GTK)
  // Also needed to undo theme installation in the linux UI.
  bool previous_use_gtk_theme_;
#endif
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
