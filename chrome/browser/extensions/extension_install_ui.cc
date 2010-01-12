// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#if defined(TOOLKIT_VIEWS)  // TODO(port)
#include "chrome/browser/views/extensions/extension_installed_bubble.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/extension_installed_bubble_gtk.h"
#endif
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

#if defined(TOOLKIT_GTK)
#include "chrome/browser/extensions/gtk_theme_installed_infobar_delegate.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#endif

namespace {

static std::wstring GetInstallWarning(Extension* extension) {
  // If the extension has a plugin, it's easy: the plugin has the most severe
  // warning.
  if (!extension->plugins().empty())
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_NEW_FULL_ACCESS);

  // We also show the severe warning if the extension has access to any file://
  // URLs. They aren't *quite* as dangerous as full access to the system via
  // NPAPI, but pretty dang close. Content scripts are currently the only way
  // that extension can get access to file:// URLs.
  for (UserScriptList::const_iterator script =
           extension->content_scripts().begin();
       script != extension->content_scripts().end();
       ++script) {
    for (UserScript::PatternList::const_iterator pattern =
             script->url_patterns().begin();
         pattern != script->url_patterns().end();
         ++pattern) {
      if (pattern->scheme() == chrome::kFileScheme) {
        return l10n_util::GetString(
            IDS_EXTENSION_PROMPT_WARNING_NEW_FULL_ACCESS);
      }
    }
  }

  // Otherwise, we go in descending order of severity: all hosts, several hosts,
  // a single host, no hosts. For each of these, we also have a variation of the
  // message for when api permissions are also requested.
  if (extension->HasAccessToAllHosts()) {
    if (extension->api_permissions().empty())
      return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_NEW_ALL_HOSTS);
    else
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_NEW_ALL_HOSTS_AND_BROWSER);
  }

  const std::set<std::string> hosts = extension->GetEffectiveHostPermissions();
  if (hosts.size() > 1) {
    if (extension->api_permissions().empty())
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_NEW_MULTIPLE_HOSTS);
    else
      return l10n_util::GetString(
          IDS_EXTENSION_PROMPT_WARNING_NEW_MULTIPLE_HOSTS_AND_BROWSER);
  }

  if (hosts.size() == 1) {
    if (extension->api_permissions().empty())
      return l10n_util::GetStringF(
          IDS_EXTENSION_PROMPT_WARNING_NEW_SINGLE_HOST,
          UTF8ToWide(*hosts.begin()));
    else
      return l10n_util::GetStringF(
          IDS_EXTENSION_PROMPT_WARNING_NEW_SINGLE_HOST_AND_BROWSER,
          UTF8ToWide(*hosts.begin()));
  }

  DCHECK(hosts.size() == 0);
  if (extension->api_permissions().empty())
    return L"";
  else
    return l10n_util::GetString(IDS_EXTENSION_PROMPT_WARNING_NEW_BROWSER);
}

}  // namespace

ExtensionInstallUI::ExtensionInstallUI(Profile* profile)
    : profile_(profile), ui_loop_(MessageLoop::current())
#if defined(TOOLKIT_GTK)
    ,previous_use_gtk_theme_(false)
#endif
{}

// static
void ExtensionInstallUI::ShowExtensionInstallPrompt(
    Profile* profile, Delegate* delegate, Extension* extension, SkBitmap* icon,
    const string16& warning_text) {
  ShowExtensionInstallUIPromptImpl(profile, delegate, extension, icon,
                                   warning_text, false);  // uninstall == false.
}

// static
void ExtensionInstallUI::ShowExtensionUninstallPrompt(
    Profile* profile, Delegate* delegate, Extension* extension, SkBitmap* icon,
    const string16& warning_text) {
  ShowExtensionInstallUIPromptImpl(profile, delegate, extension, icon,
                                   warning_text, true);  // uninstall == true.
}

void ExtensionInstallUI::ConfirmInstall(Delegate* delegate,
                                        Extension* extension,
                                        SkBitmap* install_icon) {
  DCHECK(ui_loop_ == MessageLoop::current());

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->IsTheme()) {
    // Remember the current theme in case the user pressed undo.
    Extension* previous_theme = profile_->GetTheme();
    if (previous_theme)
      previous_theme_id_ = previous_theme->id();

#if defined(TOOLKIT_GTK)
    // On linux, we also need to take the user's system settings into account
    // to undo theme installation.
    previous_use_gtk_theme_ =
        GtkThemeProvider::GetFrom(profile_)->UseGtkTheme();
#endif

    delegate->InstallUIProceed();
    return;
  }

  if (!install_icon) {
    install_icon = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_EXTENSION_DEFAULT_ICON);
  }
  icon_ = *install_icon;

  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::EXTENSION_WILL_SHOW_CONFIRM_DIALOG,
                  Source<ExtensionInstallUI>(this),
                  NotificationService::NoDetails());

  ShowExtensionInstallPrompt(profile_, delegate, extension, &icon_,
                             WideToUTF16Hack(GetInstallWarning(extension)));
}

void ExtensionInstallUI::ConfirmUninstall(Delegate* delegate,
                                          Extension* extension,
                                          SkBitmap* icon) {
  DCHECK(ui_loop_ == MessageLoop::current());

  if (!icon) {
    icon = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_EXTENSION_DEFAULT_ICON);
  }

  string16 message =
      l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_CONFIRMATION);
  ShowExtensionUninstallPrompt(profile_, delegate, extension, icon, message);
}

void ExtensionInstallUI::OnInstallSuccess(Extension* extension) {
  if (extension->IsTheme()) {
    ShowThemeInfoBar(extension);
    return;
  }

  // GetLastActiveWithProfile will fail on the build bots. This needs to
  // implemented differently if any test is created which depends on
  // ExtensionInstalledBubble showing.
#if defined(TOOLKIT_VIEWS)
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  ExtensionInstalledBubble::Show(extension, browser, icon_);
#elif defined(TOOLKIT_GTK)
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  ExtensionInstalledBubbleGtk::Show(extension, browser, icon_);
#else
// TODO(port) crbug.com/26974 (mac)
#endif
}

void ExtensionInstallUI::OnInstallFailure(const std::string& error) {
  DCHECK(ui_loop_ == MessageLoop::current());

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  platform_util::SimpleErrorBox(
      browser ? browser->window()->GetNativeHandle() : NULL,
      l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_FAILURE_TITLE),
      UTF8ToUTF16(error));
}

void ExtensionInstallUI::OnOverinstallAttempted(Extension* extension) {
  ShowThemeInfoBar(extension);
}

void ExtensionInstallUI::ShowThemeInfoBar(Extension* new_theme) {
  if (!new_theme->IsTheme())
    return;

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  // First find any previous theme preview infobars.
  InfoBarDelegate* old_delegate = NULL;
  for (int i = 0; i < tab_contents->infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents->GetInfoBarDelegateAt(i);
    if (delegate->AsThemePreviewInfobarDelegate()) {
      old_delegate = delegate;
      break;
    }
  }

  // Then either replace that old one or add a new one.
  InfoBarDelegate* new_delegate = GetNewInfoBarDelegate(new_theme,
      tab_contents);

  if (old_delegate)
    tab_contents->ReplaceInfoBar(old_delegate, new_delegate);
  else
    tab_contents->AddInfoBar(new_delegate);
}

InfoBarDelegate* ExtensionInstallUI::GetNewInfoBarDelegate(
    Extension* new_theme, TabContents* tab_contents) {
#if defined(TOOLKIT_GTK)
  return new GtkThemeInstalledInfoBarDelegate(tab_contents, new_theme,
      previous_theme_id_, previous_use_gtk_theme_);
#else
  return new ThemeInstalledInfoBarDelegate(tab_contents, new_theme,
      previous_theme_id_);
#endif
}
