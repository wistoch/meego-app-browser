// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/gtk_util.h"
#endif

#if defined(OS_WIN)
#include "base/registry.h"
const HKEY kBackgroundModeRegistryRootKey = HKEY_CURRENT_USER;
const wchar_t* kBackgroundModeRegistrySubkey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kBackgroundModeRegistryKeyName = L"chromium";
#endif

BackgroundModeManager::BackgroundModeManager(Profile* profile,
                                             CommandLine* command_line)
    : profile_(profile),
      background_app_count_(0),
      in_background_mode_(false),
      status_tray_(NULL),
      status_icon_(NULL) {
  // If background mode is disabled, just exit - don't listen for
  // any notifications.
  if (!command_line->HasSwitch(switches::kEnableBackgroundMode))
    return;

  // If the -keep-alive-for-test flag is passed, then always keep chrome running
  // in the background until the user explicitly terminates it, by acting as if
  // we loaded a background app.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKeepAliveForTest))
    OnBackgroundAppLoaded();

  // When an extension is installed, make sure launch on startup is properly
  // set if appropriate. Likewise, turn off launch on startup when the last
  // background app is uninstalled.
  registrar_.Add(this, NotificationType::EXTENSION_INSTALLED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::EXTENSION_UNINSTALLED,
                 Source<Profile>(profile));
  // Listen for when extensions are loaded/unloaded so we can track the
  // number of background apps.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));

  // Check for the presence of background apps after all extensions have been
  // loaded, to handle the case where an extension has been manually removed
  // while Chrome was not running.
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile));

  // Listen for the application shutting down so we can decrement our KeepAlive
  // count.
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());

  // Listen for changes to the background mode preference.
  profile_->GetPrefs()->AddPrefObserver(prefs::kBackgroundModeEnabled, this);
}

BackgroundModeManager::~BackgroundModeManager() {
  // We're going away, so exit background mode (does nothing if we aren't in
  // background mode currently). This is primarily needed for unit tests,
  // because in an actual running system we'd get an APP_TERMINATING
  // notification before being destroyed.
  EndBackgroundMode();
  // Manually remove our pref observer so we don't get notified for prefs
  // changes (have to do it manually because we can't use the registrar for
  // prefs notifications).
  profile_->GetPrefs()->RemovePrefObserver(prefs::kBackgroundModeEnabled, this);
}

bool BackgroundModeManager::IsBackgroundModeEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kBackgroundModeEnabled);
}

bool BackgroundModeManager::IsLaunchOnStartupResetAllowed() {
  return profile_->GetPrefs()->GetBoolean(prefs::kLaunchOnStartupResetAllowed);
}

void BackgroundModeManager::SetLaunchOnStartupResetAllowed(bool allowed) {
  profile_->GetPrefs()->SetBoolean(prefs::kLaunchOnStartupResetAllowed,
                                   allowed);
}

void BackgroundModeManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
    // On a Mac, we use 'login items' mechanism which has user-facing UI so we
    // don't want to stomp on user choice every time we start and load
    // registered extensions.
#if !defined(OS_MACOSX)
      EnableLaunchOnStartup(IsBackgroundModeEnabled() &&
                            background_app_count_ > 0);
#endif
      break;
    case NotificationType::EXTENSION_LOADED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppLoaded();
      break;
    case NotificationType::EXTENSION_UNLOADED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppUnloaded();
      break;
    case NotificationType::EXTENSION_INSTALLED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppInstalled();
      break;
    case NotificationType::EXTENSION_UNINSTALLED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppUninstalled();
      break;
    case NotificationType::APP_TERMINATING:
      // Performing an explicit shutdown, so exit background mode (does nothing
      // if we aren't in background mode currently).
      EndBackgroundMode();
      // Shutting down, so don't listen for any more notifications so we don't
      // try to re-enter/exit background mode again.
      registrar_.RemoveAll();
      break;
    case NotificationType::PREF_CHANGED:
      DCHECK(0 == Details<std::string>(details).ptr()->compare(
          prefs::kBackgroundModeEnabled));
      OnBackgroundModePrefChanged();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool BackgroundModeManager::IsBackgroundApp(Extension* extension) {
  return extension->HasApiPermission(Extension::kBackgroundPermission);
}


void BackgroundModeManager::OnBackgroundModePrefChanged() {
  // Background mode has been enabled/disabled in preferences, so update our
  // state accordingly.
  if (IsBackgroundModeEnabled() && !in_background_mode_ &&
      background_app_count_ > 0) {
    // We should be in background mode, but we're not, so switch to background
    // mode.
    EnableLaunchOnStartup(true);
    StartBackgroundMode();
  }
  if (!IsBackgroundModeEnabled() && in_background_mode_) {
    // We're in background mode, but we shouldn't be any longer.
    EnableLaunchOnStartup(false);
    EndBackgroundMode();
  }
}

void BackgroundModeManager::OnBackgroundAppLoaded() {
  // When a background app loads, increment our count and also enable
  // KeepAlive mode if the preference is set.
  background_app_count_++;
  if (background_app_count_ == 1 && IsBackgroundModeEnabled())
    StartBackgroundMode();
}

void BackgroundModeManager::StartBackgroundMode() {
  // Don't bother putting ourselves in background mode if we're already there.
  if (in_background_mode_)
    return;

  // Mark ourselves as running in background mode.
  in_background_mode_ = true;

  // Put ourselves in KeepAlive mode and create a status tray icon.
  BrowserList::StartKeepAlive();

  // Display a status icon to exit Chrome.
  CreateStatusTrayIcon();
}

void BackgroundModeManager::OnBackgroundAppUnloaded() {
  // When a background app unloads, decrement our count and also end
  // KeepAlive mode if appropriate.
  background_app_count_--;
  DCHECK(background_app_count_ >= 0);
  if (background_app_count_ == 0 && IsBackgroundModeEnabled())
    EndBackgroundMode();
}

void BackgroundModeManager::EndBackgroundMode() {
  if (!in_background_mode_)
    return;
  in_background_mode_ = false;

  // End KeepAlive mode and blow away our status tray icon.
  BrowserList::EndKeepAlive();
  RemoveStatusTrayIcon();
}

void BackgroundModeManager::OnBackgroundAppInstalled() {
  // We're installing a background app. If this is the first background app
  // being installed, make sure we are set to launch on startup.
  if (IsBackgroundModeEnabled() && background_app_count_ == 0)
    EnableLaunchOnStartup(true);
}

void BackgroundModeManager::OnBackgroundAppUninstalled() {
  // When uninstalling a background app, disable launch on startup if it's the
  // last one.
  if (IsBackgroundModeEnabled() && background_app_count_ == 1)
    EnableLaunchOnStartup(false);
}

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // TODO(BUG43382): Add code for other platforms to enable/disable launch on
  // startup.
  // This functionality is only defined for default profile, currently.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;

#if defined(OS_MACOSX)
  if (should_launch) {
    // Return if Chrome is already a Login Item (avoid overriding user choice).
    if (mac_util::CheckLoginItemStatus(NULL))
      return;

    mac_util::AddToLoginItems(true);  // Hide on startup.

    // Remember we set Login Item, not the user - so we can reset it later.
    SetLaunchOnStartupResetAllowed(true);
  } else {
    // If we didn't set Login Item, don't mess with it.
    if (!IsLaunchOnStartupResetAllowed())
      return;
    SetLaunchOnStartupResetAllowed(false);

    // Check if Chrome is not a login Item, or is a Login Item but w/o 'hidden'
    // flag - most likely user has modified the setting, don't override it.
    bool is_hidden = false;
    if (!mac_util::CheckLoginItemStatus(&is_hidden) || !is_hidden)
      return;

    mac_util::RemoveFromLoginItems();
  }
#elif defined(OS_WIN)
  // TODO(BUG53597): Make RegKey mockable by adding virtual destructor and
  //     factory method.
  // TODO(BUG53600): Use distinct registry keys per flavor of chromium and
  //     profile.
  const wchar_t* key_name = kBackgroundModeRegistryKeyName;
  RegKey read_key(kBackgroundModeRegistryRootKey,
                  kBackgroundModeRegistrySubkey, KEY_READ);
  RegKey write_key(kBackgroundModeRegistryRootKey,
                   kBackgroundModeRegistrySubkey, KEY_WRITE);
  if (should_launch) {
    FilePath executable;
    if (!PathService::Get(base::FILE_EXE, &executable))
      return;
    if (read_key.ValueExists(key_name)) {
      std::wstring current_value;
      if (read_key.ReadValue(key_name, &current_value) &&
          (current_value == executable.value()))
        return;
    }
    if (!write_key.WriteValue(key_name, executable.value().c_str()))
      LOG(WARNING) << "Failed to register launch on login.";
  } else {
    if (read_key.ValueExists(key_name) && !write_key.DeleteValue(key_name))
      LOG(WARNING) << "Failed to deregister launch on login.";
  }
#endif
}

void BackgroundModeManager::CreateStatusTrayIcon() {
  // Only need status icons on windows/linux. ChromeOS doesn't allow exiting
  // Chrome and Mac can use the dock icon instead.
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  if (!status_tray_)
    status_tray_ = profile_->GetStatusTray();
#endif

  // If the platform doesn't support status icons, or we've already created
  // our status icon, just return.
  if (!status_tray_ || status_icon_)
    return;
  status_icon_ = status_tray_->CreateStatusIcon();
  if (!status_icon_)
    return;

  // Set the image and add ourselves as a click observer on it
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  status_icon_->SetImage(*bitmap);
  status_icon_->SetToolTip(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  // Create a context menu item for Chrome.
  menus::SimpleMenuModel* menu = new menus::SimpleMenuModel(this);
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringFUTF16(IDS_ABOUT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));

#if defined(TOOLKIT_GTK)
  string16 preferences = gtk_util::GetStockPreferencesMenuLabel();
  if (preferences.empty())
    menu->AddItemWithStringId(IDC_OPTIONS, IDS_OPTIONS);
  else
    menu->AddItem(IDC_OPTIONS, preferences);
#else
  menu->AddItemWithStringId(IDC_OPTIONS, IDS_OPTIONS);
#endif
  menu->AddSeparator();
  menu->AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  status_icon_->SetContextMenu(menu);
}

bool BackgroundModeManager::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BackgroundModeManager::IsCommandIdEnabled(int command_id) const {
  // For now, we do not support disabled items.
  return true;
}

bool BackgroundModeManager::GetAcceleratorForCommandId(
    int command_id,
    menus::Accelerator* accelerator) {
  // No accelerators for status icon context menus.
  return false;
}

void BackgroundModeManager::RemoveStatusTrayIcon() {
  if (status_icon_)
    status_tray_->RemoveStatusIcon(status_icon_);
  status_icon_ = NULL;
}


void BackgroundModeManager::ExecuteCommand(int item) {
  switch (item) {
    case IDC_EXIT:
      UserMetrics::RecordAction(UserMetricsAction("Exit"), profile_);
      BrowserList::CloseAllBrowsersAndExit();
      break;
    case IDC_ABOUT:
      GetBrowserWindow()->OpenAboutChromeDialog();
      break;
    case IDC_OPTIONS:
      GetBrowserWindow()->OpenOptionsDialog();
      break;
    default:
      NOTREACHED();
      break;
  }
}

Browser* BackgroundModeManager::GetBrowserWindow() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser) {
    Browser::OpenEmptyWindow(profile_);
    browser = BrowserList::GetLastActive();
  }
  return browser;
}

// static
void BackgroundModeManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBackgroundModeEnabled, true);
  prefs->RegisterBooleanPref(prefs::kLaunchOnStartupResetAllowed, false);
}
