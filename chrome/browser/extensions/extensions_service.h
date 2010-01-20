// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/time.h"
#include "base/tuple.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/extensions/extension.h"

class Browser;
class ExtensionsServiceBackend;
class ExtensionToolbarModel;
class ExtensionUpdater;
class GURL;
class PrefService;
class Profile;
class ResourceDispatcherHost;
class SiteInstance;

// This is an interface class to encapsulate the dependencies that
// ExtensionUpdater has on ExtensionsService. This allows easy mocking.
class ExtensionUpdateService {
 public:
  virtual ~ExtensionUpdateService() {}
  virtual const ExtensionList* extensions() const = 0;
  virtual void UpdateExtension(const std::string& id, const FilePath& path) = 0;
  virtual Extension* GetExtensionById(const std::string& id,
                                      bool include_disabled) = 0;
  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) = 0;
  virtual bool HasInstalledExtensions() = 0;
};

// Manages installed and running Chromium extensions.
class ExtensionsService
    : public base::RefCountedThreadSafe<ExtensionsService,
                                        ChromeThread::DeleteOnUIThread>,
      public ExtensionUpdateService,
      public NotificationObserver {
 public:
  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

  // If auto-updates are turned on, default to running every 5 hours.
  static const int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

  // The name of the file that the current active version number is stored in.
  static const char* kCurrentVersionFileName;

  // Determine if a given extension download should be treated as if it came
  // from the gallery.
  static bool IsDownloadFromGallery(const GURL& download_url,
                                    const GURL& referrer_url);

  // Determine if the downloaded extension came from the theme mini-gallery,
  // Used to test if we need to show the "Loading" dialog for themes.
  static bool IsDownloadFromMiniGallery(const GURL& download_url);

  ExtensionsService(Profile* profile,
                    const CommandLine* command_line,
                    PrefService* prefs,
                    const FilePath& install_directory,
                    bool autoupdate_enabled);

  // Gets the list of currently installed extensions.
  virtual const ExtensionList* extensions() const { return &extensions_; }
  virtual const ExtensionList* disabled_extensions() const {
    return &disabled_extensions_;
  }

  // Returns true if any extensions are installed.
  virtual bool HasInstalledExtensions() {
    return !(extensions_.empty() && disabled_extensions_.empty());
  }

  const FilePath& install_directory() const { return install_directory_; }

  // Initialize and start all installed extensions.
  void Init();

  // Look up an extension by ID.
  Extension* GetExtensionById(const std::string& id, bool include_disabled) {
    return GetExtensionByIdInternal(id, true, include_disabled);
  }

  // Install the extension file at |extension_path|.  Will install as an
  // update if an older version is already installed.
  // For fresh installs, this method also causes the extension to be
  // immediately loaded.
  // TODO(aa): This method can be removed. It is only used by the unit tests,
  // and they could use CrxInstaller directly instead.
  void InstallExtension(const FilePath& extension_path);

  // Updates a currently-installed extension with the contents from
  // |extension_path|.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual void UpdateExtension(const std::string& id,
                               const FilePath& extension_path);

  // Reloads the specified extension.
  void ReloadExtension(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |external_uninstall| is a magical parameter
  // that is only used to send information to ExtensionPrefs, which external
  // callers should never set to true.
  // TODO(aa): Remove |external_uninstall| -- this information should be passed
  // to ExtensionPrefs some other way.
  void UninstallExtension(const std::string& extension_id,
                          bool external_uninstall);

  // Enable or disable an extension. The extension must be in the opposite state
  // before calling.
  void EnableExtension(const std::string& extension_id);
  void DisableExtension(const std::string& extension_id);

  // Load the extension from the directory |extension_path|.
  void LoadExtension(const FilePath& extension_path);

  // Load all known extensions (used by startup and testing code).
  void LoadAllExtensions();

  // Continues loading all know extensions. It can be called from
  // LoadAllExtensions or from file thread if we had to relocalize manifest
  // (write_to_prefs is true in that case).
  void ContinueLoadAllExtensions(ExtensionPrefs::ExtensionsInfo* info,
                                 base::TimeTicks start_time,
                                 bool write_to_prefs);

  // Check for updates (or potentially new extensions from external providers)
  void CheckForExternalUpdates();

  // Unload the specified extension.
  void UnloadExtension(const std::string& extension_id);

  // Unload all extensions. This is currently only called on shutdown, and
  // does not send notifications.
  void UnloadAllExtensions();

  // Called only by testing.
  void ReloadExtensions();

  // Scan the extension directory and clean up the cruft.
  void GarbageCollectExtensions();

  // Lookup an extension by |url|.  This uses the host of the URL as the id.
  Extension* GetExtensionByURL(const GURL& url);

  // Clear all ExternalExtensionProviders.
  void ClearProvidersForTesting();

  // Sets an ExternalExtensionProvider for the service to use during testing.
  // |location| specifies what type of provider should be added.
  void SetProviderForTesting(Extension::Location location,
                             ExternalExtensionProvider* test_provider);

  // Called when the initial extensions load has completed.
  virtual void OnLoadedInstalledExtensions();

  // Called by the backend when an extension has been loaded.
  void OnExtensionLoaded(Extension* extension,
                         bool allow_privilege_increase);

  // Called by the backend when an extension has been installed.
  void OnExtensionInstalled(Extension* extension,
                            bool allow_privilege_increase);

  // Called by the backend when an attempt was made to reinstall the same
  // version of an existing extension.
  void OnExtensionOverinstallAttempted(const std::string& id);

  // Called by the backend when an external extension is found.
  void OnExternalExtensionFound(const std::string& id,
                                const std::string& version,
                                const FilePath& path,
                                Extension::Location location);

  // Go through each extensions in pref, unload blacklisted extensions
  // and update the blacklist state in pref.
  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist);

  void set_extensions_enabled(bool enabled) { extensions_enabled_ = enabled; }
  bool extensions_enabled() { return extensions_enabled_; }

  void set_show_extensions_prompts(bool enabled) {
    show_extensions_prompts_ = enabled;
  }

  bool show_extensions_prompts() {
    return show_extensions_prompts_;
  }

  Profile* profile() { return profile_; }

  // Profile calls this when it is destroyed so that we know not to call it.
  void ProfileDestroyed() { profile_ = NULL; }

  ExtensionPrefs* extension_prefs() { return extension_prefs_.get(); }

  // Whether the extension service is ready.
  bool is_ready() { return ready_; }

  // Note that this may return NULL if autoupdate is not turned on.
  ExtensionUpdater* updater() { return updater_.get(); }

  ExtensionToolbarModel* toolbar_model() { return &toolbar_model_; }

  ExtensionsQuotaService* quota_service() { return &quota_service_; }

  // Notify the frontend that there was an error loading an extension.
  // This method is public because ExtensionsServiceBackend can post to here.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error,
                                NotificationType type,
                                bool be_noisy);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  virtual ~ExtensionsService();
  friend class ChromeThread;
  friend class DeleteTask<ExtensionsService>;

  // Look up an extension by ID, optionally including either or both of enabled
  // and disabled extensions.
  Extension* GetExtensionByIdInternal(const std::string& id,
                                      bool include_enabled,
                                      bool include_disabled);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(Extension* extension);

  // Helper that updates the active extension list used for crash reporting.
  void UpdateActiveExtensionsInCrashReporter();

  // Helper method. Loads extension from prefs.
  void LoadInstalledExtension(const ExtensionInfo& info, bool relocalize);

  // The profile this ExtensionsService is part of.
  Profile* profile_;

  // Preferences for the owning profile.
  scoped_ptr<ExtensionPrefs> extension_prefs_;

  // The current list of installed extensions.
  ExtensionList extensions_;

  // The list of installed extensions that have been disabled.
  ExtensionList disabled_extensions_;

  // The full path to the directory where extensions are installed.
  FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_;

  // Whether to notify users when they attempt to install an extension.
  bool show_extensions_prompts_;

  // The backend that will do IO on behalf of this instance.
  scoped_refptr<ExtensionsServiceBackend> backend_;

  // Used by dispatchers to limit API quota for individual extensions.
  ExtensionsQuotaService quota_service_;

  // Is the service ready to go?
  bool ready_;

  // Our extension updater, if updates are turned on.
  scoped_refptr<ExtensionUpdater> updater_;

  // The model that tracks extensions with BrowserAction buttons.
  ExtensionToolbarModel toolbar_model_;

  // Map of inspector cookies that are detached, waiting for an extension to be
  // reloaded.
  typedef std::map<std::string, int> OrphanedDevTools;
  OrphanedDevTools orphaned_dev_tools_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsService);
};

// Implements IO for the ExtensionsService.
// TODO(aa): This can probably move into the .cc file.
class ExtensionsServiceBackend
    : public base::RefCountedThreadSafe<ExtensionsServiceBackend>,
      public ExternalExtensionProvider::Visitor {
 public:
  // |rdh| can be NULL in the case of test environment.
  // |extension_prefs| contains a dictionary value that points to the extension
  // preferences.
  explicit ExtensionsServiceBackend(const FilePath& install_directory);

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  // Errors are reported through ExtensionErrorReporter. On success,
  // OnExtensionLoaded() is called.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  void LoadSingleExtension(const FilePath &path,
                           scoped_refptr<ExtensionsService> frontend);

  // Check externally updated extensions for updates and install if necessary.
  // Errors are reported through ExtensionErrorReporter. Succcess is not
  // reported.
  void CheckForExternalUpdates(std::set<std::string> ids_to_ignore,
                               scoped_refptr<ExtensionsService> frontend);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, tell the frontend to uninstall it.
  void CheckExternalUninstall(scoped_refptr<ExtensionsService> frontend,
                              const std::string& id,
                              Extension::Location location);

  // Clear all ExternalExtensionProviders.
  void ClearProvidersForTesting();

  // Sets an ExternalExtensionProvider for the service to use during testing.
  // |location| specifies what type of provider should be added.
  void SetProviderForTesting(Extension::Location location,
                             ExternalExtensionProvider* test_provider);

  // ExternalExtensionProvider::Visitor implementation.
  virtual void OnExternalExtensionFound(const std::string& id,
                                        const Version* version,
                                        const FilePath& path,
                                        Extension::Location location);

  // When Chrome locale changes we need to re-localize manifest files for
  // extensions that are actually localized.
  void ReloadExtensionManifestsForLocaleChanged(
      ExtensionPrefs::ExtensionsInfo* extensions_to_reload,
      base::TimeTicks start_time,
      scoped_refptr<ExtensionsService> frontend);

 private:
  friend class base::RefCountedThreadSafe<ExtensionsServiceBackend>;

  virtual ~ExtensionsServiceBackend();

  // Finish installing the extension in |crx_path| after it has been unpacked to
  // |unpacked_path|.  If |expected_id| is not empty, it's verified against the
  // extension's manifest before installation. If |silent| is true, there will
  // be no install confirmation dialog. |from_gallery| indicates whether the
  // crx was installed from our gallery, which results in different UI.
  //
  // Note: We take ownership of |extension|.
  void OnExtensionUnpacked(
      const FilePath& crx_path,
      const FilePath& unpacked_path,
      Extension* extension,
      const std::string expected_id);

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error);

  // Notify the frontend that an extension was loaded.
  void ReportExtensionLoaded(Extension* extension);

  // Lookup an external extension by |id| by going through all registered
  // external extension providers until we find a provider that contains an
  // extension that matches. If |version| is not NULL, the extension version
  // will be returned (caller is responsible for deleting that pointer).
  // |location| can also be null, if not needed. Returns true if extension is
  // found, false otherwise.
  bool LookupExternalExtension(const std::string& id,
                               Version** version,
                               Extension::Location* location);

  // This is a naked pointer which is set by each entry point.
  // The entry point is responsible for ensuring lifetime.
  ExtensionsService* frontend_;

  // The top-level extensions directory being installed to.
  FilePath install_directory_;

  // Whether errors result in noisy alerts.
  bool alert_on_error_;

  // A map of all external extension providers.
  typedef std::map<Extension::Location,
                   linked_ptr<ExternalExtensionProvider> > ProviderMap;
  ProviderMap external_extension_providers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
