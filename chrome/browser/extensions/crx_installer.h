// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_

#include <string>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/common/extensions/extension.h"

class SkBitmap;

// This class installs a crx file into a profile.
//
// Installing a CRX is a multi-step process, including unpacking the crx,
// validating it, prompting the user, and installing. Since many of these
// steps must occur on the file thread, this class contains a copy of all data
// necessary to do its job. (This also minimizes external dependencies for
// easier testing).
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of unpacking.
//
// TODO(aa): Pull out a frontend interface for testing?
class CrxInstaller :
    public SandboxedExtensionUnpackerClient,
    public ExtensionInstallUI::Delegate {
 public:
  // Starts the installation of the crx file in |crx_path| into
  // |install_directory|.
  //
  // Other params:
  //  install_source: The source of the install (external, --load-extension, etc
  //  expected_id: Optional. If the caller knows what the ID of this extension
  //               should be after unpacking, it can be specified here as a
  //               sanity check.
  //  delete_crx: Whether the crx should be deleted on completion.
  //  file_loop: The message loop to do file IO on.
  //  frontend: The ExtensionsService to report the successfully installed
  //            extension to.
  //  client: Optional. If specified, will be used to confirm installation and
  //          also notified of success/fail. Note that we hold a reference to
  //          this, so it can outlive its creator (eg the UI).
  static void Start(const FilePath& crx_path,
                    const FilePath& install_directory,
                    Extension::Location install_source,
                    const std::string& expected_id,
                    bool delete_crx,
                    MessageLoop* file_loop,
                    ExtensionsService* frontend,
                    ExtensionInstallUI* client);

  // Given the path to the large icon from an extension, read it if present and
  // decode it into result.
  static void DecodeInstallIcon(const FilePath& large_icon_path,
                                scoped_ptr<SkBitmap>* result);

  // ExtensionInstallUI::Delegate
  virtual void ContinueInstall();
  virtual void AbortInstall();

 private:
  CrxInstaller(const FilePath& crx_path,
               const FilePath& install_directory,
               Extension::Location install_source,
               const std::string& expected_id,
               bool delete_crx,
               MessageLoop* file_loop,
               ExtensionsService* frontend,
               ExtensionInstallUI* client);
  ~CrxInstaller();

  // SandboxedExtensionUnpackerClient
  virtual void OnUnpackFailure(const std::string& error_message);
  virtual void OnUnpackSuccess(const FilePath& temp_dir,
                               const FilePath& extension_dir,
                               Extension* extension);

  // Runs on the UI thread. Confirms with the user (via ExtensionInstallUI) that
  // it is OK to install this extension.
  void ConfirmInstall();

  // Runs on File thread. Install the unpacked extension into the profile and
  // notify the frontend.
  void CompleteInstall();

  // Result reporting.
  void ReportFailureFromFileThread(const std::string& error);
  void ReportFailureFromUIThread(const std::string& error);
  void ReportOverinstallFromFileThread();
  void ReportOverinstallFromUIThread();
  void ReportSuccessFromFileThread();
  void ReportSuccessFromUIThread();

  // The crx file we're installing.
  FilePath crx_path_;

  // The directory extensions are installed to.
  FilePath install_directory_;

  // The location the installation came from (bundled with Chromium, registry,
  // manual install, etc). This metadata is saved with the installation if
  // successful.
  Extension::Location install_source_;

  // For updates and external installs we have an ID we're expecting the
  // extension to contain.
  std::string expected_id_;

  // Whether manual extension installation is enabled. We can't just check this
  // before trying to install because themes are special-cased to always be
  // allowed.
  bool extensions_enabled_;

  // Whether we're supposed to delete the source crx file on destruction.
  bool delete_crx_;

  // The message loop to use for file IO.
  MessageLoop* file_loop_;

  // The message loop the UI is running on.
  MessageLoop* ui_loop_;

  // The extension we're installing. We own this and either pass it off to
  // ExtensionsService on success, or delete it on failure.
  scoped_ptr<Extension> extension_;

  // If non-empty, contains the current version of the extension we're
  // installing (for upgrades).
  std::string current_version_;

  // The icon we will display in the installation UI, if any.
  scoped_ptr<SkBitmap> install_icon_;

  // The temp directory extension resources were unpacked to. We own this and
  // must delete it when we are done with it.
  FilePath temp_dir_;

  // The frontend we will report results back to.
  scoped_refptr<ExtensionsService> frontend_;

  // The client we will work with to do the installation. This can be NULL, in
  // which case the install is silent.
  scoped_ptr<ExtensionInstallUI> client_;

  // The root of the unpacked extension directory. This is a subdirectory of
  // temp_dir_, so we don't have to delete it explicitly.
  FilePath unpacked_extension_root_;

  // The unpacker we will use to unpack the extension.
  SandboxedExtensionUnpacker* unpacker_;

  DISALLOW_COPY_AND_ASSIGN(CrxInstaller);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
